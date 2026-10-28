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
bool detailShowing = false;
uint32_t splashStartMs = 0;
constexpr uint32_t kMinSplashMs = 2500;
void finishSplash() {
    const uint32_t shown = millis() - splashStartMs;
    if (shown < kMinSplashMs) delay(kMinSplashMs - shown);
}
uint32_t lastFlightFetch = 0;
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

void drawRadar() {
    if (!mapShowing) return;
    mapView.draw(display, sdReady, WiFi.status() == WL_CONNECTED);
    tracker.drawMarkers(display, mapView);
    display.fillRect(110, 0, display.width() > 320 ? 90 : 85, 29, 0x08A3);
    display.setTextSize(1);
    display.setTextColor(TFT_YELLOW, 0x08A3);
    display.setCursor(110, 9);
    display.printf("%d planes", tracker.count());
    display.setTextColor(TFT_WHITE, 0x08A3);
    display.setCursor(5, display.height() - 31);
    display.print(tracker.status().substring(0, 23));
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

String configPage() {
    return R"HTML(<!doctype html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYD SkyTracker Setup</title>
<style>body{font:16px system-ui;background:#101b2c;color:#eff7ff;max-width:480px;
margin:2rem auto;padding:1rem}label{display:block;margin-top:1rem}
input,button{box-sizing:border-box;width:100%;padding:.85rem;border-radius:8px;
font:inherit}input{border:1px solid #698097;background:#203650;color:#fff}
button{margin-top:1.5rem;background:#1684c7;color:#fff;border:0}
small{color:#b8cade}</style></head><body>
<h1>Aircraft Radar Setup</h1><p>Connect this device to your 2.4 GHz home Wi-Fi.</p>
<form method="post" action="/save">
<label>Wi-Fi network name<input name="ssid" required maxlength="32"></label>
<label>Wi-Fi password<input name="password" type="password" maxlength="63"></label>
<label>Home latitude<input name="lat" type="number" step="any" min="-90" max="90" required></label>
<label>Home longitude<input name="lon" type="number" step="any" min="-180" max="180" required></label>
<button type="submit">Save and connect</button></form>
<p><small>This is an early prototype. A UK postcode lookup and real aircraft
map will be added in future versions.</small></p></body></html>)HTML";
}

void handleSave() {
    const String ssid = web.arg("ssid");
    const String password = web.arg("password");
    const String lat = web.arg("lat");
    const String lon = web.arg("lon");
    const float latitude = lat.toFloat();
    const float longitude = lon.toFloat();
    if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 63 ||
        lat.length() == 0 || lon.length() == 0 ||
        latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180) {
        web.send(400, "text/plain", "Invalid Wi-Fi or location settings");
        return;
    }
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.putFloat("lat", latitude);
    preferences.putFloat("lon", longitude);
    web.send(200, "text/html",
             "<h2>Saved!</h2><p>SkyTracker is restarting and connecting to your Wi-Fi.</p>");
    delay(1000);
    ESP.restart();
}

void startSetupPortal() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    const uint32_t suffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFU);
    apSsid = String("SkyTracker-") + String(suffix, HEX);
    apPassword = String("Setup-") + String(suffix, HEX);
    if (!WiFi.softAP(apSsid.c_str(), apPassword.c_str())) {
        statusScreen("Setup failed", "Restart the device");
        return;
    }
    dns.start(53, "*", WiFi.softAPIP());
    web.on("/", HTTP_GET, []() { web.send(200, "text/html", configPage()); });
    web.on("/save", HTTP_POST, handleSave);
    web.onNotFound([]() {
        web.sendHeader("Location", "http://192.168.4.1/", true);
        web.send(302, "text/plain", "");
    });
    web.begin();
    portalActive = true;
    finishSplash();
    statusScreen("Wi-Fi setup", apSsid,
                 "Password: " + apPassword, "Phone: http://192.168.4.1");
}

void startWifi() {
    const String savedSsid = preferences.getString("ssid", "");
    if (savedSsid.isEmpty()) {
        startSetupPortal();
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSsid.c_str(), preferences.getString("password", "").c_str());
    const unsigned long started = millis();
    Splash::status(display, "Connecting to home Wi-Fi...");
    while (WiFi.status() != WL_CONNECTED && millis() - started < 12000UL) {
        delay(150);
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WIFI] Connected, local IP %s\n", WiFi.localIP().toString().c_str());
        finishSplash();
        const float lat = preferences.getFloat("lat", 0.0f);
        const float lon = preferences.getFloat("lon", 0.0f);
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
    Serial.println("Tap SKYTRACKER at top left to open Wi-Fi setup hotspot.");
}

void loop() {
    if (mapShowing && touch.calibrated()) {
        int x = 0, y = 0;
        const bool pressed = touch.sample(display.width(), display.height(), x, y);
        if (pressed) {
            touchEndX = x; touchEndY = y;
            if (!touchingLastFrame) { touchStartX = x; touchStartY = y; }
        } else if (touchingLastFrame) {
            const int dx = touchEndX - touchStartX;
            const int dy = touchEndY - touchStartY;
            if (detailShowing) {
                // Keep the flight selected on return so its track and outline
                // remain visible on the map through subsequent ADS-B polls.
                detailShowing = false;
                drawRadar();
            } else if (abs(dx) > 22 || abs(dy) > 22) {
                mapView.panPixels(dx, dy);
                drawRadar();
            } else if (touchEndY < 31 && touchEndX < 98) {
                // On-device Wi-Fi/position setup without USB.
                mapShowing = false;
                tracker.deselect();
                startSetupPortal();
            } else if (touchEndX >= display.width() - 46 &&
                       touchEndY >= 32 && touchEndY < 170) {
                mapView.tap(touchEndX, touchEndY, display.width(), display.height());
                drawRadar();
            } else if (tracker.selectAt(touchEndX, touchEndY, mapView)) {
                detailShowing = true;
                tracker.drawDetails(display);
            }
        }
        touchingLastFrame = pressed;
    }
    if (mapShowing && millis() - lastFlightFetch > 30000UL) {
        lastFlightFetch = millis();
        const bool ok = tracker.poll();
        Serial.printf("ADS-B: %s (%d aircraft)%s\\n",
                      tracker.status().c_str(), tracker.count(), ok ? "" : " - retrying");
        if (detailShowing && !tracker.hasSelection()) {
            // A selected flight may leave the API radius. Return to the
            // map instead of showing a frozen, no-longer-updating detail page.
            detailShowing = false;
            drawRadar();
        } else if (detailShowing) tracker.drawDetails(display);
        else drawRadar();
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
    while (Serial.available()) {
        char ch = static_cast<char>(Serial.read());
        if (ch == '\r') continue;
        if (ch == '\n') {
            if (lastCommand == "CAL_TOUCH") {
                if (touch.calibrate(display, preferences)) {
                    Serial.println("Touch calibration saved.");
                } else {
                    Serial.println("Touch calibration cancelled or invalid.");
                }
                if (mapShowing) { detailShowing = false; drawRadar(); }
                else statusScreen("Touch calibration", touch.calibrated() ? "Calibration saved" : "Calibration failed");
                touchingLastFrame = false;
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
            lastCommand = "";
        } else if (lastCommand.length() < 40) {
            lastCommand += ch;
        }
    }
    delay(10);
}
