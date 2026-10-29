# CYD SkyTracker RGB565/photo/map update — 29 October 2026

## Hardware and existing features are preserved

All changes are incremental to `development/sd-ota-foundation`.
The physically tested `cyd_2432s028r_v2` PlatformIO profile remains
**ILI9341_2_DRIVER / HSPI / TFT_INVERSION_ON / 27 MHz**. SD remains VSPI.
Wi-Fi credentials and location remain in NVS, and the existing ADS-B
reader, touchscreen calibration, offline maps, aircraft selection,
`Map → Details → Photo` navigation and app-only flasher remain intact.

## Source-level colour diagnosis

Offline map tiles and aircraft photographs are little-endian RGB565:
red is bytes `00 F8`. The TFT expects high byte first. The established
`SkyRgb565::pushRow` path uses `setSwapBytes(true)` before
`pushImage`, then restores false. This covers the full map, small
map repair rectangles, aircraft thumbnail and full-screen images.
The native `COLOUR_TEST` command checks RGB/BGR separately from SD
byte-order. **Real panel colours still need a user-provided hardware
photo before a physical colour PASS can be asserted.**

## What's new in this revision

- 68 candidate ICAO aircraft type entries, including cargo aircraft,
  helicopters and light aircraft. A 3-character type (e.g. `R44`)
  is now recognised instead of rejected.
- Reusable curated source-photo titles for bad automated results
  (B739 cabin, C17 interior, B753 engine, GLF6 engine, A35K sky).
  The builder rechecks Commons file, creator, licence and source URL.
  Auto-selected photos are explicitly labelled *automated_unreviewed*
  rather than falsely claiming human approval.
- Photo converter now preserves the entire airframe with letterboxing
  instead of stretching/cropping; it makes 304×128 and 320×166
  little-endian RGB565 in **the existing SD naming scheme**.
- Decoder round-trip verifies RGB565 red/green/blue channels against
  the source and creates `RGB565-original-vs-decoded-review.jpg`.
- Revised map style uses softened land, water, and road colours,
  stronger road hierarchy and less fine-footpath clutter at high zoom.
  Still 256×256, little-endian `/maps/z/x/y.rgb565`, compatible with
  the **existing SD tile reader**; copying new tiles is optional.
- Aircraft detail panel now expands a known ICAO type into a
  manufacturer and model across two compact lines.

## What is, and is not, proven

Unit tests and CI builds confirm known pixel byte order and source code
compilation only. The photo pack workflow validates licensed, credited
photos and checks lengths 304×128×2 and 320×166×2, but review of
individual images is still required: automatic filename and contrast
checks cannot guarantee recognizable aircraft framing. Wikimedia API
429 rate limiting may cause some target codes to be absent in an
individual run. Do not substitute a different aircraft type; display
the original silhouette instead.

## Install on the already-configured ILI9341 v2 CYD

1. Back up the current microSD before copying anything.
2. Download the newest *successful* `SkyTracker-Aircraft-Direct-RGB565-SD-Pack`
   artifact. Extract the nested `SkyTracker-Direct-RGB565-Aircraft-Photos.zip`
   and merge its `aircraft/` folder on the FAT32 card. **Do not format the
   SD or erase `maps/`.** Existing photos can be kept as backups.
3. Optionally replace only the `maps/` folder with a validated muted-map
   package. The tile storage format is unchanged. Never delete the old
   map pack until the new one has been physically checked.
4. To update firmware, use only the
   `skytracker-dev-cyd_2432s028r_v2` artifact from successful
   `Compile CYD firmware variants` CI. Extract `firmware.bin` and
   `partitions.bin` and run existing `tools/flash-app-ubuntu.sh`
   with its SHA256SUMS.txt checks. Its numbered-port selection,
   partition-layout check, readback backup and **app-only 0x10000
   write** safeguard protect NVS. Never flash the `factory.bin`
   to a configured device.
5. Run `COLOUR_TEST` over 115200-baud serial with the optional
   `diagnostics/rgb565-check.rgb565` on the card; compare top native
   TFT swatches to lower SD swatches. Then test a known B738 aircraft
   and the details/full-photo/back navigation, including a normal
   30-second ADS-B update.

Never claim a physical display PASS until the screen photograph agrees
with the desktop reference. No map software can fix an uncorrected TFT
byte-order fault.

## License

Per-aircraft `/aircraft/CODE.txt` retains creator, licence, Commons
source and optional licence URL. Photo modifications and redistribution
must comply with their individual licences, including share-alike
conditions. The map data is from OpenStreetMap contributors, ODbL.
