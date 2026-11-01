#pragma once

// Shared aircraft silhouette geometry, independent of TFT_eSPI, ESP32 and
// screen dimensions. Unit coordinates are (right, forward) with heading
// clockwise from north: 0=north, 90=east, 180=south, 270=west.
#include <cstdint>
#include <cmath>

namespace AircraftIcon {

struct Point {
    int16_t x;
    int16_t y;
};

// Symmetric, swept-wing passenger plane, viewed from directly above.
// Silhouette approximately 25 x 28 px at scale 1.0.
// The shape uses a slender nose, broad main wings and a small rear tailplane.
constexpr int kOutlineSize = 20;

inline void polygon(int cx, int cy, float headingDegrees,
                    Point (&out)[kOutlineSize], float scale = 1.0f) {
    struct Local { int8_t right, forward; };
    static const Local vertices[kOutlineSize] = {
        {  0,  13 }, // nose
        {  2,   8 },
        {  3,   4 },
        { 12,  -3 }, // starboard swept wing leading edge
        { 12,  -6 }, // starboard wing tip
        {  3,  -3 }, // starboard wing trailing edge
        {  3, -10 },
        {  6, -13 }, // right tailplane
        {  6, -15 },
        {  1, -13 },
        { -1, -13 },
        { -6, -15 }, // left tailplane
        { -6, -13 },
        { -3, -10 },
        { -3,  -3 },
        {-12,  -6 }, // port wing tip
        {-12,  -3 },
        { -3,   4 },
        { -2,   8 },
        { -1,  11 }
    };
    if (!std::isfinite(headingDegrees)) headingDegrees = 0.0f;
    const float radians = headingDegrees * 0.01745329251994329577f;
    const float sn = std::sin(radians), cs = std::cos(radians);
    for (int i = 0; i < kOutlineSize; ++i) {
        const float right = float(vertices[i].right) * scale;
        const float forward = float(vertices[i].forward) * scale;
        out[i].x = int16_t(std::lround(cx + right * cs + forward * sn));
        out[i].y = int16_t(std::lround(cy + right * sn - forward * cs));
    }
}
}
