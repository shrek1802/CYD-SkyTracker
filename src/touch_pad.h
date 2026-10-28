#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// XPT2046 resistive touch for the initial five Sunton SPI-board profiles.
// Other touchscreen types must implement the same sample() API separately.
class TouchPad {
public:
    TouchPad();
    void begin(Preferences& preferences);
    bool sample(int width, int height, int& x, int& y);
    bool calibrated() const { return calibrated_; }
    bool calibrate(TFT_eSPI& display, Preferences& preferences);
private:
    SPIClass bus_;
    XPT2046_Touchscreen chip_;
    int raw_[3][2] = {};
    bool calibrated_ = false;
    bool capturePoint(int index, TFT_eSPI& tft, int x, int y);
};
