#pragma once
// Independently authored ICAO aircraft-type lookup used by the photo panel.
// Exact model where possible; type photos are ALWAYS labelled representative.
#include <cctype>
#include <cstring>

namespace AircraftTypePhoto {
struct Entry { const char* code; const char* model; const char* commonsCategory; };
static constexpr Entry kModels[] = {
    {"A319", "Airbus A319", "Airbus A319"},
    {"A320", "Airbus A320", "Airbus A320"},
    {"A20N", "Airbus A320neo", "Airbus A320neo"},
    {"A321", "Airbus A321", "Airbus A321"},
    {"A21N", "Airbus A321neo", "Airbus A321neo"},
    {"A333", "Airbus A330-300", "Airbus A330"},
    {"A359", "Airbus A350-900", "Airbus A350-900"},
    {"B737", "Boeing 737-700", "Boeing 737-700"},
    {"B738", "Boeing 737-800", "Boeing 737-800"},
    {"B739", "Boeing 737-900", "Boeing 737-900"},
    {"B38M", "Boeing 737 MAX 8", "Boeing 737 MAX 8"},
    {"B39M", "Boeing 737 MAX 9", "Boeing 737 MAX 9"},
    {"B744", "Boeing 747-400", "Boeing 747-400"},
    {"B77W", "Boeing 777-300ER", "Boeing 777-300ER"},
    {"B788", "Boeing 787-8", "Boeing 787-8"},
    {"B789", "Boeing 787-9", "Boeing 787-9"},
    {"E190", "Embraer 190", "Embraer 190"},
    {"E195", "Embraer 195", "Embraer 195"},
    {"E290", "Embraer E190-E2", "Embraer E190-E2"},
    {"AT76", "ATR 72-600", "ATR 72-600"},
    {"DH8D", "De Havilland Canada Dash 8-400", "De Havilland Canada Dash 8"},
    {"CRJ9", "Bombardier CRJ900", "Bombardier CRJ900"}
};
inline const Entry* find(const char* type) {
    if (!type) return nullptr;
    const size_t length = std::strlen(type);
    if (length != 4) return nullptr;
    for (const auto& model : kModels) {
        bool matches = true;
        for (size_t i = 0; i < length; ++i) {
            if (std::toupper((unsigned char)type[i]) != model.code[i]) {
                matches = false; break;
            }
        }
        if (matches) return &model;
    }
    return nullptr; // Don't guess an image for unknown type codes.
}
}
