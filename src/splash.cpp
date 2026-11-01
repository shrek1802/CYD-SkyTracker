#include "splash.h"
#include <Arduino.h>
#include <algorithm>
#include <cmath>

namespace {
constexpr uint16_t kBackground = 0x010A;  // midnight navy
constexpr uint16_t kCyan = 0x067F;
constexpr uint16_t kBrightCyan = 0x07DF;
constexpr uint16_t kBlue = 0x03B5;
constexpr uint16_t kSoftBlue = 0x01E6;
int safeRadius(int w,int h) {
    return std::max(35, std::min(w / 4, (h * 31) / 100));
}
void drawPlane(TFT_eSPI& t,int cx,int cy,int r) {
    // Top-down stylised airliner, light enough for a 320 x 240 splash.
    const int tail = std::max(3,r/13);
    t.fillTriangle(cx,cy-r*3/5,cx-tail,cy+r/2,cx+tail,cy+r/2,TFT_WHITE);
    t.fillTriangle(cx,cy-r/3,cx-r*3/5,cy+r/6,cx+r*3/5,cy+r/6,TFT_WHITE);
    t.fillTriangle(cx,cy+r/3,cx-r/5,cy+r*3/5,cx+r/5,cy+r*3/5,TFT_WHITE);
}
}
namespace Splash {
void draw(TFT_eSPI& t, const char* message) {
    const int w = t.width(),h = t.height();
    const int r = safeRadius(w,h);
    const int cx = w / 2,cy = r + 11;
    t.fillScreen(kBackground);
    // Same core geometry as the SVG web-flasher logo.
    t.fillCircle(cx,cy,r+5,kBlue);
    t.fillCircle(cx,cy,r,kBackground);
    t.drawCircle(cx,cy,r,kBrightCyan);
    t.drawCircle(cx,cy,r*3/4,kCyan);
    t.drawCircle(cx,cy,r/2,kCyan);
    t.drawCircle(cx,cy,r/4,kSoftBlue);
    t.drawFastHLine(cx-r,cy,2*r,kBlue);
    t.drawFastVLine(cx,cy-r,2*r,kBlue);
    // Radar targets
    t.fillCircle(cx-r/2,cy-r/2,3,TFT_CYAN);
    t.fillCircle(cx+r*2/3,cy-r/4,3,TFT_CYAN);
    t.fillCircle(cx+r/2,cy+r/2,2,TFT_CYAN);
    drawPlane(t,cx,cy,r);
    // Static, branded wordmark. Kept beneath the circle on 240px displays.
    t.setTextColor(TFT_WHITE,kBackground);
    t.setTextSize(2);
    t.drawCentreString("SkyTracker",cx,2*r+22,1);
    t.setTextColor(TFT_CYAN,kBackground);
    t.setTextSize(1);
    t.drawCentreString("CYD  -  LIVE FLIGHT RADAR",cx,2*r+48,1);
    status(t,message);
}
void status(TFT_eSPI& t,const char* message) {
    const int w=t.width(),h=t.height();
    const int barX=w/5,barW=w*3/5;
    const int statusY=h-26;
    t.fillRect(0,statusY-3,w,29,kBackground);
    t.setTextColor(TFT_WHITE,kBackground);
    t.setTextSize(1);
    t.drawCentreString(message, w/2,statusY,1);
    t.fillRoundRect(barX,h-11,barW,5,2,kBlue);
    // Startup indicator, not an inaccurate completion percentage.
    t.fillRoundRect(barX,h-11,std::max(2,barW/3),5,2,kBrightCyan);
}
}
