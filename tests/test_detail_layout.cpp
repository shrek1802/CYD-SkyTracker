#include "../src/detail_layout.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
    using namespace DetailLayout;
    assert(openPhoto(100,100));
    assert(openPhoto(8,47));
    assert(!openPhoto(8,46));
    assert(openPhoto(12,225));
    assert(!openPhoto(200,225));
    assert(goToMap(200,225));
    assert(!goToMap(120,225));
    assert(!goToMap(10,100));
    for (const Rect r: {kHero,kAltitude,kSpeed,kDistance,kTrack,kPhotoButton,kMapButton}) {
        assert(r.x>=0 && r.y>=0 && r.x+r.w<=320 && r.y+r.h<=240);
    }
    assert(kHero.y+kHero.h<=kCreditY);
    assert(kCreditY+12<=kAltitude.y);
    assert(kTrack.y+kTrack.h<=kPhotoButton.y);
    assert(std::strcmp(compass16(0),"N")==0);
    assert(std::strcmp(compass16(45),"NE")==0);
    assert(std::strcmp(compass16(180),"S")==0);
    assert(std::strcmp(compass16(270),"W")==0);
    assert(std::strcmp(compass16(359),"N")==0);
    std::cout<<"Aircraft detail layout and compass checks passed\n";
}
