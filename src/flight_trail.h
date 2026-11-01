#pragma once
// SkyTracker-owned, fixed-memory path history for ONE selected aircraft.
// Independent implementation based on standard great-circle geometry.
// No Mayhem source code, data or bitmaps used.
#include <cmath>
#include <cstddef>

class FlightTrail {
public:
    struct Point { double lat; double lon; };
    static constexpr int kCapacity = 20;
    static constexpr double kMinMoveMetres = 120.0;

    void clear() { count_ = 0; }
    int size() const { return count_; }
    const Point& at(int index) const { return points_[index]; }

    bool append(double lat, double lon) {
        if (!std::isfinite(lat) || !std::isfinite(lon) ||
            lat < -90 || lat > 90 || lon < -180 || lon > 180) return false;
        if (count_ && distanceMetres(points_[count_ - 1], {lat, lon}) < kMinMoveMetres)
            return false;
        if (count_ == kCapacity) {
            for (int i = 1; i < kCapacity; ++i) points_[i - 1] = points_[i];
            --count_;
        }
        points_[count_++] = {lat, lon};
        return true;
    }

private:
    static double distanceMetres(const Point& p, const Point& q) {
        constexpr double radians = 0.01745329251994329577;
        constexpr double radius = 6371008.8;
        const double deltaLat = (q.lat - p.lat) * radians;
        const double deltaLon = (q.lon - p.lon) * radians;
        const double a = std::sin(deltaLat/2)*std::sin(deltaLat/2) +
                         std::cos(p.lat*radians)*std::cos(q.lat*radians)*
                         std::sin(deltaLon/2)*std::sin(deltaLon/2);
        return 2*radius*std::atan2(std::sqrt(a), std::sqrt(std::fmax(0.0, 1.0 - a)));
    }
    Point points_[kCapacity] = {};
    int count_ = 0;
};
