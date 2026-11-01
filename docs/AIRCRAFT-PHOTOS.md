# SkyTracker aviation UI (experimental)

## New touchscreen navigation

**Map -> Aircraft details -> Fullscreen photo.** On the radar, tap the
aircraft silhouette to open a detail card. The card has a smaller 152x64
thumbnail, registration, aircraft type, altitude, speed, distance and track,
with a **VIEW PHOTO** button. Tap the thumbnail or the button to open a
**320x166 full-width aircraft picture**. From the large picture choose
**< DETAILS** or **BACK TO MAP**. The details screen also has its own
**BACK TO MAP** control. Accidental taps elsewhere don't exit the card.

The RADAR map has slim top and bottom bars, smaller stacked + / - / H
controls, a muted colour treatment that also works with the existing
`/maps/` offline tile pack, clearer status labels and the mandatory
OpenStreetMap contributor attribution. The previous no-whole-screen-refresh
aircraft redraw is preserved. During ADS-B refreshes the photo is not
repainted.

## Photo data on SD (new pack)

For the 2.8-inch 320x240 ILI9341, photos are generated on a desktop from
licensed Wikimedia Commons JPEGs and stored as **little-endian RGB565**
(one row at a time, no large image framebuffers):

- `/aircraft/B738.rgb565` — 304×128 px, 77,824 bytes, source for the small thumbnail
- `/aircraft/B738.full.rgb565` — 320×166 px, 106,240 bytes, dedicated fullscreen picture
- `/aircraft/B738.txt` — photographer, licence and original Commons page URL

This naming pattern repeats for other supported ICAO types. The old
`/aircraft/*.rgb565` files still work for the thumbnail; if a fullscreen
file is missing, the old 304x128 asset is centred without pretending it
contains more detail. Licence credit remains visible.

**Install both the new firmware and the new `aircraft/` folder**. Existing
`maps/` tiles, Wi-Fi setup, SD formatting, and live ADS-B decoding do not
need changing. Do not erase flash or NVS. The update script checks the
partition layout and backs up the current app.

Representative model photos may be from a different airline or aircraft
registration from the one being tracked. No exact-aircraft match is claimed.

## Firmware compatibility

The two-USB CYD 2.8-inch **ILI9341_2_DRIVER / HSPI / INVERSION_ON**
profile remains the physically confirmed display configuration.
All other hardware variants are CI compile targets only. The new
touchscreen layout and fullscreen photo have not yet been tested on the
user's physical CYD.

A user can test the page by selecting an aircraft for which an SD photo
exists, tapping the thumbnail, using both navigation buttons, and
waiting 30+ seconds for a telemetry refresh.

## Sources & reuse conditions

The representative aircraft photographs are downloaded from Wikimedia
Commons; each keeps its individual author/licence metadata and original
file page. Existing Commons permissions and attribution/share-alike
obligations still apply. Do not copy Mayhem bitmap assets or other
third-party artwork into this MIT-licensed project without licence review.

- <https://commons.wikimedia.org/wiki/Commons:Reusing_content_outside_Wikimedia>
- <https://github.com/shrek1802/CYD-SkyTracker/tree/development/sd-ota-foundation>
