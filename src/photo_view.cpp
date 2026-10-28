#include "photo_view.h"
#include "photo_match.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <TJpg_Decoder.h>
#include <cstdlib>
#include <algorithm>

namespace {
constexpr size_t kMaxJpegBytes = 90000;
constexpr int kX = 7, kY = 39, kW = 150, kH = 99;
TFT_eSPI* currentDisplay = nullptr;

bool drawJpegBlock(int16_t x, int16_t y, uint16_t w, uint16_t h,
                   uint16_t* bitmap) {
    if (!currentDisplay || x >= kX + kW || y >= kY + kH) return false;
    // The decoder's blocks are generally 8 or 16 pixels wide. The image is
    // requested at 280px and scaled by 2, keeping it inside the photo panel.
    const uint16_t visibleRows = uint16_t(std::min<int>(int(h), kY + kH - y));
    currentDisplay->pushImage(x, y, w, visibleRows, bitmap);
    return true;
}

String plainText(const String& html) {
    String out;
    out.reserve(82);
    bool inTag = false;
    for (size_t i = 0; i < html.length() && out.length() < 77; ++i) {
        const char ch = html[i];
        if (ch == '<') { inTag = true; continue; }
        if (ch == '>') { inTag = false; continue; }
        if (!inTag && ch >= ' ' && ch <= '~') out += ch;
    }
    out.replace("&amp;", "&");
    out.replace("&quot;", "'");
    out.replace("&#39;", "'");
    out.replace("&nbsp;", " ");
    return out;
}

bool acceptedLicence(const String& licence) {
    // Credit is still mandatory for BY/BY-SA. Reject unknown/restricted
    // metadata and non-free licences, even if a search hit matches.
    return licence == "CC0" || licence == "Public domain" ||
           licence.startsWith("CC BY ") || licence.startsWith("CC BY-SA ");
}

bool validRegistration(const String& registration) {
    if (registration.length() < 4 || registration.length() > 14) return false;
    for (unsigned i = 0; i < registration.length(); ++i) {
        const char c = registration[i];
        if (!((c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-')) return false;
    }
    return true;
}

bool allowedImageUrl(const String& url) {
    // Prevent an API response from causing requests to arbitrary hosts.
    return url.startsWith("https://upload.wikimedia.org/") &&
           url.length() < 1024;
}
}

void PhotoView::clear() {
    if (jpeg_) free(jpeg_);
    jpeg_ = nullptr;
    size_ = 0;
    status_ = "No photo loaded";
    artist_ = "";
    licence_ = "";
    source_ = "";
}

void PhotoView::load(const char* registration) {
    clear();
    String reg = registration ? String(registration) : String("");
    reg.toUpperCase();
    if (!validRegistration(reg)) {
        status_ = "No registration";
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        status_ = "Photo needs Wi-Fi";
        return;
    }
    status_ = "Searching Commons...";
    Serial.printf("[PHOTO] Looking for %s on Wikimedia Commons\n", reg.c_str());

    WiFiClientSecure client;
    // Development data-only fetching. Production will use a verified
    // certificate bundle; never use downloaded content for OTA or settings.
    client.setInsecure();
    client.setTimeout(6500);
    HTTPClient http;
    http.setTimeout(6500);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    const String url = "https://commons.wikimedia.org/w/api.php?action=query" 
        "&generator=search&gsrnamespace=6&gsrlimit=5&gsrsearch=intitle%3A%22" +
        reg + "%22&prop=imageinfo&iiprop=url%7Cextmetadata%7Cmime"
        "&iiurlwidth=280&format=json&formatversion=2";
    if (!http.begin(client, url)) {
        status_ = "Photo search unavailable";
        return;
    }
    http.setUserAgent("CYD-SkyTracker/0.2 (https://github.com/shrek1802/CYD-SkyTracker)");
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        status_ = String("Photo search HTTP ") + code;
        Serial.println("[PHOTO] " + status_);
        http.end();
        return;
    }

    JsonDocument filter;
    JsonObject page = filter["query"]["pages"][0].to<JsonObject>();
    page["title"] = true;
    JsonObject info = page["imageinfo"][0].to<JsonObject>();
    info["thumburl"] = true;
    info["descriptionurl"] = true;
    info["mime"] = true;
    info["extmetadata"]["Artist"]["value"] = true;
    info["extmetadata"]["LicenseShortName"]["value"] = true;

    JsonDocument doc;
    const DeserializationError err = deserializeJson(
        doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (err) {
        status_ = "Photo search JSON error";
        Serial.printf("[PHOTO] JSON: %s\n", err.c_str());
        return;
    }

    String thumbnailUrl;
    JsonArrayConst pages = doc["query"]["pages"].as<JsonArrayConst>();
    for (JsonVariantConst item : pages) {
        const String title = item["title"].as<String>();
        if (!PhotoMatch::titleHasRegistration(title.c_str(), reg.c_str())) continue;
        JsonObjectConst image = item["imageinfo"][0].as<JsonObjectConst>();
        if (image["mime"].as<String>() != "image/jpeg") continue;
        JsonObjectConst metadata = image["extmetadata"].as<JsonObjectConst>();
        const String lic = metadata["LicenseShortName"]["value"].as<String>();
        if (!acceptedLicence(lic)) continue;
        const String candidate = image["thumburl"].as<String>();
        if (!allowedImageUrl(candidate)) continue;
        thumbnailUrl = candidate;
        artist_ = plainText(metadata["Artist"]["value"].as<String>());
        if (artist_.isEmpty()) artist_ = "Wikimedia contributor";
        licence_ = lic;
        source_ = image["descriptionurl"].as<String>();
        break;
    }
    if (thumbnailUrl.isEmpty()) {
        status_ = "No freely licensed photo";
        Serial.printf("[PHOTO] No licensed exact-registration image for %s\n", reg.c_str());
        return;
    }

    WiFiClientSecure imageClient;
    imageClient.setInsecure();
    imageClient.setTimeout(6500);
    HTTPClient imageHttp;
    imageHttp.setTimeout(6500);
    imageHttp.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    if (!imageHttp.begin(imageClient, thumbnailUrl)) {
        status_ = "Thumbnail connection failed";
        return;
    }
    imageHttp.setUserAgent("CYD-SkyTracker/0.2 (https://github.com/shrek1802/CYD-SkyTracker)");
    const int imageCode = imageHttp.GET();
    const int contentLength = imageHttp.getSize();
    if (imageCode != HTTP_CODE_OK || contentLength < 16 ||
        contentLength > int(kMaxJpegBytes)) {
        status_ = "Thumbnail unavailable";
        Serial.printf("[PHOTO] Image HTTP %d length %d\n", imageCode, contentLength);
        imageHttp.end();
        return;
    }
    if (ESP.getFreeHeap() < uint32_t(contentLength + 40000)) {
        status_ = "Not enough photo memory";
        imageHttp.end();
        return;
    }
    jpeg_ = static_cast<uint8_t*>(malloc(size_t(contentLength)));
    if (!jpeg_) {
        status_ = "Photo memory allocation failed";
        imageHttp.end();
        return;
    }
    const size_t got = imageHttp.getStream().readBytes(jpeg_, size_t(contentLength));
    imageHttp.end();
    if (got != size_t(contentLength) ||
        jpeg_[0] != 0xFF || jpeg_[1] != 0xD8) {
        status_ = "Unsupported photo data";
        free(jpeg_);
        jpeg_ = nullptr;
        return;
    }
    size_ = got;
    status_ = "Photo available";
    Serial.printf("[PHOTO] Loaded %u-byte licensed JPEG from Commons\n", unsigned(got));
    if (source_.length()) Serial.printf("[PHOTO] Photographer/source: %s | %s | %s\n",
                    artist_.c_str(), licence_.c_str(), source_.c_str());
}

void PhotoView::draw(TFT_eSPI& display) const {
    display.fillRoundRect(kX - 2, kY - 2, kW + 4, kH + 4, 5, 0x10A4);
    display.drawRoundRect(kX - 2, kY - 2, kW + 4, kH + 4, 5, TFT_DARKGREY);
    if (!jpeg_ || !size_) {
        display.setTextColor(TFT_WHITE, 0x10A4);
        display.setTextSize(1);
        display.drawCentreString(status_.substring(0, 23),
                                 kX + kW/2, kY + kH/2 - 5, 1);
        return;
    }
    currentDisplay = &display;
    TJpgDec.setJpgScale(2);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(drawJpegBlock);
    TJpgDec.drawJpg(kX + 1, kY + 1, jpeg_, uint32_t(size_));
    currentDisplay = nullptr;
}

void PhotoView::drawCredit(TFT_eSPI& display) const {
    display.setTextSize(1);
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.setCursor(7, 184);
    if (!jpeg_) {
        display.print("Photo: " + status_);
        return;
    }
    display.print("Photo: ");
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.print(artist_.substring(0, 42));
    display.setCursor(7, 198);
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.print(licence_.substring(0, 18) + "  -  Wikimedia Commons");
    display.setCursor(7, 211);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.print("Original link: USB serial [PHOTO]");
}
