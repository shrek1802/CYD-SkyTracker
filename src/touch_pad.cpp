#include "touch_pad.h"
#include "board_profile.h"
#include <cmath>

namespace {
constexpr int kTouchCs = 33;
constexpr int kMargin = 25;
constexpr int kTopAnchorY = 50;
constexpr uint32_t kCalTimeoutMs = 18000;
constexpr int kRawMinimum = 100;
constexpr int kRawMaximum = 4095;
// Classic 2.8-inch CYD uses independent touch pins. 2.4"/3.5" boards
// use the display SPI *wires* but their own touch chip select.
#if SKYTRACKER_BOARD_ID >= 1 && SKYTRACKER_BOARD_ID <= 3
constexpr int kTouchClk = 25;
constexpr int kTouchMiso = 39;
constexpr int kTouchMosi = 32;
#else
constexpr int kTouchClk = 14;
constexpr int kTouchMiso = 12;
constexpr int kTouchMosi = 13;
#endif
}

void TouchPad::begin(Preferences& prefs) {
    pinMode(kTouchCs, OUTPUT);
    digitalWrite(kTouchCs, HIGH);
    pinMode(kTouchClk, OUTPUT);
    digitalWrite(kTouchClk, LOW);
    pinMode(kTouchMosi, OUTPUT);
    pinMode(kTouchMiso, INPUT);
    calibrated_ = prefs.getBool("calValid", false);
    for (int i = 0; i < 3; ++i) {
        raw_[i][0] = prefs.getInt((String("cal") + i + "x").c_str(), 0);
        raw_[i][1] = prefs.getInt((String("cal") + i + "y").c_str(), 0);
    }
    const double ax = raw_[1][0] - raw_[0][0];
    const double ay = raw_[1][1] - raw_[0][1];
    const double bx = raw_[2][0] - raw_[0][0];
    const double by = raw_[2][1] - raw_[0][1];
    if (std::fabs(ax*by - ay*bx) < 200000.0) calibrated_ = false;
}
uint16_t TouchPad::transfer16(uint16_t v) {
    uint16_t incoming = 0;
    for (int bit = 15; bit >= 0; --bit) {
        digitalWrite(kTouchClk, LOW);
        digitalWrite(kTouchMosi, (v & (uint16_t(1) << bit)) ? HIGH : LOW);
        delayMicroseconds(1);
        digitalWrite(kTouchClk, HIGH);
        incoming = uint16_t((incoming << 1) | (digitalRead(kTouchMiso) ? 1 : 0));
        delayMicroseconds(1);
    }
    digitalWrite(kTouchClk, LOW);
    return incoming;
}
uint16_t TouchPad::measure(uint8_t command) {
    digitalWrite(kTouchCs, LOW);
    // XPT2046 transmits 8-bit command then 12-bit result in 16-bit frame.
    // Keeping select low for the command and result avoids bus interference.
    uint16_t commandResponse = transfer16(uint16_t(command) << 8);
    (void)commandResponse;
    uint16_t value = transfer16(0);
    digitalWrite(kTouchCs, HIGH);
    return (value >> 3) & 0x0FFF;
}
bool TouchPad::readRaw(Point& out) {
    // Sample several times; valid pressure must be present at contact.
    const int z1 = measure(0xB1);
    const int z2 = measure(0xC1);
    const int pressure = z1 + 4095 - z2;
    if (pressure < 300) return false;
    int x = 0, y = 0;
    for (int i=0; i<3; ++i) {
        x += int(measure(0xD1));
        y += int(measure(0x91));
    }
    out.x = x / 3;
    out.y = y / 3;
    out.pressure = pressure;
    return out.x >= kRawMinimum && out.y >= kRawMinimum &&
           out.x <= kRawMaximum && out.y <= kRawMaximum;
}
bool TouchPad::sample(int width, int height, int& x, int& y) {
    if (!calibrated_ || width <= 60 || height <= 90) return false;
    if (millis() - lastSample_ < 25) return false;
    lastSample_ = millis();
    Point p;
    if (!readRaw(p)) return false;
    const double ax = raw_[1][0] - raw_[0][0];
    const double ay = raw_[1][1] - raw_[0][1];
    const double bx = raw_[2][0] - raw_[0][0];
    const double by = raw_[2][1] - raw_[0][1];
    const double determinant = ax*by - ay*bx;
    if (std::fabs(determinant) < 200000.0) return false;
    const double px = p.x - raw_[0][0], py = p.y - raw_[0][1];
    const double sx = (px*by - py*bx) / determinant;
    const double sy = (ax*py - ay*px) / determinant;
    x = constrain(int(std::lround(kMargin + sx * (width - 2*kMargin))), 0, width - 1);
    y = constrain(int(std::lround(kTopAnchorY + sy * (height - kMargin - kTopAnchorY))), 0, height - 1);
    return true;
}
bool TouchPad::capturePoint(int index, TFT_eSPI& tft, int x, int y) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(8, 8);
    tft.print("Touch calibration");
    tft.setTextSize(1);
    tft.setCursor(8, 38);
    tft.printf("Touch cross %d / 3", index + 1);
    tft.drawCircle(x, y, 11, TFT_CYAN);
    tft.drawFastHLine(x - 14, y, 29, TFT_YELLOW);
    tft.drawFastVLine(x, y - 14, 29, TFT_YELLOW);
    const uint32_t start = millis();
    bool hadPress = false;
    while (millis() - start < kCalTimeoutMs) {
        Point p;
        const bool pressed = readRaw(p);
        if (pressed && !hadPress) {
            raw_[index][0] = p.x;
            raw_[index][1] = p.y;
            while (readRaw(p) && millis() - start < kCalTimeoutMs) delay(20);
            return true;
        }
        hadPress = pressed;
        delay(20);
    }
    return false;
}
bool TouchPad::calibrate(TFT_eSPI& tft, Preferences& prefs) {
    const int w = tft.width(), h = tft.height();
    const int points[3][2] = {
        {kMargin, kTopAnchorY}, {w - kMargin, kTopAnchorY}, {kMargin, h - kMargin}
    };
    for (int i=0; i<3; ++i) {
        if (!capturePoint(i, tft, points[i][0], points[i][1])) return false;
    }
    const double ax = raw_[1][0] - raw_[0][0], ay = raw_[1][1] - raw_[0][1];
    const double bx = raw_[2][0] - raw_[0][0], by = raw_[2][1] - raw_[0][1];
    if (std::fabs(ax*by - ay*bx) < 200000.0) return false;
    for (int i=0; i<3; ++i) {
        prefs.putInt((String("cal") + i + "x").c_str(), raw_[i][0]);
        prefs.putInt((String("cal") + i + "y").c_str(), raw_[i][1]);
    }
    prefs.putBool("calValid", true);
    calibrated_ = true;
    return true;
}
