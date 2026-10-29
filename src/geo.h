#pragma once
#include <cmath>
#include <cstdint>

namespace Geo {
constexpr int kTilePixels = 256;
constexpr double kMaxLatitude = 85.05112878;
constexpr double kPi = 3.14159265358979323846;
inline double clamp(double x, double a, double b) {
    return x < a ? a : (x > b ? b : x);
}
inline double wrapLongitude(double lon) {
    lon = std::fmod(lon + 180.0, 360.0);
    if (lon < 0) lon += 360.0;
    return lon - 180.0;
}
inline double worldSize(int zoom) { return double(kTilePixels) * double(1u << zoom); }
inline double longitudeToX(double lon, int zoom) {
    return (wrapLongitude(lon) + 180.0) / 360.0 * worldSize(zoom);
}
inline double latitudeToY(double lat, int zoom) {
    const double r = clamp(lat, -kMaxLatitude, kMaxLatitude) * kPi / 180.0;
    return (1.0 - std::log(std::tan(r) + 1.0 / std::cos(r)) / kPi) * 0.5 * worldSize(zoom);
}
inline double xToLongitude(double x, int zoom) {
    return wrapLongitude(x / worldSize(zoom) * 360.0 - 180.0);
}
inline double yToLatitude(double y, int zoom) {
    return std::atan(std::sinh(kPi * (1.0 - 2.0 * y / worldSize(zoom)))) * 180.0 / kPi;
}
inline double wrapDeltaX(double dx, int zoom) {
    const double world = worldSize(zoom);
    if (dx > world / 2.0) dx -= world;
    if (dx < -world / 2.0) dx += world;
    return dx;
}
}
