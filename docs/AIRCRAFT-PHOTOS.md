# SkyTracker aircraft photographs — larger, curated, SD-first

## What changed (29 October 2026)

The original photo card wasted screen space: images were displayed in a
150×99 region, with 144×96 pixels of actual aircraft photography. Some
first-search Wikimedia results showed an engine, gear, the sun, or only
part of an aircraft.

The new ILI9341 build reserves **304×128 pixels** at the top of the
320×240 details page — 304 columns rather than 144 — while leaving
registration, altitude, speed, distance, ICAO type, heading and creator
credit visible below the photograph. It renders direct little-endian
RGB565 rows from microSD (only **608 bytes per row**, 77,824 bytes per
on-disk image). This avoids live JPEG decoding for packed models.

The model-photo pack uses manually chosen Wikimedia Commons exterior
pictures for known problem models (Airbus A319/A320/A321, Boeing
737-800 and ATR 72-600), and verifies MIME/licence/creator and URL.
Photographs are **representative aircraft models**, not the real
registration or necessarily the present airline. Attributions and
original file-page URLs are provided.

## Installation

Copy the ZIP's `aircraft/` folder to the **root** of the FAT32 microSD
alongside the existing `maps/` folder:

```text
microSD/
    maps/...
    aircraft/
        B738.rgb565
        B738.txt
        A320.rgb565
        A320.txt
        ...
```

Each `.txt` contains the photographer, licence and original Commons
file link. The creator and licence appear on the details screen.

**The new firmware REQUIRES the new 304×128 RGB565 pack**. The previous
144×96 images are intentionally not accepted as direct-render photos
because they would show a smaller/cropped image. Keep both the new
firmware and map/data ZIP backups.

Not all ICAO type codes have a prepared picture. The model lookup is
`src/aircraft_type_photo.h`; a missing model is not substituted with
an unrelated aircraft. When a prepared SD image is absent, the older
licensed-JPEG fallback may be attempted over Wi-Fi but is not the
recommended rendering path.

The `PHOTO_TEST` serial command now expects a 77,824-byte B738 RGB565
file and shows it for eight seconds. `VERSION` verifies the active
firmware. The live aircraft feed and map packs are not changed.

## How the pictures are sourced

- `tools/build_aircraft_photo_pack.py` — retrieves an open-licence
  aircraft-exterior photograph and metadata from Wikimedia Commons.
- `tools/build_aircraft_photo_rgb565.py` — crops/sizes to 304×128,
  applies restrained sharpening, emits little-endian RGB565 rows.
- `.github/workflows/aircraft-photo-pack.yml` — validates image dimensions,
  creator credits, source URLs and file sizes.

All photos are the copyright of their individual Commons creators
and carry their **own licences**. SkyTracker software remains MIT.
When distributing altered CC BY-SA photos, comply with their
attribution and share-alike conditions.

## Known limitations

The 320×240 ILI9341 is not a high-resolution photographic display, so a
304×128 picture still has finite detail. Both the revised full-width
details card and source image quality need an actual screen photograph
from the working CYD to validate the final visual result.

Do not copy proprietary planespotter images without permission.
This experimental firmware currently allows unverified TLS for public
read-only image fallback and is not a production-security release.

- https://commons.wikimedia.org/wiki/Commons:Reusing_content_outside_Wikimedia
