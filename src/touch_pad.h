#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

// Shared XPT2046 touch abstraction for resistive Sunton CYDs.
// Software SPI avoids resetting the hardware SPI bus used by TFT or SD.
// Other capacitive touch variants will use a separate driver in future.
class TouchPad {
public:
    void begin(Preferences& preferences);
    bool sample(int width, int height, int& x, int& y);
    bool calibrated() const { return calibrated_; }
    bool calibrate(TFT_eSPI& display, Preferences& preferences);
private:
    struct Point { int x, y, pressure; };
    bool readRaw(Point& out);
    uint16_t transfer16(uint16_t v);
    uint16_t measure(uint8_t command);
    bool capturePoint(int index, TFT_eSPI& tft, int x, int y);
    int raw_[3][2] = {};
    bool calibrated_ = false;
    uint32_t lastSample_ = 0;
};
