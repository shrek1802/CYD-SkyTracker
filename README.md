# CYD SkyTracker ✈️

**A DIY live aircraft-spotting display for the ESP32 Cheap Yellow Display (CYD).**

> **Development status:** This repository is an early prototype, **not yet an
> aircraft radar**. Do not flash a development artefact expecting a working
> live map or automatic OTA updates.

<p align="center">
  <img width="480" alt="Example ESP32 Cheap Yellow Display board"
    src="https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/assets/1562562/76c3d481-2523-4b6f-881c-2e29f9368cd0">
</p>

*CYD hardware photograph from [witnessmenow's CYD repository](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display). Product display shown is for hardware identification, **not a screenshot of SkyTracker firmware**.*

## The goal

A gift-friendly, standalone aircraft spotting screen. It will show a **real
local map** centred on the recipient's home, nearby moving aircraft and a
touch-to-open details page with aircraft type, altitude, speed, route and
an aircraft photo where available.

The plan is to store **offline geographical map tiles on microSD**, with
firmware in ESP32 flash and live aircraft updates over 2.4 GHz Wi-Fi.
The owner sets their Wi-Fi network and home location from their phone via
a temporary Wi-Fi setup hotspot.

## Hardware

- **ESP32-2432S028R** (2.8-inch CYD; 320 × 240 ILI9341 TFT).
- **16 GB or 32 GB FAT32 microSD** for map tiles and image cache.
- USB data cable for initial programming and a USB 5 V supply.
- Optional angled 3D-printed case.

Not all boards sold as "CYD" share the same pins or display. Only the
common ESP32-2432S028R is targeted initially.

## Development milestones

- [x] GitHub repository and PlatformIO skeleton
- [x] CYD screen boot/status page (source added; awaiting CI/hardware test)
- [x] Phone captive-portal Wi-Fi setup (source added; awaiting test)
- [x] SD mount and manual, SHA-256 checked SD-to-flash OTA code (awaiting test)
- [ ] Real local map from FAT32 SD card, multiple zoom levels
- [ ] Wi-Fi ADS-B flight positions and aircraft markers
- [ ] Touch aircraft -> flight details and photo
- [ ] Postcode-based phone setup, look-direction and distance
- [ ] Signed, automatic GitHub Release download to SD card
- [ ] Verified unattended/one-tap installation with recovery
- [ ] Public ESP Web Tools one-click browser installer

## For developers

This development build is **not suitable as a gift yet**. Work is proposed
on the `development/sd-ota-foundation` branch.

To compile using [PlatformIO](https://platformio.org/):

```bash
pio run -e cyd_2432s028r
```

GitHub Actions also compiles each change and saves a **development-only**
firmware artefact. A successful compile is not a hardware test and does not
make that image a supported public release.

The current startup page creates a uniquely named setup Wi-Fi hotspot,
shows its password on the CYD, and serves a phone configuration page at
`http://192.168.4.1`. The latitude/longitude fields are temporary until
postcodes and map selection are implemented.

### SD card update proof of concept

The **current test implementation does not download GitHub updates**.
It supports a *manual USB-serial test command* `INSTALL_SD` after an
image and its exact SHA-256 digest have been placed on the microSD at
`/updates/firmware.bin` and `/updates/sha256.txt`.

Do **not** attempt this until the build and hardware/rollback behaviour
have been checked. SHA-256 is an integrity check, not publisher
authentication. Signed update metadata and safe unattended operation are
future milestones.

See [SD update architecture](docs/SD-OTA-DESIGN.md) and
[upstream code research](docs/CODE-RESEARCH.md).

## How installing will work when released

Visitors will open the GitHub Pages installer, plug the CYD into a
computer's USB port, choose **Install** in Chrome or Edge, then use
their phone for setup. Later updates will be staged on the SD card,
verified and installed over Wi-Fi without needing a computer.

**There is no public installable release yet.**

## Sources and credits

- [TheJinxNL/ESP32FlightRadar](https://github.com/TheJinxNL/ESP32FlightRadar)
  — MIT-licensed map/aircraft UI research.
- [theqkash/esp32flight](https://github.com/theqkash/esp32flight)
  — MIT-licensed flight-tracker feature research.
- [Eiswolf-BG/eiswolfs-flightradar-CYD](https://github.com/Eiswolf-BG/eiswolfs-flightradar-CYD)
  — read-only architectural reference. Its README has an ambiguous MIT
  placeholder and the repository has no formal LICENSE file. **No source
  code from this project has been copied.**
- [Espressif SD Update example](https://github.com/espressif/arduino-esp32/blob/master/libraries/Update/examples/SD_Update/SD_Update.ino)
  — official SD-to-flash update reference.

This project is independent and is not affiliated with aviation
tracking providers. Flight positions and photos depend on third-party
data availability and licensing.

## Licence

Original SkyTracker code in this repository is released under the
[MIT licence](LICENSE). Third-party code, when added, must preserve its
upstream notices and applicable terms.
