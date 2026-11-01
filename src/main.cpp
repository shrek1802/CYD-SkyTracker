#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include "sd_update.h"
#include "board_profile.h"
#include "map_view.h"
#include "touch_pad.h"
#include "flights.h"
#include "splash.h"
#include "photo_view.h"
#include "ui_style.h"
#include "detail_layout.h"
#include "colour_diagnostic.h"
#include "uk_postcode.h"
#include "postcode_lookup.h"
#include "radar_popup.h"
#include "aircraft_type_photo.h"
#include <cmath>
#include <cstdlib>

namespace {
constexpr int SD_CS = BoardProfile::sdCs();
constexpr int SD_SCK = BoardProfile::sdClk();
constexpr int SD_MISO = BoardProfile::sdMiso();
constexpr int SD_MOSI = BoardProfile::sdMosi();
constexpr int TFT_BL_PIN = BoardProfile::backlightPin();
Preferences preferences;
TFT_eSPI display;
// Screen uses HSPI on the physically tested dual-USB ILI9341 board.
// The SD card MUST use the other controller: calling SD.begin() on HSPI
// would reassign the TFT's SCK/MOSI pins, producing a white screen.
#if defined(USE_HSPI_PORT)
SPIClass sdSpi(VSPI);
#else
SPIClass sdSpi(HSPI);
#endif
MapView mapView;
TouchPad touch;
FlightTracker tracker;
PhotoView photo;
bool detailShowing = false;
bool photoFullscreen = false;
bool moreInfoShowing = false;
uint32_t splashStartMs = 0;
constexpr uint32_t kMinSplashMs = 2500;
void finishSplash() {
    const uint32_t shown = millis() - splashStartMs;
    if (shown < kMinSplashMs) delay(kMinSplashMs - shown);
}
uint32_t lastFlightFetch = 0;
uint32_t lastMotionFrame = 0;
constexpr uint32_t kMotionFrameMs = 2000; // redraw only moved aircraft
uint32_t lastWiFiRetry = 0;
bool mapShowing = false;
bool touchingLastFrame = false;
int touchStartX = 0;
int touchStartY = 0;
int touchEndX = 0;
int touchEndY = 0;
WebServer web(80);
DNSServer dns;
bool portalActive = false;
bool sdReady = false;
String apSsid;
String apPassword;
String lastCommand;
String setupError;
bool previewShowing = false;
bool followSelected = false;
bool setupTouchHeld = false;
bool portalRoutesReady = false;
// Keep the active network and two previous locations; changing location
// must not erase the home's Wi-Fi credentials.
const char* const kWifiNames[]={"ssid","ssid2","ssid3"};
const char* const kWifiPasswords[]={"password","pass2","pass3"};
String wifiPasswordFor(const String& ssid) {
    for(int i=0;i<3;++i)
        if(preferences.getString(kWifiNames[i],"")==ssid)
            return preferences.getString(kWifiPasswords[i],"");
    return "";
}
void rememberWifi(const String& ssid,const String& password) {
    String names[3],passwords[3];
    names[0]=ssid; passwords[0]=password;
    int next=1;
    for(int i=0;i<3 && next<3;++i) {
        const String existing=preferences.getString(kWifiNames[i],"");
        if(existing.isEmpty()||existing==ssid)continue;
        names[next]=existing;
        passwords[next]=preferences.getString(kWifiPasswords[i],"");
        ++next;
    }
    for(int i=0;i<3;++i) {
        preferences.putString(kWifiNames[i],names[i]);
        preferences.putString(kWifiPasswords[i],passwords[i]);
    }
}

void drawAircraftPopup() {
    if(!mapShowing || detailShowing || !previewShowing)return;
    const AircraftRecord* a=tracker.selectedRecord();
    if(!a)return;
    using namespace RadarPopup;
    display.fillRoundRect(card.x,card.y,card.w,card.h,5,SkyUI::kPanel);
    display.drawRoundRect(card.x,card.y,card.w,card.h,5,SkyUI::kAccent);
    display.setTextSize(1);
    display.setTextColor(SkyUI::kAccent,SkyUI::kPanel);
    display.setCursor(card.x+7,card.y+8);
    display.print(String(a->flight[0]?a->flight:a->hex).substring(0,13));
    display.setTextColor(SkyUI::kWhite,SkyUI::kPanel);
    display.setCursor(card.x+7,card.y+23);
    const auto* model=AircraftTypePhoto::find(a->type);
    const String modelName=model?String(model->model):
        a->type[0]?String(a->type):String("Unknown type");
    display.print(modelName.substring(0,25));
    display.setTextColor(SkyUI::kMuted,SkyUI::kPanel);
    display.setCursor(card.x+7,card.y+39);
    if(a->altitudeFt>=0)display.printf("%d ft",a->altitudeFt);
    else display.print("-- ft");
    display.setCursor(card.x+7,card.y+54);
    if(a->speedKt>=0)display.printf("%.0f kt",double(a->speedKt));
    else display.print("-- kt");
    SkyUI::button(display,details.x,details.y,details.w,details.h,"DETAIL",true);
    SkyUI::button(display,follow.x,follow.y,follow.w,follow.h,
                  followSelected?"UNFOLLOW":"FOLLOW",followSelected);
    display.setTextColor(SkyUI::kGold,SkyUI::kPanel);
    display.setCursor(dismiss.x+4,dismiss.y+2);
    display.print("x");
}
void drawRadar() {
    if (!mapShowing) return;
    // A full map paint is needed only after user-driven pan/zoom or returning
    // from another page. NOT after a regular 30-second aircraft update.
    tracker.invalidatePaint();
    mapView.draw(display, sdReady, WiFi.status() == WL_CONNECTED);
    tracker.drawMarkers(display, mapView);
    drawAircraftPopup();
    mapView.drawFlightStatus(display, tracker.count(), tracker.status(),
                             tracker.lastGoodPollMs() ?
                             int((millis()-tracker.lastGoodPollMs())/1000) : -1,
                             tracker.selectedRecord() ?
                             (tracker.selectedRecord()->flight[0] ?
                              tracker.selectedRecord()->flight :
                              tracker.selectedRecord()->hex) : nullptr);
}

void drawFlightScreen() {
    if (!tracker.hasSelection()) return;
    photoFullscreen = false;
    moreInfoShowing = false;
    tracker.drawDetails(display);
    photo.drawHero(display);
    photo.drawCreditAt(display, DetailLayout::kCreditY);
}
void drawMoreScreen() {
    if(!tracker.hasSelection())return;
    photoFullscreen=false;
    moreInfoShowing=true;
    tracker.drawExtraDetails(display);
}
void drawPhotoScreen() {
    if (!tracker.hasSelection()) return;
    photoFullscreen = true;
    display.fillScreen(SkyUI::kBackground);
    display.fillRect(0,0,display.width(),27,SkyUI::kPanel);
    display.fillRect(0,26,display.width(),2,SkyUI::kAccent);
    display.setTextSize(1);
    display.setTextColor(SkyUI::kGold,SkyUI::kPanel);
    display.setCursor(8,9);
    display.print("PHOTO");
    const AircraftRecord* a=tracker.selectedRecord();
    display.setTextColor(SkyUI::kWhite,SkyUI::kPanel);
    display.setCursor(51,9);
    if(a)display.print(String(a->flight[0]?a->flight:a->type).substring(0,17));
    SkyUI::button(display,display.width()-64,2,57,23,"MAP");
    photo.drawFullscreen(display);
    photo.drawCreditAt(display,199);
    SkyUI::button(display,8,214,146,24,"< DETAILS");
    SkyUI::button(display,164,214,display.width()-172,24,"BACK TO MAP",true);
}

void statusScreen(const String& heading, const String& line1,
                  const String& line2 = "", const String& line3 = "") {
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.setTextSize(2);
    display.setCursor(8, 10);
    display.println("CYD SKYTRACKER");
    display.drawFastHLine(8, 37, display.width() - 16, TFT_DARKGREY);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextSize(2);
    display.setCursor(8, 53);
    display.println(heading);
    display.setTextSize(1);
    display.setCursor(8, 100);
    display.println(line1);
    display.setCursor(8, 124);
    display.println(line2);
    display.setCursor(8, 148);
    display.println(line3);
    display.setCursor(8, display.height() - 26);
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.println("DEVELOPMENT BUILD - NO RADAR YET");
    display.setCursor(8, display.height() - 12);
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.println(BoardProfile::id());
}

String htmlEscape(String value) {
    value.replace("&","&amp;");
    value.replace("<","&lt;");
    value.replace(">","&gt;");
    value.replace("\"","&quot;");
    value.replace("'","&#39;");
    return value;
}
String configPage() {
    String page=R"HTML(<!doctype html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SkyTracker - Set location</title>
<style>
body{font:16px system-ui;background:#101b2c;color:#eff7ff;max-width:510px;
margin:1rem auto;padding:1rem}
h1{color:#72ddf8}p{line-height:1.45}label{display:block;margin-top:1rem}
input,button{box-sizing:border-box;width:100%;padding:.85rem;border-radius:8px;
font:inherit}input{border:1px solid #698097;background:#203650;color:#fff}
button{margin-top:1.5rem;background:#1684c7;color:#fff;border:0;font-weight:700}
small{color:#b8cade}details{border-top:1px solid #486076;margin-top:1.5rem;padding-top:.8rem}
summary{cursor:pointer;color:#72ddf8}.error{background:#512c34;padding:12px;border-radius:8px}
</style></head><body>
<h1>SKYTRACKER</h1>
<p>Set your home radar location using a <b>UK postcode</b>. No GPS coordinates needed.</p>
{{ERROR}}
<form method="post" action="/save">
<label>Home Wi-Fi name
<input name="ssid" maxlength="32" required value="{{SSID}}" autocomplete="off"></label>
<label>Wi-Fi password
<input name="password" type="password" maxlength="63"
placeholder="Leave blank to keep saved password"></label>
<label>Full UK postcode (recommended)
<input name="postcode" value="{{POSTCODE}}" maxlength="9"
placeholder="e.g. NG12 3AB" autocapitalize="characters"></label>
<p><small>Postcode is resolved after the device joins Wi-Fi; it uses the postcode's
approximate centre, not your exact property. One lookup needs internet;
saved coordinates work after that without another lookup.</small></p>
<details><summary>Manual latitude / longitude (outside UK or fallback)</summary>
<p><small>Clear the postcode above if you wish to use manual coordinates.</small></p>
<label>Latitude
<input name="lat" type="number" step="any" min="-90" max="90" value="{{LAT}}"></label>
<label>Longitude
<input name="lon" type="number" step="any" min="-180" max="180" value="{{LON}}"></label>
</details>
<button type="submit">Save and start radar</button></form>
<p><small>This device remembers up to three Wi-Fi networks, so you can take
it to family and return home without losing your original password.
Update the postcode whenever you move the radar.</small></p>
<p><small>If your postcode cannot be found, check Wi-Fi and try again,
or enter coordinates manually. Maps and aircraft photos on the SD stay unchanged.</small></p>
</body></html>)HTML";
    page.replace("{{SSID}}",htmlEscape(preferences.getString("ssid","")));
    page.replace("{{POSTCODE}}",htmlEscape(preferences.getString("loc_mode","")=="postcode"
                    ?preferences.getString("postcode",""):String("")));
    const bool haveLegacy=(preferences.getFloat("lat",0)!=0.0f ||
                           preferences.getFloat("lon",0)!=0.0f);
    page.replace("{{LAT}}",haveLegacy?String(preferences.getFloat("lat",0),6):String(""));
    page.replace("{{LON}}",haveLegacy?String(preferences.getFloat("lon",0),6):String(""));
    page.replace("{{ERROR}}",setupError.isEmpty()?String(""):
                 String("<p class=\"error\">")+htmlEscape(setupError)+"</p>");
    return page;
}
bool parseCoordinate(const String& text,double minimum,double maximum,double& out) {
    if(text.length()==0)return false;
    char* end=nullptr;
    const double result=std::strtod(text.c_str(),&end);
    if(end==text.c_str()||!std::isfinite(result)||result<minimum||result>maximum)
        return false;
    while(*end==' '||*end=='\t')++end;
    if(*end!='\0')return false;
    out=result;
    return true;
}
void handleSave() {
    String ssid=web.arg("ssid");
    ssid.trim();
    const String password=web.arg("password");
    String rawPostcode=web.arg("postcode");
    rawPostcode.trim();
    const std::string canonical=UkPostcode::format(std::string(rawPostcode.c_str()));
    double latitude=0,longitude=0;
    const bool postcodeEntered=rawPostcode.length()>0;
    if(ssid.isEmpty() || ssid.length()>32 || password.length()>63) {
        web.send(400,"text/plain","Invalid Wi-Fi name or password");
        return;
    }
    if(postcodeEntered && canonical.empty()) {
        web.send(400,"text/plain","Enter a complete UK postcode, e.g. NG12 3AB");
        return;
    }
    if(!postcodeEntered &&
       (!parseCoordinate(web.arg("lat"),-90,90,latitude) ||
        !parseCoordinate(web.arg("lon"),-180,180,longitude))) {
        web.send(400,"text/plain","Enter a full UK postcode or valid manual coordinates");
        return;
    }
    const String previousSsid=preferences.getString("ssid","");
    const String resolvedPassword=password.isEmpty()?wifiPasswordFor(ssid):password;
    if(resolvedPassword.isEmpty() && ssid!=previousSsid) {
        web.send(400,"text/plain","Enter the password for the new Wi-Fi network");
        return;
    }
    // Remember old places and try the newly selected network first next boot.
    rememberWifi(ssid,resolvedPassword);
    if(postcodeEntered) {
        const String target=String(canonical.c_str());
        const bool reused=preferences.getString("loc_mode","")=="postcode" &&
                          preferences.getString("postcode","")==target &&
                          preferences.getBool("loc_ready",false);
        preferences.putString("loc_mode","postcode");
        preferences.putString("postcode",target);
        // A new postcode invalidates the previous geographic fix. The new
        // postcode is never silently mapped to old coordinates.
        preferences.putBool("loc_ready",reused);
    } else {
        preferences.putString("loc_mode","manual");
        preferences.putString("postcode","");
        preferences.putFloat("lat",float(latitude));
        preferences.putFloat("lon",float(longitude));
        preferences.putBool("loc_ready",true);
    }
    web.send(200,"text/html",
        "<h2>Saved</h2><p>SkyTracker is restarting. "
        "If a postcode was entered, it will resolve it using home Wi-Fi.</p>");
    delay(1000);
    ESP.restart();
}

void startWifi();
void startSetupPortal() {
    if(portalActive)return;
    mapShowing=false;
    detailShowing=false;
    photoFullscreen=false;
    moreInfoShowing=false;
    previewShowing=false;
    followSelected=false;
    touchingLastFrame=false;
    tracker.deselect();
    photo.clear();
    WiFi.disconnect(false);
    WiFi.mode(WIFI_AP);
    const uint32_t suffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFU);
    apSsid = String("SkyTracker-") + String(suffix, HEX);
    // Per-session random WPA2 password; unlike a MAC-derived password, it
    // cannot be calculated by someone standing near the device.
    const uint32_t secretA=esp_random(),secretB=esp_random();
    char password[21];
    snprintf(password,sizeof(password),"ST%08lX%08lX",
             (unsigned long)secretA,(unsigned long)secretB);
    apPassword=password;
    if (!WiFi.softAP(apSsid.c_str(), apPassword.c_str())) {
        statusScreen("Setup failed", "Restart the device");
        return;
    }
    dns.start(53, "*", WiFi.softAPIP());
    if(!portalRoutesReady) {
        web.on("/", HTTP_GET, []() { web.send(200, "text/html", configPage()); });
        web.on("/save", HTTP_POST, handleSave);
        web.onNotFound([]() {
            web.sendHeader("Location", "http://192.168.4.1/", true);
            web.send(302, "text/plain", "");
        });
        portalRoutesReady=true;
    }
    web.begin();
    portalActive = true;
    finishSplash();
    statusScreen("SETUP / TRAVEL", "Network: " + apSsid,
                 "Password: " + apPassword, "Open: 192.168.4.1");
    display.setTextSize(1);
    display.setTextColor(TFT_CYAN,TFT_BLACK);
    display.setCursor(9,177);
    display.print("Change Wi-Fi and postcode on phone");
    if(touch.calibrated())
        SkyUI::button(display,95,208,130,27,"RETRY WIFI");
}
void leaveSetupPortal() {
    if(!portalActive)return;
    portalActive=false;
    dns.stop();
    web.stop();
    WiFi.softAPdisconnect(true);
    touchingLastFrame=false;
    // Reconnect using the remembered networks without clearing location.
    startWifi();
}

void startWifi() {
    const String savedSsid = preferences.getString("ssid", "");
    if(savedSsid.isEmpty() && preferences.getString("ssid2","").isEmpty() &&
       preferences.getString("ssid3","").isEmpty()) {
        startSetupPortal();
        return;
    }
    WiFi.mode(WIFI_STA);
    Splash::status(display, "Connecting to saved Wi-Fi...");
    // Most recently configured first. If none is in range, phone setup
    // activates automatically without needing a computer or flash.
    for(int i=0;i<3 && WiFi.status()!=WL_CONNECTED;++i) {
        const String ssid=preferences.getString(kWifiNames[i],"");
        if(ssid.isEmpty())continue;
        const String password=preferences.getString(kWifiPasswords[i],"");
        Serial.printf("[WIFI] Trying stored profile %d: %s\n",i+1,ssid.c_str());
        WiFi.disconnect(false,false);
        WiFi.begin(ssid.c_str(),password.c_str());
        const uint32_t started=millis();
        while(WiFi.status()!=WL_CONNECTED && millis()-started<6500UL)
            delay(150);
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] Connected, local IP %s\n", WiFi.localIP().toString().c_str());
        finishSplash();
        const String mode=preferences.getString("loc_mode","manual");
        bool locationReady=preferences.getBool("loc_ready",false);
        float lat=preferences.getFloat("lat",0.0f);
        float lon=preferences.getFloat("lon",0.0f);
        // Migrate pre-postcode firmware without discarding valid stored fixes.
        if(!locationReady && mode!="postcode" &&
           std::isfinite(lat) && std::isfinite(lon) &&
           lat>=-90 && lat<=90 && lon>=-180 && lon<=180 &&
           (lat!=0 || lon!=0)) {
            locationReady=true;
            preferences.putBool("loc_ready",true);
        }
        if(mode=="postcode" && !locationReady) {
            Splash::status(display,"Finding UK postcode...");
            PostcodeLookup::Location resolved;
            String reason;
            if(!PostcodeLookup::resolve(preferences.getString("postcode",""),
                                         resolved,reason)) {
                setupError=String("Postcode lookup failed: ")+reason+
                    ". Check your connection or use manual coordinates.";
                Serial.printf("[LOCATION] %s\n",reason.c_str());
                startSetupPortal();
                return;
            }
            lat=resolved.latitude;lon=resolved.longitude;
            preferences.putFloat("lat",lat);
            preferences.putFloat("lon",lon);
            preferences.putString("postcode",resolved.postcode);
            preferences.putBool("loc_ready",true);
            locationReady=true;
            Serial.printf("[LOCATION] %s -> %.6f, %.6f\n",
                          resolved.postcode.c_str(),double(lat),double(lon));
        }
        if(!locationReady || !std::isfinite(lat) || !std::isfinite(lon) ||
           lat < -90 || lat > 90 || lon < -180 || lon > 180) {
            setupError="Enter a UK postcode or manual coordinates to begin.";
            startSetupPortal();
            return;
        }
        setupError="";
        mapView.begin(lat, lon, 9);
        tracker.begin(lat, lon, 60);
        mapShowing = true;
        Serial.println("[BOOT] Preparing first radar display");
        if (!touch.calibrated()) {
            statusScreen("Touch setup", "Tap the 3 crosses", "Touch calibration starting...");
            delay(600);
            if (!touch.calibrate(display, preferences)) {
                Serial.println("Touch setup timed out: use USB serial CAL_TOUCH to retry");
            }
        }
        drawRadar();
    } else {
        startSetupPortal();
    }
}
}

void setup() {
    Serial.begin(115200);
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);
    Serial.printf("[BOOT] Hardware profile: %s\n", BoardProfile::id());
    Serial.println("[BOOT] Initialising TFT...");
    display.init();
    display.setRotation(1);
    Serial.println("[BOOT] TFT initialised");
    Splash::draw(display, "Initialising microSD...");
    splashStartMs = millis();
    preferences.begin("skytracker", false);
    Serial.println("[BOOT] Mounting SD on isolated SPI bus...");
    sdSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    sdReady = SD.begin(SD_CS, sdSpi, 4000000);
    Serial.printf("[BOOT] SD mounted: %s\n", sdReady ? "YES" : "NO");
    // The branded screen must remain visible after touching the SD bus.
    // If it becomes blank here, inspect display vs SD SPI ownership.
    Splash::status(display, sdReady ? "SD mounted" : "No SD - grid available");
    Splash::status(display, sdReady ? "Reading SD card..." : "No SD card - using grid");
    Serial.println("[BOOT] Initialising touch...");
    touch.begin(preferences);
    Serial.println("[BOOT] Starting Wi-Fi...");
    startWifi();
    Serial.printf("CYD SkyTracker development build - hardware: %s\n", BoardProfile::id());
    Serial.println("For physical USB-serial test only: type INSTALL_SD to install");
    Serial.println("/updates/firmware.bin with matching /updates/sha256.txt.");
    Serial.println("Type CAL_TOUCH to calibrate the resistive touch screen.");
    Serial.println("Type PHOTO_TEST to show /aircraft/B738.rgb565 without Wi-Fi or ADS-B.");
    Serial.println("Type COLOUR_TEST to compare TFT and SD RGB565 colour patches.");
    Serial.println("Type VERSION to verify that PHOTO_TEST-capable firmware is installed.");
    Serial.println("Tap SKYTRACKER at top left to open Wi-Fi setup hotspot.");
}

void loop() {
    // Handle serial diagnostics before any network polling or map repaint.
    // Prevent a busy HTTPS/SD renderer delaying PHOTO_TEST acknowledgement.
    while (Serial.available()) {
        char ch = static_cast<char>(Serial.read());
        if (ch == '\r') continue;
        if (ch == '\n') {
            lastCommand.trim();
            if (lastCommand == "VERSION" || lastCommand == "HELP") {
                Serial.printf("[CMD] SkyTracker diagnostic: %s | board: %s | PHOTO_TEST=YES\n",
                              lastCommand.c_str(), BoardProfile::id());
                Serial.printf("[CMD] SD mounted=%s | free heap=%u\n",
                              sdReady ? "YES" : "NO", unsigned(ESP.getFreeHeap()));
            }
            if (lastCommand == "CAL_TOUCH") {
                if (touch.calibrate(display, preferences)) {
                    Serial.println("Touch calibration saved.");
                } else {
                    Serial.println("Touch calibration cancelled or invalid.");
                }
                if (mapShowing) { detailShowing = false; photoFullscreen = false; drawRadar(); }
                else statusScreen("Touch calibration", touch.calibrated() ? "Calibration saved" : "Calibration failed");
                touchingLastFrame = false;
            }
            if (lastCommand == "PHOTO_TEST") {
                Serial.println("[CMD] Received PHOTO_TEST; starting SD-only image test.");
                // Deterministic test independent of live aircraft and HTTPS.
                // Shows whether this CYD reads the SD asset and can paint it.
                Serial.printf("[PHOTO_TEST] start: SD mounted=%s freeheap=%u\n",
                              sdReady ? "YES" : "NO", unsigned(ESP.getFreeHeap()));
                photo.load(nullptr, "B738", sdReady, true);
                Serial.printf("[PHOTO_TEST] load result: %s\n",photo.status().c_str());
                display.fillScreen(TFT_BLACK);
                display.setTextSize(1);
                display.setTextColor(TFT_CYAN,TFT_BLACK);
                display.setCursor(7,8);
                display.print("AIRCRAFT PHOTO SD TEST");
                photo.draw(display);
                photo.drawCredit(display);
                Serial.println("[PHOTO_TEST] screen shown for 8 seconds");
                delay(8000);
                photo.clear();
                if(mapShowing) {
                    detailShowing = false;
                    photoFullscreen = false;
                    drawRadar();
                }
            }
            if (lastCommand == "COLOUR_TEST") {
                Serial.println("[COLOUR_TEST] Compare upper TFT swatches with lower SD RGB565 swatches.");
                const bool ok=SkyColourDiagnostic::draw(display,sdReady);
                Serial.printf("[COLOUR_TEST] SD RGB565: %s\\n",ok?"PASS":"MISSING/ERROR");
                delay(10000);
                if (mapShowing) {detailShowing=false;photoFullscreen=false;drawRadar();}
            }
            if (lastCommand == "INSTALL_SD") {
                if (!sdReady) {
                    Serial.println("SD card unavailable; update aborted");
                } else {
                    statusScreen("Testing SD update", "Verifying firmware");
                    String error;
                    if (SdUpdate::installFromCard("/updates/firmware.bin",
                                                  "/updates/sha256.txt", error)) {
                        statusScreen("Update successful", "Rebooting");
                        delay(700);
                        ESP.restart();
                    } else {
                        Serial.println(String("SD update rejected: ") + error);
                        statusScreen("Update rejected", error);
                    }
                }
            }
            if (lastCommand.length() && lastCommand != "VERSION" &&
                lastCommand != "HELP" && lastCommand != "PHOTO_TEST" &&
                lastCommand != "CAL_TOUCH" && lastCommand != "COLOUR_TEST" &&
                lastCommand != "INSTALL_SD") {
                Serial.printf("[CMD] Unknown command: %s\n", lastCommand.c_str());
            }
            lastCommand = "";
        } else if (lastCommand.length() < 40) {
            lastCommand += ch;
        }
    }
    // The AP setup page remains recoverable without a computer. Once
    // calibrated, the bottom button retries saved Wi-Fi without clearing it.
    if(portalActive && touch.calibrated()) {
        int sx=0,sy=0;
        const bool pressed=touch.sample(display.width(),display.height(),sx,sy);
        if(pressed) {
            setupTouchHeld=true;
            touchEndX=sx;touchEndY=sy;
        } else if(setupTouchHeld) {
            setupTouchHeld=false;
            if(touchEndY>=204)leaveSetupPortal();
        }
    } else if (mapShowing && touch.calibrated()) {
        int x = 0, y = 0;
        const bool pressed = touch.sample(display.width(), display.height(), x, y);
        if (pressed) {
            touchEndX = x; touchEndY = y;
            if (!touchingLastFrame) { touchStartX = x; touchStartY = y; }
        } else if (touchingLastFrame) {
            const int dx = touchEndX - touchStartX;
            const int dy = touchEndY - touchStartY;
            if (detailShowing) {
                if (photoFullscreen) {
                    // Fullscreen photo has explicit navigation: details left,
                    // map right. Tapping the photograph does not lose selection.
                    if ((touchEndY >= 210 && touchEndX >= 158) ||
                        (touchEndY < 28 && touchEndX > display.width()-72)) {
                        detailShowing = false;
                        photoFullscreen = false;
                        photo.clear();
                        drawRadar();
                    } else if ((touchEndY >= 210 && touchEndX < 158) ||
                               (touchEndY < 28 && touchEndX < 100)) {
                        drawFlightScreen();
                    }
                } else if(moreInfoShowing) {
                    if(touchEndY>=210 && touchEndX<158) {
                        drawFlightScreen();
                    } else if(touchEndY>=210 && touchEndX>=158) {
                        detailShowing=false;moreInfoShowing=false;
                        photo.clear();drawRadar();
                    }
                } else {
                    const bool showPhoto=DetailLayout::kHero.hit(touchEndX,touchEndY);
                    const bool showMore=DetailLayout::kPhotoButton.hit(touchEndX,touchEndY);
                    const bool returnToMap=DetailLayout::goToMap(touchEndX,touchEndY);
                    if(showPhoto)drawPhotoScreen();
                    else if(showMore)drawMoreScreen();
                    else if(returnToMap) {
                        detailShowing=false;moreInfoShowing=false;
                        photo.clear();drawRadar();
                    }
                }
            } else if (SkyUI::setupHit(touchEndX,touchEndY,display.width()) &&
                       abs(dx)<16 && abs(dy)<16) {
                startSetupPortal();
            } else if (previewShowing &&
                       RadarPopup::dismiss.hit(touchEndX,touchEndY)) {
                previewShowing=false;followSelected=false;
                tracker.deselect();drawRadar();
            } else if (previewShowing &&
                       RadarPopup::details.hit(touchEndX,touchEndY)) {
                detailShowing=true;previewShowing=false;
                photo.searching();drawFlightScreen();
                const AircraftRecord* a=tracker.selectedRecord();
                photo.load(a?a->registration:nullptr,
                           a?a->type:nullptr,sdReady);
                drawFlightScreen();
            } else if (previewShowing &&
                       RadarPopup::follow.hit(touchEndX,touchEndY)) {
                followSelected=!followSelected;
                const AircraftRecord* a=tracker.selectedRecord();
                if(followSelected && a)
                    mapView.centerOn(a->lat,a->lon);
                drawRadar();
            } else if (previewShowing &&
                       RadarPopup::card.hit(touchEndX,touchEndY)) {
                // Text/background in the popup isn't a map tap.
            } else if (abs(dx) > 22 || abs(dy) > 22) {
                followSelected=false;
                mapView.panPixels(dx, dy);
                drawRadar();
            } else if (touchEndY < 31 && touchEndX < 98) {
                // On-device Wi-Fi/position setup without USB.
                mapShowing = false;
                tracker.deselect();
                photo.clear();
                startSetupPortal();
            } else if (SkyUI::mapControlHit(touchEndX,touchEndY,display.width())) {
                mapView.tap(touchEndX, touchEndY, display.width(), display.height());
                drawRadar();
            } else if (tracker.selectAt(touchEndX, touchEndY, mapView)) {
                // Show a fast, map-side preview first; the user can enter
                // the full photo/telemetry page using its DETAIL button.
                previewShowing=true;
                followSelected=false;
                drawRadar();
            } else if(previewShowing) {
                // Tap blank map to dismiss the selected aircraft popup.
                previewShowing=false;followSelected=false;
                tracker.deselect();drawRadar();
            }
        }
        touchingLastFrame = pressed;
    }
    if (mapShowing && millis() - lastFlightFetch >= tracker.pollIntervalMs()) {
        const bool ok = tracker.poll();
        lastFlightFetch = millis(); // never count slow HTTP time toward next poll
        Serial.printf("ADS-B: %s (%d aircraft)%s\\n",
                      tracker.status().c_str(), tracker.count(), ok ? "" : " - retrying");
        if(previewShowing && !tracker.hasSelection()) {
            previewShowing=false;
            followSelected=false;
            drawRadar();
        }
        bool followed=false;
        if(followSelected && previewShowing && !detailShowing) {
            const AircraftRecord* target=tracker.selectedRecord();
            if(target) {
                int px=0,py=0;
                const bool onScreen=mapView.project(target->lat,target->lon,px,py);
                if(!onScreen || abs(px-display.width()/2)>24 ||
                   abs(py-(SkyUI::kTop+(display.height()-SkyUI::kBottom))/2)>24) {
                    mapView.centerOn(target->lat,target->lon);
                    drawRadar();
                    followed=true;
                }
            }
        }
        if (detailShowing && !tracker.hasSelection()) {
            // A selected flight may leave the API radius. Return to the
            // map instead of showing a frozen, no-longer-updating detail page.
            detailShowing = false;
            photoFullscreen = false;
            photo.clear();
            drawRadar();
        } else if (detailShowing) {
            // Neither screen repaints its photo every 30s.
            if (moreInfoShowing) tracker.drawExtraDetails(display);
            else if (!photoFullscreen) tracker.updateDetailsTelemetry(display);
        } else if(!followed) {
            // Aircraft move; the static map background stays on the screen.
            tracker.drawChanges(display, mapView, sdReady);
            drawAircraftPopup();
            mapView.drawFlightStatus(display, tracker.count(), tracker.status(),
                             tracker.lastGoodPollMs() ?
                             int((millis()-tracker.lastGoodPollMs())/1000) : -1,
                             tracker.selectedRecord() ?
                             (tracker.selectedRecord()->flight[0] ?
                              tracker.selectedRecord()->flight :
                              tracker.selectedRecord()->hex) : nullptr);
        }
    }
    // Keep radar symbols moving between network snapshots. This does not
    // make additional HTTP requests and never redraws the whole map.
    if (mapShowing && !detailShowing && tracker.hasData() &&
        millis() - lastMotionFrame >= kMotionFrameMs) {
        lastMotionFrame = millis();
        tracker.drawChanges(display,mapView,sdReady);
        drawAircraftPopup();
        mapView.drawFlightStatus(display,tracker.count(),tracker.status(),
                    tracker.lastGoodPollMs() ?
                    int((millis()-tracker.lastGoodPollMs())/1000) : -1,
                             tracker.selectedRecord() ?
                             (tracker.selectedRecord()->flight[0] ?
                              tracker.selectedRecord()->flight :
                              tracker.selectedRecord()->hex) : nullptr);
    }
    if (mapShowing && WiFi.status() != WL_CONNECTED &&
        millis() - lastWiFiRetry > 30000UL) {
        lastWiFiRetry = millis();
        WiFi.reconnect();
    }
    if (portalActive) {
        dns.processNextRequest();
        web.handleClient();
    }

    delay(10);
}
