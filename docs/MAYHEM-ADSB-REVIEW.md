# PortaPack Mayhem ADS-B evaluation for CYD SkyTracker

Reviewed: 28 October 2026. Upstream: <https://github.com/portapack-mayhem/mayhem-firmware>, default branch `next`.

## Hardware / transport differences — keep independent

PortaPack Mayhem runs with **HackRF/PortaPack SDR hardware**. Its
`firmware/baseband/proc_adsbrx.cpp` receives 1090 MHz radio samples and
extracts ADS-B Mode S frames. Its `firmware/common/adsb.cpp` decodes
callsign, position/CPR odd-even frames, velocities and other parameters.

**SkyTracker is a 2.8-inch ESP32 CYD without a 1090 MHz RF frontend.**
Our ADS-B position source is an existing HTTPS aircraft feed. The raw SDR
pipeline and baseband/DSP firmware do not run on our CYD and must not be
represented as extra data that can be obtained without hardware.

## Specific code/features inspected

| Upstream file | Grounded feature | SkyTracker action |
|---|---|---|
| `firmware/application/external/adsbrx/ui_adsb_rx.hpp` | Aircraft records keyed by ICAO24 with current/recent/old/expired states, confidence/position validity | **Consider** an explicit stale-aircraft indicator instead of abruptly disappearing |
| `firmware/application/external/adsbrx/ui_adsb_rx.cpp` | Recent aircraft list, details, per-plane map mode, up to 30 trail points, coloured altitude gradient | **Added independently:** persistent selected ICAO and bounded 20-point trail; consider altitude colour legend next |
| `firmware/application/ui/ui_geomap.cpp` | Geographical marker filtering, marker labels, and rotated *bearing arrows* | Current SkyTracker plane silhouette is more recognisable than this arrow: keep our independently drawn geometry, add small optional callsign labels later |
| `firmware/application/database.hpp` | Offline indexed aircraft and airline lookup on SD | **Potential future feature:** separate, legally sourced searchable database with aircraft make/model/manufacturer/operator/airline. Do not copy upstream binary databases |
| `firmware/tools/make_icao24_db/make_icao24_db.py` | Builds aircraft metadata from OpenSky CSV | Research upstream dataset rights and metadata freshness before making our own SD cache |
| `firmware/tools/make_airlines_db/make_airlines_db.py` | Builds airline-code lookup from a Planefence source | Consider small independently generated airline/ICAO-prefix lookup if legally permitted and useful |

### Accurate icon comparison

The Mayhem ADS-B **map** uses `GeoMap::draw_bearing`, which draws a
small rotated triangular arrow. Its ADS-B app menu has a separate
bitmap icon. SkyTracker now draws **a filled swept-wing plane
silhouette** for each aircraft, which is easier to recognise on
a 320 × 240 CYD map. No Mayhem glyphs or bitmap arrays are used.

### Offline map format

Mayhem references `/OSM/<zoom>/<x>/<y>.bmp` for its raster OSM map
view. SkyTracker uses `/maps/<zoom>/<x>/<y>.rgb565` with
256×256 little-endian RGB565 tiles. The two layouts are **not
interchangeable**; converting is required.

### Licence / provenance

The Mayhem repository includes a **GPL-3.0** root LICENSE; individual
ADS-B and UI source files carry GPLv2-or-later notices.
SkyTracker's own code is MIT-licensed. **Do not copy/paste Mayhem
C++ implementation, icon bitmaps, databases or source assets into the MIT
tree.** Concepts such as aging, trails, altitude colouring and fixed-size
indices can be implemented independently, but always check third-party
data rights for metadata.

## Work incorporated into our existing development branch

- Introduced SkyTracker-owned `FlightTrail` with a **fixed cap of 20
  points**, minimum 120 m movement to suppress jitter, no heap allocations
  and date-line-safe distance maths.
- Selected aircraft now survives successive 30-second API refreshes
  using its ICAO hexadecimal ID (previously lost selection every poll).
- Tapping a flight, then returning to the map, retains highlighting and
  renders its observed flight trail as a fine line.
- If the selected plane leaves feed coverage, the details view returns
  to the map instead of silently freezing.
- Added self-contained `g++` tests to GitHub CI for trail bounds,
  jitter rejection, and antimeridian behaviour.

Important: the trail only shows **positions SkyTracker actually observes
while a particular plane is selected**. It is not a full historical
flight path or a prediction, and starts from the position at selection.
Its accuracy and on-screen appearance need verification on real hardware.

## Next, in priority order

1. Finish touch-selection field test with the physical dual-USB
   ILI9341 / HSPI / inverted panel.
2. Optional callsign/registration labels on uncluttered maps.
3. A visually explained altitude colour scale and stale-data display.
4. Offline aircraft metadata database on SD, with compatible rights.
5. Aircraft photos and routes from authorised sources (distinct from
   raw ADS-B telemetry).
6. No SDR-specific decoders unless a separate RF frontend is added.

Sources:

- <https://github.com/portapack-mayhem/mayhem-firmware/blob/next/firmware/application/external/adsbrx/ui_adsb_rx.cpp>
- <https://github.com/portapack-mayhem/mayhem-firmware/blob/next/firmware/application/external/adsbrx/ui_adsb_rx.hpp>
- <https://github.com/portapack-mayhem/mayhem-firmware/blob/next/firmware/application/ui/ui_geomap.cpp>
- <https://github.com/portapack-mayhem/mayhem-firmware/blob/next/firmware/baseband/proc_adsbrx.cpp>
- <https://github.com/portapack-mayhem/mayhem-firmware/blob/next/firmware/tools/make_icao24_db/make_icao24_db.py>
- <https://github.com/portapack-mayhem/mayhem-firmware/blob/next/LICENSE>
