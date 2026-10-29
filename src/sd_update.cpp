#include "sd_update.h"
#include "board_profile.h"

#include <SD.h>
#include <FS.h>
#include <Update.h>
#include <mbedtls/sha256.h>

namespace {
bool hashFile(File& file, String& hexDigest) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    if (mbedtls_sha256_starts_ret(&ctx, 0) != 0) {
        mbedtls_sha256_free(&ctx);
        return false;
    }
    uint8_t buffer[1024];
    bool ok = true;
    while (file.available()) {
        const int got = file.read(buffer, sizeof(buffer));
        if (got <= 0 || mbedtls_sha256_update_ret(&ctx, buffer, got) != 0) {
            ok = false;
            break;
        }
    }
    unsigned char digest[32] = {};
    if (ok && mbedtls_sha256_finish_ret(&ctx, digest) != 0) ok = false;
    mbedtls_sha256_free(&ctx);
    if (!ok) return false;
    char out[65] = {};
    for (size_t i = 0; i < sizeof(digest); ++i) {
        snprintf(out + i * 2, 3, "%02x", digest[i]);
    }
    hexDigest = String(out);
    return file.seek(0);
}
}

namespace SdUpdate {
bool installFromCard(const char* binaryPath,
                     const char* sha256Path,
                     String& errorMessage) {
    // Development-only guard against accidentally using another board's
    // update. This unsigned marker does NOT authenticate the firmware.
    File board = SD.open("/updates/board-id.txt", FILE_READ);
    if (!board) {
        errorMessage = "Board ID file missing";
        return false;
    }
    String imageBoardId = board.readStringUntil('\n');
    board.close();
    imageBoardId.trim();
    if (imageBoardId != BoardProfile::id()) {
        errorMessage = "Firmware is for a different CYD model";
        return false;
    }

    File checksum = SD.open(sha256Path, FILE_READ);
    if (!checksum) {
        errorMessage = "SHA256 file not found";
        return false;
    }
    String expected = checksum.readStringUntil('\n');
    checksum.close();
    expected.trim();
    expected.toLowerCase();
    if (expected.length() != 64) {
        errorMessage = "Invalid SHA256 checksum";
        return false;
    }
    for (size_t i = 0; i < expected.length(); ++i) {
        if (!isxdigit(static_cast<unsigned char>(expected[i]))) {
            errorMessage = "Invalid SHA256 character";
            return false;
        }
    }

    File image = SD.open(binaryPath, FILE_READ);
    if (!image) {
        errorMessage = "Firmware file missing";
        return false;
    }
    const size_t imageSize = image.size();
    if (imageSize < 1024 || imageSize > ESP.getFreeSketchSpace()) {
        errorMessage = "Firmware is empty or exceeds OTA partition";
        image.close();
        return false;
    }
    if (image.read() != 0xE9 || !image.seek(0)) {
        errorMessage = "Not an ESP32 application image";
        image.close();
        return false;
    }

    String actual;
    if (!hashFile(image, actual) || actual != expected) {
        errorMessage = "Firmware SHA256 mismatch";
        image.close();
        return false;
    }

    // The SHA file only detects corruption. It is NOT authentication:
    // anyone able to replace the SD contents could replace both files.
    if (!Update.begin(imageSize, U_FLASH)) {
        errorMessage = String("Update.begin failed: ") + Update.errorString();
        image.close();
        return false;
    }
    const size_t written = Update.writeStream(image);
    image.close();
    if (written != imageSize) {
        Update.abort();
        errorMessage = "Incomplete firmware write";
        return false;
    }
    if (!Update.end()) {
        errorMessage = String("Update.end failed: ") + Update.errorString();
        return false;
    }
    return true; // Caller must reboot after successful installation.
}
}
