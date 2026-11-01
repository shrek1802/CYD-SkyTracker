#pragma once
#include <Arduino.h>
namespace PostcodeLookup {
struct Location {
    float latitude = 0;
    float longitude = 0;
    String postcode;
};
bool resolve(const String& input, Location& location, String& reason);
}
