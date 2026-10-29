#pragma once
#include <TFT_eSPI.h>

// Lightweight, resolution-independent startup artwork for every SPI CYD.
// Drawn as vector shapes; no expensive PNG/JPEG decoding at boot.
namespace Splash {
    void draw(TFT_eSPI& screen, const char* message = "Starting...");
    void status(TFT_eSPI& screen, const char* message);
}
