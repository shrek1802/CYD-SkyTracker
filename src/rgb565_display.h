#pragma once
#include <TFT_eSPI.h>
#include <cstdint>

// SD map and aircraft pixels are little-endian on disk (RGB565):
// red 0xF800 is stored as bytes [0x00, 0xF8].
// TFT_eSPI::pushImage() defaults to _swapBytes=false and sends those bytes
// verbatim over SPI, which displays red as the wrong colour (0x00F8).
// Correctly send a uint16_t RGB565 pixel as high byte first on the wire.
// Leave the global TFT mode in its default state for regular UI/JPEG paths.
namespace SkyRgb565 {
inline void pushRow(TFT_eSPI& tft,int x,int y,int width,uint16_t* row) {
    tft.setSwapBytes(true);
    tft.pushImage(x,y,width,1,row);
    tft.setSwapBytes(false);
}
}
