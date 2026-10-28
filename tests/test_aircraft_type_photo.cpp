#include "../src/aircraft_type_photo.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
    using AircraftTypePhoto::find;
    assert(std::strcmp(find("B738")->model,"Boeing 737-800")==0);
    assert(std::strcmp(find("b38m")->model,"Boeing 737 MAX 8")==0);
    assert(std::strcmp(find("A20N")->model,"Airbus A320neo")==0);
    assert(std::strcmp(find("AT76")->model,"ATR 72-600")==0);
    assert(find("XXXX")==nullptr);
    assert(find("B738XX")==nullptr);
    assert(find("")==nullptr);
    assert(find(nullptr)==nullptr);
    std::cout << "Aircraft type-to-model mapping passed\n";
}
