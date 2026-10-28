#pragma once

// Hardware profile IDs are stable, and are used to reject wrong-board SD
// firmware during development and (later) by signed release manifests.
// Profiles are selected at *build time*, not autodetected from the display.
#ifndef SKYTRACKER_BOARD_ID
#error "Select a CYD hardware profile in platformio.ini"
#endif
#ifndef TFT_BL
#error "Hardware profile must define a backlight pin"
#endif

namespace BoardProfile {
constexpr const char* id() {
#if SKYTRACKER_BOARD_ID == 1
    return "sunton-2432s028r-ili9341";
#elif SKYTRACKER_BOARD_ID == 2
    return "sunton-2432s028r-v2-ili9341";
#elif SKYTRACKER_BOARD_ID == 3
    return "sunton-2432s028r-v3-st7789";
#elif SKYTRACKER_BOARD_ID == 4
    return "sunton-2432s024r-ili9341";
#elif SKYTRACKER_BOARD_ID == 5
    return "sunton-3248s035r-st7796";
#else
#error "Unknown CYD hardware profile"
#endif
}
constexpr int backlightPin() { return TFT_BL; }

// Identical SD wiring for these five *Sunton* SPI-based variants only.
// S3/RGB and Guition JC boards MUST get separate profiles and drivers.
constexpr int sdCs() { return 5; }
constexpr int sdClk() { return 18; }
constexpr int sdMiso() { return 19; }
constexpr int sdMosi() { return 23; }
}
