#pragma once
#include <TFT_eSPI.h>
#include "aircraft_symbol.h"

// Type-aware, heading-rotated aircraft silhouettes. No PNG or SD reads.
namespace AircraftIcon {
void draw(TFT_eSPI& tft, int x, int y, float heading,
          uint16_t fillColour, bool highlighted = false,
          AircraftSymbol::Kind kind = AircraftSymbol::Kind::Airliner,
          float scale = 0.86f);
}
