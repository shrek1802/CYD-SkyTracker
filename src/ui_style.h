#pragma once
#include <TFT_eSPI.h>
#include <cstring>

// Layout is designed around the user's physically tested 320x240
// ILI9341 in landscape, but uses the current TFT width where practical.
namespace SkyUI {
constexpr uint16_t kBackground = 0x0843; // deep navy
constexpr uint16_t kPanel = 0x10A5;      // slate
constexpr uint16_t kPanelAlt = 0x18E7;
constexpr uint16_t kBorder = 0x318B;
constexpr uint16_t kWhite = 0xE77D;
constexpr uint16_t kMuted = 0xAD76;
constexpr uint16_t kAccent = 0x47FB; // icy blue
constexpr uint16_t kGold = 0xFEC5;
constexpr uint16_t kGreen = 0x4ECA;
constexpr int kTop = 27;
constexpr int kBottom = 24;
constexpr int kControlW = 31;
constexpr int kControlH = 29;
constexpr int kControlXMargin = 5;
constexpr int kControlY[3] = {36, 72, 108};
constexpr int kThemeY = 144;
inline bool mapControlHit(int x, int y, int w) {
    return x >= w - kControlW - kControlXMargin - 3 &&
           y >= kControlY[0] - 3 && y <= kThemeY + kControlH + 3;
}
inline void button(TFT_eSPI& tft,int x,int y,int w,int h,
                   const char* label,bool primary=false) {
    const uint16_t bg=primary?kAccent:kPanel;
    tft.fillRoundRect(x,y,w,h,5,bg);
    tft.drawRoundRect(x,y,w,h,5,primary?kAccent:kBorder);
    tft.setTextColor(primary?kBackground:kWhite,bg);
    tft.setTextSize(1);
    const int len=static_cast<int>(strlen(label));
    tft.setCursor(x+(w-len*6)/2,y+(h-8)/2);
    tft.print(label);
}
}
