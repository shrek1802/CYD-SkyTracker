#include "../src/flight_trail.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    FlightTrail p;
    assert(p.size() == 0);
    assert(!p.append(NAN, -1));
    assert(!p.append(91, 0));
    assert(p.append(52.95, -1.15));
    assert(!p.append(52.95001, -1.15)); // sub-120m jitter
    assert(p.append(52.952, -1.15));
    assert(p.size() == 2);
    for (int i=0;i<40;i++)
        p.append(53.0+i*0.003, -1.15);
    assert(p.size() == FlightTrail::kCapacity);
    assert(std::abs(p.at(p.size()-1).lat - (53.0+39*0.003)) < 0.00001);
    p.clear();
    assert(p.size() == 0);
    // Date line discontinuity must be treated as a short physical distance.
    assert(p.append(0,179.9996));
    assert(!p.append(0,-179.9996)); // only ~89m
    assert(p.size() == 1);
    std::cout << "FlightTrail fixed-memory, jitter and dateline tests passed\n";
}
