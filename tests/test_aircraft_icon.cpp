#include "../src/aircraft_icon.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using namespace AircraftIcon;
    Point p[kOutlineSize];
    polygon(100, 100, 0.0f, p);
    assert(p[0].x == 100 && p[0].y == 87); // north
    polygon(100, 100, 90.0f, p);
    assert(p[0].x == 113 && p[0].y == 100); // east
    polygon(100, 100, 180.0f, p);
    assert(p[0].x == 100 && p[0].y == 113); // south
    polygon(100, 100, 270.0f, p);
    assert(p[0].x == 87 && p[0].y == 100); // west

    // All points remain inside a compact ~32px sprite area.
    for (int deg = 0; deg < 360; deg += 15) {
        polygon(70, 80, float(deg), p, 0.92f);
        for (const Point& point : p) {
            assert(point.x >= 52 && point.x <= 88);
            assert(point.y >= 62 && point.y <= 98);
        }
    }

    // A missing heading must never produce out-of-bounds coordinates.
    polygon(100, 100, NAN, p);
    assert(p[0].x == 100 && p[0].y == 87);
    std::cout << "Aircraft silhouette orientation tests passed\\n";
}
