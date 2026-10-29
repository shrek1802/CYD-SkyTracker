#pragma once
// Independently authored ICAO aircraft-type lookup used by the photo panel.
// Exact model where possible; type photos are ALWAYS labelled representative.
#include <cctype>
#include <cstring>

namespace AircraftTypePhoto {
struct Entry { const char* code; const char* model; const char* commonsCategory; };
static constexpr Entry kModels[] = {
    {"A318", "Airbus A318", "Airbus A318"},
    {"A319", "Airbus A319", "Airbus A319"},
    {"A320", "Airbus A320", "Airbus A320"},
    {"A20N", "Airbus A320neo", "Airbus A320neo"},
    {"A321", "Airbus A321", "Airbus A321"},
    {"A21N", "Airbus A321neo", "Airbus A321neo"},
    {"BCS1", "Airbus A220-100", "Airbus A220-100"},
    {"BCS3", "Airbus A220-300", "Airbus A220-300"},
    {"A332", "Airbus A330-200", "Airbus A330-200"},
    {"A333", "Airbus A330-300", "Airbus A330-300"},
    {"A339", "Airbus A330-900neo", "Airbus A330-900neo"},
    {"A359", "Airbus A350-900", "Airbus A350-900"},
    {"A35K", "Airbus A350-1000", "Airbus A350-1000"},
    {"A388", "Airbus A380-800", "Airbus A380-800"},
    {"B737", "Boeing 737-700", "Boeing 737-700"},
    {"B738", "Boeing 737-800", "Boeing 737-800"},
    {"B739", "Boeing 737-900", "Boeing 737-900"},
    {"B38M", "Boeing 737 MAX 8", "Boeing 737 MAX 8"},
    {"B39M", "Boeing 737 MAX 9", "Boeing 737 MAX 9"},
    {"B744", "Boeing 747-400", "Boeing 747-400"},
    {"B748", "Boeing 747-8", "Boeing 747-8"},
    {"B752", "Boeing 757-200", "Boeing 757-200"},
    {"B753", "Boeing 757-300", "Boeing 757-300"},
    {"B763", "Boeing 767-300", "Boeing 767-300"},
    {"B764", "Boeing 767-400", "Boeing 767-400"},
    {"B772", "Boeing 777-200", "Boeing 777-200"},
    {"B77W", "Boeing 777-300ER", "Boeing 777-300ER"},
    {"B788", "Boeing 787-8", "Boeing 787-8"},
    {"B789", "Boeing 787-9", "Boeing 787-9"},
    {"B78X", "Boeing 787-10", "Boeing 787-10"},
    {"E170", "Embraer 170", "Embraer 170"},
    {"E175", "Embraer 175", "Embraer 175"},
    {"E190", "Embraer 190", "Embraer 190"},
    {"E195", "Embraer 195", "Embraer 195"},
    {"E290", "Embraer E190-E2", "Embraer E190-E2"},
    {"E295", "Embraer E195-E2", "Embraer E195-E2"},
    {"AT72", "ATR 72", "ATR 72"},
    {"AT76", "ATR 72-600", "ATR 72-600"},
    {"DH8D", "De Havilland Canada Dash 8-400", "De Havilland Canada Dash 8-400"},
    {"CRJ2", "Bombardier CRJ200", "Bombardier CRJ200"},
    {"CRJ7", "Bombardier CRJ700", "Bombardier CRJ700"},
    {"CRJ9", "Bombardier CRJ900", "Bombardier CRJ900"},
    {"C172", "Cessna 172", "Cessna 172"},
    {"C182", "Cessna 182", "Cessna 182"},
    {"C208", "Cessna 208 Caravan", "Cessna 208 Caravan"},
    {"PA28", "Piper PA-28", "Piper PA-28"},
    {"PC12", "Pilatus PC-12", "Pilatus PC-12"},
    {"C25A", "Cessna Citation CJ2", "Cessna Citation CJ2"},
    {"C56X", "Cessna Citation Excel", "Cessna Citation Excel"},
    {"GLF5", "Gulfstream V", "Gulfstream V"},
    {"GLF6", "Gulfstream G650", "Gulfstream G650"},
    {"CL60", "Bombardier Challenger 600", "Bombardier Challenger 600"},
    {"LJ45", "Learjet 45", "Learjet 45"},
    {"B77F", "Boeing 777F", "Boeing 777F"},
    {"A400", "Airbus A400M", "Airbus A400M"},
    {"C130", "Lockheed C-130 Hercules", "Lockheed C-130 Hercules"},
    {"C17", "Boeing C-17 Globemaster III", "Boeing C-17 Globemaster III"},
    {"E145", "Embraer ERJ 145", "Embraer ERJ 145"},
    {"SF34", "Saab 340", "Saab 340"},
    {"C152", "Cessna 152", "Cessna 152"},
    {"DA40", "Diamond DA40", "Diamond DA40"},
    {"DA42", "Diamond DA42", "Diamond DA42"},
    {"SR22", "Cirrus SR22", "Cirrus SR22"},
    {"EC35", "Eurocopter EC135", "Eurocopter EC135"},
    {"A139", "Leonardo AW139", "Leonardo AW139"},
    {"R44", "Robinson R44", "Robinson R44"},
    {"S76", "Sikorsky S-76", "Sikorsky S-76"},
    {"CL35", "Bombardier Challenger 350", "Bombardier Challenger 350"},
};
inline const Entry* find(const char* type) {
    if (!type) return nullptr;
    const size_t length = std::strlen(type);
    if (length < 2 || length > 4) return nullptr;
    for (const auto& model : kModels) {
        bool matches = true;
        for (size_t i = 0; i < length; ++i) {
            if (!model.code[i] || std::toupper((unsigned char)type[i]) != model.code[i]) {
                matches = false; break;
            }
        }
        if (matches && model.code[length] == '\0') return &model;
    }
    return nullptr; // Don't guess an image for unknown type codes.
}
}
