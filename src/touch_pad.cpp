#include "touch_pad.h"
#include "board_profile.h"
#include <cmath>

namespace {
constexpr int kTouchCs = 33;
constexpr int kMargin = 25;
constexpr int kTopAnchorY = 50;
constexpr uint32_t kCalTimeoutMs = 16000;
constexpr int kRawMinimum = 100;
constexpr int kRawMaximum = 4095;
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

TouchPad::TouchPad() : bus_(VSPI), chip_(kTouchCs) {}

void TouchPad::begin(Preferences& prefs) {
    bus_.begin(kTouchClk, kTouchMiso, kTouchMosi, kTouchCs);
    chip_.begin(bus_);
    chip_.setRotation(1);
    calibrated_ = prefs.getBool("calValid", false);
    for (int i = 0; i < 3; ++i) {
        raw_[i][0] = prefs.getInt((String("cal") + i + "x").c_str(), 0);
        raw_[i][1] = prefs.getInt((String("cal") + i + "y").c_str(), 0);
    }
    if (calibrated_) {
        const double ax = raw_[1][0] - raw_[0][0];
        const double ay = raw_[1][1] - raw_[0][1];
        const double bx = raw_[2][0] - raw_[0][0];
        const double by = raw_[2][1] - raw_[0][1];
        if (std::fabs(ax*by-ay*bx) < 200000.0) calibrated_ = false;
    }
}

bool TouchPad::sample(int width, int height, int& x, int& y) {
    if (!calibrated_ || !chip_.touched()) return false;
    TS_Point p = chip_.getPoint();
    if (p.z < 300 || p.x < kRawMinimum || p.y < kRawMinimum ||
        p.x > kRawMaximum || p.y > kRawMaximum) return false;
    const double ax = raw_[1][0] - raw_[0][0];
    const double ay = raw_[1][1] - raw_[0][1];
    const double bx = raw_[2][0] - raw_[0][0];
    const double by = raw_[2][1] - raw_[0][1];
    const double determinant = ax*by - ay*bx;
    if (std::fabs(determinant) < 200000.0) return false;
    const double px = p.x - raw_[0][0];
    const double py = p.y - raw_[0][1];
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
        const bool pressed = chip_.touched();
        if (pressed && !hadPress) {
            const TS_Point p = chip_.getPoint();
            if (p.z >= 300) {
                raw_[index][0] = p.x;
                raw_[index][1] = p.y;
                while (chip_.touched() && millis() - start < kCalTimeoutMs) delay(15);
                return true;
            }
        }
        hadPress = pressed;
        delay(15);
    }
    return false;
}
bool TouchPad::calibrate(TFT_eSPI& tft, Preferences& prefs) {
    const int w = tft.width();
    const int h = tft.height();
    const int points[3][2] = {
        {kMargin, kTopAnchorY},
        {w - kMargin, kTopAnchorY},
        {kMargin, h - kMargin}
    };
    // Top cross is 25px lower to keep calibration text readable.
    // The affine transform assumes the actual TL/T-right/BL target positions.
    // Use the same nominal anchor Y in both first points below.
    for (int i=0; i<3; ++i) {
        if (!capturePoint(i, tft, points[i][0], points[i][1])) return false;
    }
    // If axis geometry is degenerate, reject calibration.
    const double ax = raw_[1][0] - raw_[0][0];
    const double ay = raw_[1][1] - raw_[0][1];
    const double bx = raw_[2][0] - raw_[0][0];
    const double by = raw_[2][1] - raw_[0][1];
    if (std::fabs(ax*by - ay*bx) < 200000.0) return false;
    for (int i=0; i<3; ++i) {
        prefs.putInt((String("cal") + i + "x").c_str(), raw_[i][0]);
        prefs.putInt((String("cal") + i + "y").c_str(), raw_[i][1]);
    }
    prefs.putBool("calValid", true);
    calibrated_ = true;
    return true;
}
