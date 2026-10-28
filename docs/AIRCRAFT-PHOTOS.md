# Aircraft photo details — representative type-first lookup (development)

**Current behaviour:** aircraft photographs on the touch detail screen now use
the ADS-B **ICAO aircraft type** rather than assuming every airframe registration
will have an openly licensed photograph. A type photo is explicitly marked
**"Representative type photo"**. It may show a different operator, livery,
registration or individual aircraft from the one you're tracking.

## Photo selection and storage

1. On tap, SkyTracker reads the received ADS-B `t` type code (e.g. `B738`,
   `A20N`, `B38M`). It maps supported codes to model names, without guessing
   unsupported types.
2. If microSD contains **both** `/aircraft/B738.jpg` and
   `/aircraft/B738.txt`, it checks image dimensions, file-size cap,
   and the licensing attribution, then uses it straight from the SD card.
   This works without any further image-server requests.
3. If an SD image is not available and Wi-Fi is connected, it searches the
   matching aircraft model category on Wikimedia Commons, then searches by
   model name. It accepts a licensed JPEG and fetches a small thumbnail.
4. If aircraft type is unavailable/unknown, it can still fall back to an
   exact-registration lookup, with no generic or potentially misleading photo.
5. The screen displays the photo credit and licence and prints its full
   original Wikimedia file page to serial diagnostics.

**This does not change our live ADS-B feed or offline map format.**
It does not add an SDR ADS-B radio receiver to the ESP32.

## Supported type codes

The lookup table `src/aircraft_type_photo.h` includes:
`A319`, `A320`, `A20N`, `A321`, `A21N`, `A333`, `A359`,
`B737`, `B738`, `B739`, `B38M`, `B39M`, `B744`, `B77W`,
`B788`, `B789`, `E190`, `E195`, `E290`, `AT76`,
`DH8D`, `CRJ9`.
The first optional photo pack only includes codes for which a suitable
freely licensed, correctly credited JPEG could be sourced; missing images
are skipped rather than substituted with unrelated aircraft.

## Optional offline photo pack: copying onto your SD

The **separate** GitHub Actions workflow `aircraft-photo-pack.yml` generates
a downloadable ZIP with a top-level `aircraft/` folder. Extract and copy
that folder to the same FAT32 SD card as the `maps/` folder:

```text
microSD/
  maps/
    ...
  aircraft/
    A320.jpg
    A320.txt
    B738.jpg
    B738.txt
    attribution.json
    README.txt
```

Each matching `.txt` file has three lines: photographer, short licence,
original Commons file-page HTTPS URL. Keep the attribution files whenever
copying or distributing JPEGs. This makes the photo pack available offline.
The displayed image is always marked representative, never presented as the
actual aircraft identified by live ADS-B.

The ZIP source data and build script:
- <https://github.com/shrek1802/CYD-SkyTracker/blob/development/sd-ota-foundation/tools/build_aircraft_photo_pack.py>
- <https://github.com/shrek1802/CYD-SkyTracker/actions/workflows/aircraft-photo-pack.yml>

## Limitations and diagnostics

Images are bounded to 90 KB JPEG plus a RAM safety margin. The ESP32
draws the scaled JPEG on the small 320×240 screen. Network requests have
timeouts and do not fetch arbitrary hosts. If no SD or online image is
available, the details remain visible and a status explains what failed.

The image helper currently uses TLS without CA validation for public,
read-only metadata/images and must be hardened before a public unattended
release. No image is used to configure the device or install firmware.

After flashing the experimental update, watch USB serial at 115200 while
tapping an aircraft. `[PHOTO]` logs show type mapping, SD-file results,
Wikimedia metadata, JPEG download status, free heap and decoder result.
Real photo display needs testing on the user's CYD; passing CI compilation
does not prove a successful on-device network photo download.

## Licence

Photos are sourced from Wikimedia Commons under accepted Creative
Commons/public-domain licences, each with source and creator attribution.
No photographs are copied from proprietary photo sites without permission.
Do not mix third-party photo licences into the SkyTracker MIT code licence.

- <https://www.mediawiki.org/wiki/API:Imageinfo>
- <https://commons.wikimedia.org/wiki/Commons:Reusing_content_outside_Wikimedia>
