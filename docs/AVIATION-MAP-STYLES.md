# SkyTracker offline aviation map styles

The dual-USB ILI9341 v2 CYD has a 320 x 240 pixel display and does not
have the PSRAM framebuffer used by the 7-inch ESP32-S3 weather display at
https://projecthub.arduino.cc/mircemk/esp32-weather-dashboard-with-satellite-maps-and-16-day-weather-forecast-efdc7e.

That weather example loads PNG XYZ tiles from CARTO Dark, OpenTopoMap,
and OpenStreetMap, then overlays weather/rain tiles; its basemaps are
not satellite imagery. Directly cloning its full-frame rendering system
would put far more strain on the small CYD's memory and Wi-Fi connection.

## Implementation

- **Day**: cleaner, pale OSM navigation cartography.
- **Night**: dark navy/charcoal aviation style with subdued green parks,
  blue water, visible motorways, locally labelled settlements and airports.
- Geofabrik county **vector extracts** are converted by
  `tools/build_geofabrik_maps.py` on an ordinary computer / in GitHub Actions.
  No bulk downloads from OSM public raster tile servers.
- Both themes retain the same 256 x 256 **little-endian RGB565** pixel
  encoding and Web Mercator xyz coordinates.
- The original `/maps/{z}/{x}/{y}.rgb565` remains the **day** style;
  `/maps/night/{z}/{x}/{y}.rgb565` is the additional **night** style.
  Original SD map packs and firmware settings remain compatible.
- Night theme becomes the default when the new night folder is present.
  The bottom right-hand map button is **D** (switch to day), or **N**
  (switch to night). Existing **+**, **-**, and **H** controls still work.
- On-the-fly aircraft plots and dirty-rectangle restores use whichever
  theme is active, so the 30-second aircraft poll does not cause a
  full-map refresh. Changing the theme manually redraws the background once.
- Map tiles stream a row at a time (less than 1 KiB); no full-map
  framebuffer, PNG decoding or weather-map HTTP fetch on the CYD.
- Preserve `/maps/attribution.txt` and map-screen OpenStreetMap
  contributor attribution (ODbL).

## SD installation

1. Back up the existing FAT32 card.
2. Extract the map ZIP and merge its `maps/` folder into the **root**
   of the SD card. Keep the existing `aircraft/` folder and settings.
3. Install the matching `cyd_2432s028r_v2` app-only firmware using
   the supplied Ubuntu installer, which checks the partition table,
   makes a backup and does not erase NVS.
4. Reboot; night mode should open by default. Tap **D** / **N** to switch.

These were compiled and validated in CI, but physical CYD colour and
touch performance must be confirmed by photographing the running display.
The source maps are geographically scoped around Nottinghamshire,
not a complete offline UK/world tile catalogue. Outside the installed
area or zoom levels the original missing-tile grid still appears.


## Wider-area zoom support (2026-10-29)

Previously the downloadable night/day pack contained only **zoom 9-13**.
Tapping minus again continued changing the map level, but no SD tiles
were present below zoom 9, so the screen fell back to missing-map tiles.

The independent wide-area builder `tools/build_wide_area_maps.py` creates
both themes for **zoom 5-8**, covering much of Britain and the surrounding
region. Its broader low-zoom land and major city layers come from Natural
Earth, which is **public domain**; the existing detailed zoom 9-13 tiles
remain based on OSM/Geofabrik and retain OpenStreetMap attribution.

The new download is deliberately an **add-on map ZIP**: extract and
**merge the maps/ folder at the microSD root**, keeping
`maps/9` through `maps/13`, `maps/night/9` through `maps/night/13`,
your `aircraft/` directory, and `maps/attribution.txt`.

The updated firmware checks whether the centre tile exists before
switching zoom. On an older SD pack, minus stops at zoom 9 rather
than showing a blank map. With this additional pack, minus reaches
zoom 5, while plus can return to zoom 13. The current pan area is
finite: outside the generated tiles, the missing-map background remains.

**Note:** At zoom 5-8 the map deliberately prioritises country coastlines
and major city labels rather than fine residential streets, which would
not be legible on a 320x240 display. The live ADS-B search radius is
configured separately from map zoom and does **not** automatically
grow when the screen zooms out.

## Future options

To reproduce a provider's exact CARTO/OpenTopo appearance rather than
an independent inspired style, obtain the provider's appropriate paid
or explicit offline redistribution licence. Do not bulk cache their
public raster endpoints as a workaround. Satellite/aerial imagery
requires separate source imagery and permission, and does not come
from the OSM vector extracts.
