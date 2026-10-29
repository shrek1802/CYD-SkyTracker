#pragma once
// Visually compare native TFT RGB565 colours with SD-streamed RGB565-LE.
// Runs on the already confirmed CYD ILI9341 v2 profile; no pin changes.
#include <TFT_eSPI.h>
#include <SD.h>
#include <cstdint>
#include "rgb565_display.h"

namespace SkyColourDiagnostic {
constexpr int kWidth=320, kHeight=96;
inline bool draw(TFT_eSPI& tft, bool sdReady) {
    static const uint16_t swatches[8]={
        TFT_RED,TFT_GREEN,TFT_BLUE,TFT_WHITE,
        TFT_YELLOW,TFT_CYAN,TFT_MAGENTA,TFT_BLACK
    };
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.setCursor(7,8);
    tft.print("RGB565 COLOUR TEST - TFT vs SD");
    tft.setCursor(7,26);
    tft.print("Native TFT pixels:");
    for (int i=0;i<8;++i) {
        tft.fillRect(i*40,42,40,43,swatches[i]);
    }
    tft.setCursor(7,89);
    tft.print("SD RGB565-LE pixels:");
    if (!sdReady) {
        tft.setCursor(7,196);tft.print("NO SD CARD");
        return false;
    }
    File file=SD.open("/diagnostics/rgb565-check.rgb565",FILE_READ);
    if (!file || file.size()!=size_t(kWidth)*kHeight*2) {
        if(file)file.close();
        tft.setCursor(7,196);tft.print("MISSING /diagnostics/rgb565-check.rgb565");
        return false;
    }
    static uint16_t row[kWidth];
    bool ok=true;
    tft.startWrite();
    for(int y=0;y<kHeight;++y) {
        if(file.read(reinterpret_cast<uint8_t*>(row),sizeof(row))!=sizeof(row)) {
            ok=false;break;
        }
        SkyRgb565::pushRow(tft,0,104+y,kWidth,row);
    }
    tft.endWrite();
    file.close();
    tft.setCursor(7,210);
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.print(ok?"MATCH TOP/BOTTOM? PHOTO SCREEN":"SD SHORT READ");
    return ok;
}
}
