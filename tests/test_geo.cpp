#include "../src/geo.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    for (int z : {3, 9, 10, 12, 16}) {
        for (double lat : {-80.0, -52.91, 0.0, 52.91, 80.0}) {
            for (double lon : {-179.9, -1.2, 0.0, 1.2, 179.9}) {
                double x = Geo::longitudeToX(lon, z);
                double y = Geo::latitudeToY(lat, z);
                assert(x >= 0 && x < Geo::worldSize(z));
                assert(y >= 0 && y <= Geo::worldSize(z));
                assert(std::fabs(Geo::xToLongitude(x, z) - lon) < 1e-7);
                assert(std::fabs(Geo::yToLatitude(y, z) - lat) < 1e-7);
            }
        }
    }
    assert(Geo::wrapDeltaX(Geo::worldSize(9) - 4, 9) == -4.0);
    assert(Geo::wrapDeltaX(-Geo::worldSize(9) + 4, 9) == 4.0);
    std::cout << "Mercator projection tests passed\n";
    return 0;
}
