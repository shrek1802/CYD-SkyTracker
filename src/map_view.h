#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// One renderer for every currently targeted screen resolution.
// Stream one RGB565 row at a time from microSD (no 128 KiB tile buffers).
class MapView {
public:
    void begin(double homeLat, double homeLon, uint8_t zoom = 10);
    void draw(TFT_eSPI& tft, bool sdReady, bool wifiConnected);
    // Restore only a small aircraft footprint from the existing SD map.
    // Does not clear the full screen or redraw all map tiles.
    void restoreRect(TFT_eSPI& tft, int x, int y, int w, int h, bool sdReady);
    void drawFlightStatus(TFT_eSPI& tft, int aircraftCount, const String& status);
    void tap(int x, int y, int screenWidth, int screenHeight);
    void panPixels(int dx, int dy);
    bool project(double lat, double lon, int& x, int& y) const;
    uint8_t zoom() const { return zoom_; }
    bool isReady() const { return ready_; }
private:
    void drawTiles(TFT_eSPI& tft);
    void drawUi(TFT_eSPI& tft, bool wifiConnected, bool sdReady, int missingCount);
    void setZoom(uint8_t level);
    double homeLat_ = 0;
    double homeLon_ = 0;
    double centerLat_ = 0;
    double centerLon_ = 0;
    uint8_t zoom_ = 10;
    bool ready_ = false;
    int missing_ = 0;
    int width_ = 0;
    int top_ = 32;
    int bottom_ = 0;
};
