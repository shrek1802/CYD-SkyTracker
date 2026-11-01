#include "../src/rgb565_display.h"
#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    TFT_eSPI display;
    // The images/tiles store little-endian bytes [00,F8] for red.
    const uint8_t diskBytes[6]={0x00,0xF8,0xE0,0x07,0x1F,0x00};
    uint16_t pixels[3]={};
    for(int i=0;i<3;++i) {
        pixels[i]=uint16_t(diskBytes[2*i]|(uint16_t(diskBytes[2*i+1])<<8));
    }
    assert(pixels[0]==0xF800); // red
    assert(pixels[1]==0x07E0); // green
    assert(pixels[2]==0x001F); // blue
    SkyRgb565::pushRow(display,20,44,3,pixels);
    assert(display.called);
    assert(display.swappedDuringPush); // SPI sends high byte first
    assert(!display.current);          // don't leak state into other rendering
    assert(display.switches==2);
    assert(display.lastX==20 && display.lastY==44 &&
           display.lastWidth==3 && display.lastHeight==1);
    assert(display.firstPixel==0xF800);
    std::cout << "SkyTracker RGB565 display byte-order test PASS\n";
}
