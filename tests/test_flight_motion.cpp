#include "../src/flight_motion.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main() {
    using namespace FlightMotion;
    const auto zero=predict(52.95,-1.15,420,90,0);
    assert(zero.lat==52.95 && zero.lon==-1.15);
    const auto east=predict(52.95,-1.15,420,90,10000);
    assert(east.lon>-1.15 && std::abs(east.lat-52.95)<0.0001);
    const auto west=predict(52.95,-1.15,420,270,10000);
    assert(west.lon< -1.15);
    const auto stopped=predict(52.95,-1.15,0,90,20000);
    assert(stopped.lat==52.95 && stopped.lon==-1.15);
    const auto capped=predict(52.95,-1.15,420,90,60000);
    const auto max=predict(52.95,-1.15,420,90,kMaxPredictMs);
    assert(std::abs(capped.lon-max.lon)<1e-10);
    const auto nan=predict(52.95,-1.15,NAN,90,10000);
    assert(nan.lat==52.95 && nan.lon==-1.15);
    assert(stillVisible(100000,55000));
    assert(!stillVisible(100001,55000));
    assert(stillVisible(500,UINT32_MAX-100)); // millisecond wrap-safe
    std::cout<<"ADSB motion prediction and stale hold tests passed\n";
}
