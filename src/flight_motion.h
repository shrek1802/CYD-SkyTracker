#pragma once
// Short-lived *estimated* motion between live ADS-B updates.
// Pure standard C++: testable without ESP32, Wi-Fi or SPI peripherals.
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace FlightMotion {
constexpr uint32_t kMaxPredictMs = 12000;
constexpr uint32_t kKeepMissingMs = 45000;
constexpr double kEarthRadiusNm = 3440.065;
constexpr double kRad = 0.01745329251994329577;
struct Position {
    double lat;
    double lon;
};
inline Position predict(double lat,double lon,float speedKt,float trackDeg,
                        uint32_t fixAgeMs) {
    if (!std::isfinite(lat) || !std::isfinite(lon) ||
        !std::isfinite(speedKt) || !std::isfinite(trackDeg) ||
        speedKt <= 2 || speedKt > 1200 ||
        std::abs(lat)>85.0 || std::abs(lon)>180.0) return {lat,lon};
    const auto age = std::min(fixAgeMs,kMaxPredictMs);
    const double distanceNm = double(speedKt)*double(age)/3600000.0;
    const double angle=distanceNm/kEarthRadiusNm;
    const double a=lat*kRad,b=trackDeg*kRad;
    const double target=std::asin(std::sin(a)*std::cos(angle)+
                        std::cos(a)*std::sin(angle)*std::cos(b));
    const double diff=std::atan2(std::sin(b)*std::sin(angle)*std::cos(a),
                       std::cos(angle)-std::sin(a)*std::sin(target));
    double newLon=lon+diff/kRad;
    while(newLon>180)newLon-=360;
    while(newLon< -180)newLon+=360;
    return {target/kRad,newLon};
}
inline bool stillVisible(uint32_t nowMs,uint32_t lastConfirmedMs) {
    return uint32_t(nowMs-lastConfirmedMs) <= kKeepMissingMs;
}
}
