# Fix: SD map and aircraft photographs appeared cyan/purple/neon
29 October 2026 — development branch. Not yet physically verified.

## Root cause, confirmed in source

The user's physical 2.8-inch dual-USB CYD ILI9341 v2 showed normal
solid-colour UI elements, but SD-sourced map tiles and aircraft photos
looked excessively cyan/pink/green.

Both SD builders store **little-endian RGB565** pixel words (e.g. red
`0xF800` stored as bytes `00 F8`). The firmware reads into an ESP32
`uint16_t*` buffer then called `TFT_eSPI::pushImage` with default
`_swapBytes=false`, which writes those memory bytes unaltered to SPI.
ILI9341 expects the high RGB565 byte first: `F8 00` for red.

TFT_eSPI's ESP32 implementation explicitly dispatches to
`pushSwapBytePixels` when `_swapBytes=true`.
Therefore the original byte stream was the wrong way round, changing
the apparent colours even though file reads and `pushImage()` reported
success.

Reference: `Bodmer/TFT_eSPI` 
`Processors/TFT_eSPI_ESP32.c` (SPI pushPixels).

## Fix

`src/rgb565_display.h` contains `SkyRgb565::pushRow()`, which
temporarily calls `tft.setSwapBytes(true)`, pushes the host-order
16-bit pixels, then restores `setSwapBytes(false)`.

Both the full-map and clipped map-patch renderers, and the direct SD
aircraft image/thumbnail/full-screen renderers use this helper.

The independent JPEG decoder callback was not modified because its
decoder has its own colour conversion and swap handling.

Existing FAT32 SD files are **already correctly encoded** and need no
conversion. Keep the user's improved `/maps/` and `/aircraft/` folders
as they are. This fix is application firmware only. Do not erase NVS
or reformat SD.

## Regression coverage

`tests/test_rgb565_display.cpp` checks the standard
`00 F8 / E0 07 / 1F 00` disk byte sequence for red/green/blue, then
uses a mocked TFT to assert that byteswapping is enabled during
`pushImage` and disabled afterward. GitHub Actions compiles and runs
this before normal firmware builds.

## Physical confirmation to collect

After installing the ILI9341 v2 app-only update, take a clear photo
of the *same* map and Boeing/Ryanair aircraft image. The new firmware
should display natural blue sky, expected runway/plane colours and
legible roads, without bright cyan/magenta chroma artefacts.

The **source-level cause and code fix are confirmed**; real-world
pixel colours still require a physical CYD screen test. Also review
the map's overall cartographic style later, once colour fidelity
is verified — do not hide this root cause with arbitrary recolouring.
