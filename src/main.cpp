#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include "sd_update.h"

namespace {
constexpr int SD_CS = 5;
constexpr int SD_SCK = 18;
constexpr int SD_MISO = 19;
constexpr int SD_MOSI = 23;
constexpr int TFT_BL_PIN = 21;
Preferences preferences;
TFT_eSPI display;
SPIClass sdSpi(VSPI);
WebServer web(80);
DNSServer dns;
bool portalActive = false;
bool sdReady = false;
String apSsid;
String apPassword;
String lastCommand;

void statusScreen(const String& heading, const String& line1,
                  const String& line2 = "", const String& line3 = "") {
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.setTextSize(2);
    display.setCursor(8, 10);
    display.println("CYD SKYTRACKER");
    display.drawFastHLine(8, 37, 304, TFT_DARKGREY);
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
    display.setCursor(8, 212);
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.println("DEVELOPMENT BUILD - NO RADAR YET");
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
    statusScreen("Connecting...", savedSsid);
    while (WiFi.status() != WL_CONNECTED && millis() - started < 12000UL) {
        delay(150);
    }
    if (WiFi.status() == WL_CONNECTED) {
        statusScreen("Wi-Fi connected", WiFi.localIP().toString(),
                     sdReady ? "SD card ready" : "No SD card detected",
                     "Air traffic screen: coming soon");
    } else {
        startSetupPortal();
    }
}
}

void setup() {
    Serial.begin(115200);
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);
    display.init();
    display.setRotation(1);
    preferences.begin("skytracker", false);
    statusScreen("Starting...", "Initialising microSD");
    sdSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    sdReady = SD.begin(SD_CS, sdSpi, 4000000);
    startWifi();
    Serial.println("CYD SkyTracker development build.");
    Serial.println("For physical USB-serial test only: type INSTALL_SD to install");
    Serial.println("/updates/firmware.bin with matching /updates/sha256.txt.");
}

void loop() {
    if (portalActive) {
        dns.processNextRequest();
        web.handleClient();
    }
    while (Serial.available()) {
        char ch = static_cast<char>(Serial.read());
        if (ch == '\r') continue;
        if (ch == '\n') {
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
