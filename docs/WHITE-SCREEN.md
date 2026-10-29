# White screen / black line on the 2USB CYD

Symptoms: after phone Wi-Fi setup the CYD screen is white with a black line,
including when using both ST7789 and ILI9341 SkyTracker full-firmware variants.

**Do not assume a damaged display or keep factory-flashing different images.**

## First: capture the USB serial log without reflashing

From Ubuntu:
```bash
sudo apt install -y python3-serial
ls /dev/serial/by-id/* /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
python3 -m serial.tools.miniterm /dev/ttyUSB0 115200
```

Use the actual port. Press the CYD's RESET button while the terminal is open,
then copy the output (redact saved Wi-Fi passwords or location information).
Exit miniterm with **Ctrl+]**.

Pay special attention to reset reason, Guru Meditation errors, memory errors,
or whether code reaches the line `CYD SkyTracker development build - hardware:`.
A black line with a white screen alone does **not** prove a driver mismatch.

## Isolated hardware test

The repository builds 3 *small display-only diagnostics*:

- `diag_cyd2usb_st7789`: ST7789, BGR, inversion OFF
- `diag_cyd2usb_ili9341`: ILI9341, BGR, inversion OFF
- `diag_cyd2usb_ili9341_inverted`: ILI9341, inversion ON

They deliberately omit Wi-Fi, ADS-B, SD, map and touch code. Each cycles red,
green, blue, black and white approximately every 2.7 seconds. Black and white
frames also show geometric outlines and “CYD DISPLAY TEST”.

The tests use the known 2USB Sunton display pins:
**SCK14, MOSI13, MISO12, CS15, DC2, backlight21**, and enable
TFT_eSPI's HSPI option at a conservative **27MHz** for initial diagnosis.

If none of the three show colours/text but USB serial messages report
changing phases, suspect the actual panel/pin variant, screen ribbon
connection, incorrect SPI init/power, or a damaged panel. Do not infer the
panel from USB port count alone.

## Safe test flashing

**These `.bin` files are application-only; they are NOT initial-install
factory images.** Only use after the SkyTracker full factory image has
installed a compatible `min_spiffs.csv` partition layout.

For a board originally installed with our 4MB SkyTracker factory image, the
application in OTA slot 0 is at offset **0x10000**. Flashing just that offset
preserves NVS (including configured Wi-Fi), but prevents SkyTracker from running
until the original full application is restored.

If bootloader OTA selection currently points to a different OTA slot, writing
0x10000 might **not** activate the test. Check the ESP32 boot log first.

Use the diagnostic flashing script shipped with the test bundle.
Never erase flash or overwrite the partition table during this test.

## Result to report

1. Did any colour or text appear in **ST7789** or **ILI9341** diagnostic?
2. Does the black line stay in the same position?
3. Copy serial output at 115200 baud, especially reset errors and the
   diagnostic's `DIAG: phase=` messages.
4. Identify the exact printed PCB text and USB ports.

Once one display-only test works, copy its TFT_eSPI hardware flags into
the matching SkyTracker profile, then test SD/touch/Wi-Fi incrementally.
