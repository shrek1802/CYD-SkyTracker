# Research notes: upstream flight-radar firmware

Checked 28 October 2026. These are implementation observations, **not copied
source code**. Do not transplant upstream code without confirming permission.

## Eiswolfs CYD firmware
Project: https://github.com/Eiswolf-BG/eiswolfs-flightradar-CYD

Inspected: platformio.ini, src/ota_update.cpp, src/sd_storage.cpp,
src/wifi_manager.cpp, src/wifi_setup_screen.cpp, src/adsb_client.cpp,
src/touch_input.cpp, src/aircraft_details.cpp, src/world_map.cpp, README.md,
and the directory listing of src/radar_screen.cpp.

Useful architecture and hardware facts:
- The standard 2.8-inch CYD uses an ILI9341 display on 14/12/13/15/2,
  with backlight GPIO21, and SD on a separate SPI bus
  SCK18/MISO19/MOSI23/CS5.
- The project separates ADS-B fetching, aircraft models, touchscreen handling,
  SD storage and firmware update logic.
- Its OTA implementation checks GitHub releases and installs directly over
  HTTPS. We will instead **download and verify on SD first**.
- Its partition configuration uses min_spiffs.csv with two approximately
  1.875 MiB OTA application slots.
- Its built-in "world map" is not the geographic road/town map required here;
  the new SD tile renderer must be developed separately.

## Permission and licence caution
The upstream README currently contains "MIT (or add your preferred license
here)", but the repository has no LICENSE file and GitHub does not detect a
licence. That placeholder is ambiguous. Until clarified with its maintainer,
do not copy/paste its implementation into our public repository.

Independently implement the required behaviour or use correctly attributed
code from MIT-licensed projects:
- https://github.com/TheJinxNL/ESP32FlightRadar
- https://github.com/theqkash/esp32flight

Check the licence of **each** third-party library, map style, imagery source,
aircraft photograph provider and flight-data service before distribution.

## Components to build next
1. SD map tiles with bounded RAM, north-up map, location-to-pixel projection.
2. ADS-B feed client with sane fetch intervals and missing-data handling.
3. Aircraft markers, selection hit-test and full-screen details.
4. Photo thumbnail caching in SD (respecting source permissions).
5. Phone settings with UK postcode lookup and GPS coordinates.
6. Verified SD-staged, signed OTA downloader; failure recovery.
