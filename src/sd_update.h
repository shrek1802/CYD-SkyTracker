#pragma once
#include <Arduino.h>

// Development-only SD -> inactive OTA partition installer.
// This module does NOT download from GitHub and must NOT be used for
// unattended updates until release signatures/metadata are implemented.
namespace SdUpdate {
    bool installFromCard(const char* binaryPath,
                         const char* sha256Path,
                         String& errorMessage);
}
