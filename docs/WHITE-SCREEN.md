# Troubleshooting: white screen on dual-USB CYD

The 2.8-inch ESP32-2432S028/CYD2USB (USB-C + micro-USB) ships with
**more than one display controller**. The number and type of USB ports do
not identify the controller. The backlight may remain white if the wrong
TFT driver was flashed, even if the ESP32 successfully connects to Wi-Fi.

## Triage

1. Does the SkyTracker logo appear **at any point**, or is the LCD fully
   white from power-on?
2. Did the installer use the `cyd_2432s028r_st7789` profile?
3. If the splash has **never** appeared, use the ILI9341 v2 profile
   (`cyd_2432s028r_v2`) next; many dual-USB boards use ILI9341.
4. If the splash **does** appear and later goes white, do not blindly
   switch drivers. Capture the 115200 baud serial log because the issue
   could be SD/touch startup, Wi-Fi or a reset.
5. If the alternative still fails, try the classic ILI9341 profile only
   after reviewing the display/backlight configuration; v2 currently
   shares most display driver settings with classic ILI9341.

### Preserve saved Wi-Fi when switching panel profiles

For a device that was **already factory-flashed with SkyTracker's first
test firmware and standard 4MB min_spiffs partition scheme**: flash a
matching app-only `firmware.bin` to **0x10000**, NOT the all-in-one
factory image. This preserves the NVS configuration partition, including
Wi-Fi information. This is only valid before a subsequent OTA update has
switched the active app partition, and only with the expected layout.

From the extracted hardware-specific GitHub Actions ZIP:

```bash
# Replace /dev/ttyUSB0 with the actual USB serial port.
~/.local/share/cyd-skytracker/esptool-venv/bin/python \
  -m esptool --chip esp32 --port /dev/ttyUSB0 \
  --baud 460800 write_flash 0x10000 firmware.bin
```

Use `esptool` installed through the existing Ubuntu flash script.
If the above assumptions are unknown, identify the current flash/OTA
layout first instead of blindly overwriting the application.

**Do not post SSIDs, Wi-Fi passwords, home coordinates or full MAC
addresses in public issues or logs.**

References:

- [CYD community troubleshooting](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/TROUBLESHOOTING.md)
- [ST7789 CYD panel and alternative ILI9341 variant](https://lcdwizard.com/boards/cyd-2432s028r-st7789)
- [White screen from wrong controller](https://www.nmminer.com/2025/12/01/flash-correct-nmminer-to-cyd/)
