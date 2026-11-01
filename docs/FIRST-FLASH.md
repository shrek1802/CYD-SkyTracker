# CYD SkyTracker — first hardware test (NOT a public release)

This test firmware has **live nearby aircraft icons and a touch details
screen**. If a compatible offline map pack is on SD, aircraft render over
that real map. Otherwise the map shows a simple grid with aircraft.

**Not yet included:** aircraft photos, routes, postcode lookup, unattended OTA,
signed updates, automatic regional map-pack downloads. Read the [hardware
compatibility guide](HARDWARE-COMPATIBILITY.md) before choosing a firmware.

## 1. Get the correct experimental firmware

Open [GitHub Actions](https://github.com/shrek1802/CYD-SkyTracker/actions/workflows/build.yml)
and choose the latest **successful** run for the development pull request.

Under **Artifacts**, download the ZIP named for your **exact CYD**,
for example:

- `skytracker-dev-cyd_2432s028r` for classic 2.8" ILI9341 only
- `skytracker-dev-cyd_2432s028r_v2` for separately identified ILI9341 v2
- `skytracker-dev-cyd_2432s028r_st7789` for the ST7789 2.8"
- `skytracker-dev-cyd_2432s024r` for 2.4" resistive
- `skytracker-dev-cyd_3248s035r` for 3.5" resistive

Each ZIP contains a **merged factory image** with bootloader, partitions,
OTA boot selector and application, and a smaller OTA-only `firmware.bin`.
For initial installation use the **factory** image, not `firmware.bin`.

These builds are compiled but have **not been tested on all physical
variants**. Flash only to a device you can recover via USB.

## 2. Flash from Ubuntu

Install esptool in its own virtual environment:

```bash
sudo apt update
sudo apt install -y python3-venv
python3 -m venv ~/skytracker-flasher
~/skytracker-flasher/bin/pip install 'esptool==4.8.1'
```

Unzip the correct CI artifact. In a terminal in the extracted folder,
plug the CYD into USB **with a data cable** and inspect the serial port:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

Use the correct port (example: `/dev/ttyUSB0`) and exact extracted filename:

```bash
~/skytracker-flasher/bin/python -m esptool --chip esp32 \
  --port /dev/ttyUSB0 --baud 460800 write_flash 0x0 \
  skytracker-cyd_2432s028r-factory.bin
```

Replace **both** the port and image filename when different. The factory
image installs a partition table and can erase/reset earlier device
configuration. Back up anything important before flashing. It will not
alter the physical hardware, but a wrong display image may result in a
blank or unusable screen until flashed again with the correct one.

If the serial port is busy, close PlatformIO serial monitor, OpenCode
serial sessions or other terminal applications. Do not use `sudo` to flash
by default; use your proper `dialout` permissions on Ubuntu.

For subsequent updates use only validated, matching-board OTA builds.
Do **not** use the experimental `INSTALL_SD` USB serial command on your
gift device; that test updater is not yet signed and tested for rollback.

## 3. First-time phone setup

After reboot the CYD should display a temporary network name of the form
`SkyTracker-xxxxxx`, its setup password, and `http://192.168.4.1`.
Connect your phone to the named 2.4 GHz setup Wi-Fi and open that address.

Enter your home Wi-Fi SSID/password and the desired **latitude/longitude**.
UK postcode lookup is planned but isn't in this first test. After saving,
the device restarts and connects to the home router.

Once connected, it performs a 3-point touch calibration if not previously
calibrated. Tap each cross. A timed-out calibration can be re-run by sending
`CAL_TOUCH` and Enter over USB serial at 115200 baud.

The screen should then show the map/grid and **nearby aircraft fetched
over Wi-Fi**, refreshing about every 30 seconds. Tap an aircraft symbol to
see a details page, then tap again to return to the map.

- `+` / `-` zoom, `HOME` recentres.
- Drag the map to pan.
- Tap `SKYTRACKER` top-left to reopen phone setup.
- A missing map tile displays a grid; this is intentional.

**No aircraft?** Check that your setup location is accurate, internet access
works and there are actually aircraft within 60 nautical miles. The display
can only show aircraft currently covered by the public ADS-B feed; delayed
or incomplete aircraft positions are normal. A completely blank map but
correct UI may mean the SD card or map tiles are missing, not a network bug.

## 4. Optional: a real geographical map on SD

Use the [offline map-pack instructions](OFFLINE-MAPS.md) to convert a
permitted raster MBTiles package to `/maps/9/x/y.rgb565` etc.
The map opens at zoom 9, so include that level. Never scrape the public
OpenStreetMap tile server for offline storage.

A **16GB FAT32** card is suitable for local map packs. The current test
firmware still runs with no SD installed, displaying aircraft over a grid.

## 5. Send useful test feedback

For hardware testing report exact board number and screen driver,
number of USB ports, SD card behaviour, Wi-Fi result, whether three
touch crosses worked, whether moving aircraft show, and any serial
errors from a 115200-baud log.

Do not share your Wi-Fi password, personal home coordinates or serial
MAC address in a public GitHub issue.

## Status

This is an initial test build, **not the gift-ready version**. Verified
functions will be recorded only after real hardware tests.
