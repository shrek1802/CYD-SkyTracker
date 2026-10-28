# SkyTracker aircraft photo card (development)

The flight-details screen can request a photograph when an aircraft is tapped.

## How it works

- Uses the **registration** supplied by the existing live ADS-B feed.
- Searches Wikimedia Commons for file titles containing that **exact**
  registration (e.g. `G-EUUK`). An aircraft with no registration will
  show a clear "No registration" message.
- Only JPEG images with recognised freely reusable Creative Commons
  / public-domain metadata are accepted.
- Uses a low-resolution Wikimedia thumbnail, never a full-size photo.
- Displays a visible photographer credit, licence and Wikimedia Commons
  source below the photo. A full source URL is printed to the USB
  serial diagnostic log when an image loads.
- Keeps the thumbnail in temporary RAM **only while the details screen
  is open**. No permanent copying of third-party images to SD.
- Uses a 90KB image cap, memory safety margin, bounded HTTP timeouts,
  strict upload.wikimedia.org host matching, and a reasonable failure message.
- Selected flight details remain on screen while aircraft data refreshes;
  the same photo is reused until you return to the map.
- Photos may take several seconds to load. If Commons has no suitable
  exact-registration, openly licensed JPEG, no image is displayed.

**Important:** Search matches are not guaranteed to show the aircraft
on the same date or in its current airline livery. Community metadata
can be incomplete. This photo feature is an experimental option
that has not yet been tested on actual CYD hardware.

## Source and licensing choices

The licensed-photo research found that Planespotters.net's terms
restrict copying and caching photographs without author permission.
For now the integrated feature therefore uses an open-licence source
rather than saving Planespotters photos to SD.

Sources:
- <https://commons.wikimedia.org/wiki/Commons:Machine-readable_data>
- <https://www.planespotters.net/legal/termsofuse>
- <https://github.com/Bodmer/TJpg_Decoder>

The thumbnail parser uses ArduinoJson and a separate ESP32 JPEG
decoder. Only images with accepted metadata are shown. If a photo
cannot be shown legally or technically, the normal aircraft details
remain available without an image.

## Further improvements

- Physical hardware test of the photographer-credit layout and JPEG
  memory use.
- Link or QR code on the device for the exact Commons file page and
  applicable licence details.
- Validated SD photo cache of images with permission or compliant
  open-licence attribution, for faster repeated views.
- Improved aircraft metadata by registration and ICAO24, without
  guessing when no exact match exists.
