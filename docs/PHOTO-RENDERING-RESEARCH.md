# Aircraft photo rendering: evidence-based comparison and SD-first architecture

Reviewed: 28 October 2026. This is a research and implementation note,
not proof of successful display on the user's physical CYD board.

## Problem

SkyTracker's initial aircraft-photo viewer showed "No freely licensed
photo" for many or all aircraft despite the working live ADS-B positions.
We found that earlier lookup code rejected valid Commons thumbnail hosts
(`thumb.wikimedia.org`); the subsequent search improvements still did
not establish whether image loading, SD mounting or screen decoding works.
The first program tried too many dependencies at once.

## Open-source approaches inspected

1. **TheJinxNL/ESP32FlightRadar** — `src/display/map_background.cpp`:
   HTTP JPEG is decoded *once* by `TJpg_Decoder` into a preallocated
   RGB565 array, with explicit log lines for HTTP bytes, heap, decoded
   pixels and display installation. The produced RGB565 pixel array is
   a separate screen asset. This is a useful separation of decoding and
   presentation, although its API and board pins differ from SkyTracker.
2. **Bodmer/TFT_eSPI**, `examples/Generic/ESP32_SDcard_jpeg`, and
   **Bodmer/TJpg_Decoder**, `examples/SD Card/SD_Jpg`:
   show explicit SD initialisation, file-open verification, decoder
   status and output callbacks. These are reference implementation
   examples, not proof that the SkyTracker SD bus is mounted.
3. **dpoler/adsb** — `src/ui/detail_card.cpp` plus `README.md`:
   implements aircraft metadata and image rendering on the Pi build,
   while its ESP32-P4 build currently disables aircraft photo drawing
   due to PSRAM/image-cache corruption. Highlights that correct
   metadata and downloaded bytes are insufficient by themselves.
4. **betty02/esp32-flight-radar** — the README describes querying a
   third-party plane photo API from the selected aircraft. Useful
   lookup concept; it is not independently verified here as a
   successful JPEG drawing path on a physically tested CYD.
5. **portapack-mayhem/mayhem-firmware** — useful for aircraft trails,
   metadata and map markers, but not a drop-in photo decoder for our
   ESP32 CYD. No copyright-restricted Mayhem code/assets copied.

Sources:
- https://github.com/TheJinxNL/ESP32FlightRadar/blob/main/src/display/map_background.cpp
- https://github.com/Bodmer/TFT_eSPI/blob/master/examples/Generic/ESP32_SDcard_jpeg/ESP32_SDcard_jpeg.ino
- https://github.com/Bodmer/TJpg_Decoder/blob/master/examples/SD%20Card/SD_Jpg/SD_Jpg.ino
- https://github.com/dpoler/adsb/blob/main/src/ui/detail_card.cpp
- https://github.com/betty02/esp32-flight-radar
- https://github.com/portapack-mayhem/mayhem-firmware

## Chosen design for our dual-USB ILI9341/HSPI CYD

**Pre-render aircraft photographs off-device**. `tools/build_aircraft_photo_pack.py`
retrieves Commons-licensed representative aircraft JPEG thumbnails
and their photographer/credit metadata. The new
`tools/build_aircraft_photo_rgb565.py` converts them to a separate
`/aircraft/B738.rgb565` file with EXACTLY 144×96×2 = **27,648 bytes**
stored as little-endian RGB565. Retain `/aircraft/B738.txt` for
photographer, licence and source URL.

Unlike the previous approach, tapping a plane in the model pack does
**not** open any HTTPS request, allocate a JPEG buffer or invoke a JPEG
decoder. It opens the validated `.rgb565`, reads one 288-byte row,
and calls `TFT_eSPI::pushImage(...,144,1,row)`, which is also the
method already used by `src/map_view.cpp` for offline map rows.

The regular JPEG and live Commons lookup paths are still retained as
backwards-compatible fallback for models absent from the SD pack;
they are **not** considered proven, and should not be the primary
path for a small non-PSRAM CYD.

## One decisive physical test: PHOTO_TEST

The new `PHOTO_TEST` serial command bypasses ADS-B aircraft selection,
aircraft-type parsing, JPEG decoding and internet media requests:

1. At boot, check serial at 115200 for `[BOOT] SD mounted: YES`.
2. Copy the curated `aircraft/` folder containing `B738.rgb565`
   and `B738.txt` to the root of the FAT32 microSD.
3. Open USB serial at 115200 and send `PHOTO_TEST` + newline.
4. The CYD attempts a single known B738 model photo. It logs the
   exact SD path, size, metadata validity and draw result, and
   shows the test image for eight seconds before returning to radar.

Interpretation:
- **SD mounted: NO**: physical SD bus, card, filesystem or pin issue.
- **Missing attribution / zero-byte image**: incorrect path or card copy.
- **RGB565 size mismatch**: old JPEG pack or corrupted file.
- **SD RGB565 READY; row drawing SHORT READ**: SD I/O problem.
- **RGB565 drawing PASS; photo invisible**: display pixel order/window,
  TFT ownership, or panel issue. Test known pixel colours.
- **PHOTO_TEST works but tapping an aircraft still has no photo**:
  type-code mismatch or missing pack for that exact type; log type.
- **No matching type available**: no misleading generic photo; explicitly
  report the missing map and expand the curated model pack.

This is a reproducible diagnostic, not a claim that physical testing
has already passed.

## Licensing and safety

The pack uses Wikimedia Commons files with corresponding photographer,
open-licence and original source. Representative aircraft photos
MUST NOT be labelled as the exact tracked registration. Do not copy
another project's implementation/assets without checking its licence.
Application-only flashing preserves NVS/Wi-Fi if the partition layout
matches; avoid full flash erasure.
