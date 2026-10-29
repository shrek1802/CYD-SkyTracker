# CYD SkyTracker hardware compatibility plan

Updated: 28 October 2026.

**Compilation is not complete hardware verification.** One screen configuration
has been **physically confirmed** by a SkyTracker tester: dual-USB
ESP32-2432S028, ILI9341, HSPI, inversion ON, 27 MHz. The isolated
display-only diagnostic cycled red, green, blue, black, white and rendered
test text. **This does not verify the full aircraft radar, SD card or
touchscreen yet.** Other board combinations remain untested.

## Firmware build matrix (ESP32 Classic; Sunton SPI boards)

| PlatformIO environment | Board | Panel | Touch controller | Build profile |
|---|---|---|---|---|
| `cyd_2432s028r` | ESP32-2432S028R, classic 2.8" | ILI9341 | XPT2046 | Added |
| `cyd_2432s028r_v2` | Dual-USB ESP32-2432S028, 2.8" | ILI9341 + HSPI + inversion ON | XPT2046 | **Display-only diagnostic PASS**; integrated firmware untested |
| `cyd_2432s028r_st7789` | ESP32-2432S028Rv3, 2.8" | ST7789 | XPT2046 | Experimental |
| `cyd_2432s024r` | ESP32-2432S024R, 2.4" | ILI9341 | XPT2046 | Experimental |
| `cyd_3248s035r` | ESP32-3248S035R, 3.5" | ST7796 | XPT2046 | Experimental |

Each profile currently compiles the shared *prototype startup and phone setup*.
It does **not** yet operate touch input, maps, photo displays or ADS-B
tracking. For the v2 ILI9341 variant, a custom panel initialisation may be
needed, even though a separate compile target is already present.

These five Sunton board definitions are sourced from documented hardware pins
and all share **SD CS5, CLK18, MISO19, MOSI23**. Backlight is GPIO21 on 2.8",
GPIO27 on the 2.4"/3.5". Screen size and panel drivers are build-specific.

**SPI arbitration fix (October 2026):** the physically confirmed dual-USB
ILI9341 profile uses TFT_eSPI `USE_HSPI_PORT` on GPIO14/13/12, CS15,
DC2, GPIO21 backlight. It requires `TFT_INVERSION_ON`, 27 MHz and
`LOAD_GLCD`. Its microSD uses **VSPI** on GPIO18/19/23/5.
The previous app used **HSPI for both TFT and SD**, causing bus
reconfiguration during SD initialisation and a likely white screen.
The shared application now assigns SD to the *opposite hardware SPI
controller* when `USE_HSPI_PORT` is selected.

This conclusion is based on a successful display-only test and code review;
the full corrected application still requires hardware confirmation.

Do not assume that a two-USB board is always ST7789: both ILI9341 and ST7789
two-port examples have been reported. Confirm your panel before flashing.

## Additional board families (planned, not built)

- ESP32-2432S028C (2.8-inch capacitive) — touch pins/driver differ.
- ESP32-2432S032R/C (3.2-inch) — ST7789 and variant-specific touch.
- ESP32-3248S035C (3.5-inch capacitive) — GT911 touch.
- Guition `JC2432W328`, `JC3248W535` and related clones — **different wiring**.
- ESP32-S3 4.3-inch, 5-inch and 7-inch RGB panels — distinct build framework,
  PSRAM use, RGB display backend and likely different partition schemes.
- Round ESP32-C3 models — distinct MCU/graphics/touch and map layout.
- No-touch ("N") models — must support phone-only aircraft selection.

Do not assume every yellow-board ESP32 has a microSD slot, OTA support or enough
flash/PSRAM for photo rendering. Introduce limited functionality for boards
that cannot support the complete experience.

## Portable code architecture

```text
src/
  board_profile.h        Compile-time hardware ID, pin assignments
  display/               Planned generic draw/resize/touch interface
  maps/                  Planned SD tile renderer and projection
  flights/               Planned ADS-B API and aircraft data model
  web/                   Planned phone setup and updater
  sd_update.cpp          Manual developer update from microSD (prototype)
```

All non-hardware code must be independent of screen pixels and touch
controllers. Use resolution-aware layout and graphics primitives, with
bounded RAM for tiles/photo decoding. Keep OTA payloads variant-specific.

## Installation / OTA safety

The future browser installer will show a **board selector with identification
photos** and flash only that variant's bootloader/partition/firmware images.

Never automatically assume the model from the device's Wi-Fi MAC or USB chip:
different CYDs may use the same ESP32 chip and USB serial adapter. In practice,
the panel controller often cannot be reliably detected via software.

Once installed, store the exact board profile ID in firmware and require:
- matching SoC + flash geometry/partition layout + board ID;
- a signed release manifest and a cryptographically verified firmware image;
- space for inactive OTA partition and safe download staging on SD;
- tested reboot/rollback, preserving user Wi-Fi/map settings.

**Current dev SD installer:** requires `/updates/board-id.txt` (the exact
profile ID), `/updates/sha256.txt` and `/updates/firmware.bin`. The board
marker and SHA checksum are not signed. This remains USB-serial-only and
unsuitable for automatic/unattended OTA.

## References

- [witnessmenow CYD repo](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)
- [Sunton board definitions](https://github.com/rzeldent/platformio-espressif32-sunton)
- [CYD family overview](https://www.atomic14.com/esp32/boards/cheap-yellow-display/)

Some relevant third-party board-driver libraries are GPL-licensed, so do not
silently incorporate them into our MIT repository without resolving licence
compatibility. Board *facts* and pinouts can be implemented independently.
