# Real offline maps: MBTiles → CYD microSD

This is a **development tool**, not a ready-made UK map download.
The firmware displays a true geographic raster map if suitable pre-rendered
tiles are present on its FAT32 microSD card.

## Requirements

- Ubuntu / Linux PC, Python 3.10+, Pillow (`python3 -m pip install Pillow`).
- A **raster** `.mbtiles` file containing your desired area and zoom levels,
  legally supplied by a source that **permits offline storage and redistribution
  as applicable**. Supported MBTiles tiles: PNG, JPEG, WebP, 256 × 256 pixels.
- A FAT32-formatted 16GB or 32GB microSD.

**Do not bulk-download OpenStreetMap's public tile servers.** They prohibit
offline tile downloading and bulk scraping. OpenStreetMap geographic data itself
is open data under ODbL; map tiles may have separate terms. You may render tiles
locally from properly licensed extracts, use a permitted offline raster MBTiles
provider, or create your own from licensed vector data. Retain provider attribution.

## On your Ubuntu desktop

```bash
python3 -m pip install --user Pillow

python3 tools/pack_maps.py \
  --mbtiles /path/to/licensed-local-maps.mbtiles \
  --output ~/SkyTracker-SD \
  --lat 52.9 --lon -1.2 \
  --radius-km 60 \
  --minzoom 9 --maxzoom 12 \
  --max-tiles 5000
```

The coordinates above are **example values**, not a recipient location.
Use the latitude/longitude of the person receiving the gift.

The command only works with **existing tile data inside** the provided
MBTiles file; it does not magically create missing zoom levels or fetch tiles.

If you have a large MBTiles file, the command only exports the selected area
for each supported zoom. Each tile is 256 × 256 × 2 bytes = **128 KiB**, so
1,000 tiles require approximately 125 MiB on the microSD, excluding filesystem
overhead.

## SD card result

```text
/maps/
  attribution.txt
  map-pack.json
  9/
    253/
      169.rgb565
  10/
    ...
  11/
    ...
  12/
    ...
```

Tile names are **XYZ**, not MBTiles/TMS: `z / x / y.rgb565`.
Pixels are row-major RGB565 little-endian. The ESP32 streams horizontal
rows into its screen without allocating a full tile in RAM.

Copy the entire `maps` folder to the FAT32 microSD root. Insert the card
**before switching the CYD on**. At present, the prototype opens the map
at zoom 10 and the provided home location. Missing zoom tiles show a
checkerboard/missing-tile warning; that's intentional rather than silently
using unrelated imagery.

## Development touch controls

Once a touchscreen is calibrated:

- **+** zoom in; **-** zoom out; **HOME** recentre on configured home
- Drag the map to pan
- Enter `CAL_TOUCH` over a USB serial monitor (115200 baud) to start
  the three-point touchscreen calibration. This step is temporary until a
  complete first-run touchscreen/phone calibration flow is developed.

An uncalibrated touchscreen **does not respond** rather than using a
dangerous inaccurate coordinate mapping. Calibration is saved in ESP32 NVS
and kept across ordinary firmware upgrades.

## Still to implement

- Automatic map downloads/SD packs selected by postcode during phone setup.
- Zoom selection limited to actual installed packs, and better SD read caching.
- Live aircraft overlays, touch-to-select details, aircraft pictures.
- Dedicated web setup for later Wi-Fi/location changes.
- Verified, signed SD-staged OTA with rollback/recovery.

## Source notes

Avoid offering downloadable packs containing someone else's raster map tiles
without a clear permission or redistribution licence. Attribution by itself
does not grant redistribution rights.
