# RGB565 display diagnostic (ILI9341 v2 / HSPI)

The source fixes RGB565 byte order already, but actual panel results must be
photographed and compared. This diagnostic isolates the transport from image
processing and keeps the user's Wi-Fi, settings and offline SD maps untouched.

1. Run `python3 tools/build_colour_check.py --output /tmp/SkyTracker-Colour-Test`.
2. Copy `diagnostics/rgb565-check.rgb565` to the same folder on the microSD.
3. Install only the app firmware for the *confirmed* `cyd_2432s028r_v2`
   board profile, with the existing partition-layout check and backup.
4. Open a 115200 baud serial monitor, enter `COLOUR_TEST`, and compare the
   top native TFT swatches with lower SD-streamed swatches. Photograph the screen.
5. Compare with `rgb565-check-reference.png` on your computer.

Order left to right: red, green, blue, white, yellow, cyan, magenta, black.
If *only* the lower row differs, inspect `setSwapBytes` and SD byte order.
If *both* differ, inspect RGB/BGR colour order and the actual display profile.
Photo contrast and colour adjustment are intentionally disabled in the
RGB565 converter to avoid hiding transport errors.

The map renderer now preserves existing SD tile colours without an additional
live desaturation filter. This does not replace the current map pack.
Device-specific TFT inversion ON and HSPI settings are unchanged.
No physical colour pass is claimed until the user's screen photographs agree.
