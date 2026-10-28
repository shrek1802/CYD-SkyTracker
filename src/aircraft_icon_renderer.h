#pragma once
#include <TFT_eSPI.h>

// 2D filled aircraft silhouettes. No external PNG/SVG resources on ESP32,
// no SD dependency, and orientation comes directly from ADS-B ground track.
namespace AircraftIcon {
void draw(TFT_eSPI& tft, int x, int y, float heading,
          uint16_t fillColour, bool highlighted = false);
}
