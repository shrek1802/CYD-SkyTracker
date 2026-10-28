# SD-staged over-the-air update design

## Intended recipient experience

1. Radar checks **signed** release metadata periodically over HTTPS.
2. If a newer release exists for the exact CYD board and partition layout,
   a notification appears in the radar or phone settings.
3. Download firmware as /updates/firmware.part on microSD.
4. Stream-check file size, SHA-256 and a cryptographic release signature
   whose trust key is built into the installed firmware.
5. Rename the staged download to /updates/firmware.bin and mark it ready.
6. When installation is authorised (manual first; optional unattended later),
   write it from the SD card to the **inactive** ESP32 OTA application partition.
7. Reboot and confirm healthy startup. Preserve Wi-Fi, location and maps.
   Retain previous image for rollback where supported.

Download failures or loss of power during SD download must not interrupt the
currently running firmware. Never install a partial or unsigned download.

## What is actually implemented in this development branch

- Minimal CYD screen and phone captive portal (not yet flight tracking).
- SD mount using the standard 2432S028R pins.
- Manual USB-serial test command INSTALL_SD, requiring the local files:
  /updates/firmware.bin and /updates/sha256.txt, plus a matching
  /updates/board-id.txt (see hardware compatibility document).
- Local SHA-256 check and Update.writeStream() to the alternate OTA partition.
- OTA partition size check in GitHub Actions.

**NOT YET IMPLEMENTED:** GitHub firmware downloader, release signature
verification, hands-free installation, rollback health verification, aircraft
map or production release. SHA-256 by itself detects corruption, but does NOT
establish publisher identity if both the image and checksum can be modified.
Do not enable unattended firmware updates until signing is in place.
The current board-ID text file is only an accidental-mismatch guard and
can be falsified along with the checksum. Future signed metadata must bind
the exact firmware SHA-256 to the board, SoC and partition layout.

## GitHub Release plan

- CI must compile, run tests, check firmware size and produce an artefact.
- Release workflow publishes firmware.bin, initial-install merged image,
  versioned JSON manifest with board ID, partition scheme, length,
  SHA-256 and verified signature.
- A browser installer (ESP Web Tools, served by GitHub Pages) consumes a
  release manifest; it must not claim to install until a tested image exists.
- Never embed private API tokens or signing keys in public source.
- Secure use requires certificate validation, signature verification, and
  controlled redirect handling for GitHub's release asset downloads.

## Partition limit

The common CYD 4 MB layout has two app partitions: min_spiffs.csv
provides 0x1E0000 = 1,966,080 bytes each. The SD card does not extend
this internal firmware limit. A new app larger than the OTA partition
cannot update wirelessly even when the SD card has plenty of space.
