#pragma once
#include <cstdint>
class TFT_eSPI {
public:
    void setSwapBytes(bool enabled) { current = enabled; switches++; }
    void pushImage(int x,int y,int width,int height,uint16_t* row) {
        called = true;
        swappedDuringPush = current;
        lastX = x; lastY = y; lastWidth = width; lastHeight = height;
        firstPixel = row[0];
    }
    bool current = false,called = false,swappedDuringPush = false;
    int switches = 0,lastX = -1,lastY = -1,lastWidth = -1,lastHeight = -1;
    uint16_t firstPixel = 0;
};
