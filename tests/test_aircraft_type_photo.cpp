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
    assert(std::strcmp(find("BCS3")->model,"Airbus A220-300")==0);
    assert(std::strcmp(find("A388")->model,"Airbus A380-800")==0);
    assert(std::strcmp(find("C172")->model,"Cessna 172")==0);
    assert(std::strcmp(find("B78X")->model,"Boeing 787-10")==0);
    assert(std::strcmp(find("R44")->model,"Robinson R44")==0);
    assert(std::strcmp(find("s76")->model,"Sikorsky S-76")==0);
    assert(std::strcmp(find("B77F")->model,"Boeing 777F")==0);
    assert(std::strcmp(find("B733")->model,"Boeing 737-300")==0);
    assert(std::strcmp(find("E55P")->model,"Embraer Phenom 300")==0);
    assert(std::strcmp(find("C680")->model,"Cessna Citation Sovereign")==0);
    assert(std::strcmp(find("GLEX")->model,"Bombardier Global Express")==0);
    assert(std::strcmp(find("PC24")->model,"Pilatus PC-24")==0);
    assert(std::strcmp(find("DA62")->model,"Diamond DA62")==0);
    assert(std::strcmp(find("A169")->model,"Leonardo AW169")==0);
    assert(std::strcmp(find("B407")->model,"Bell 407")==0);
    assert(std::strcmp(find("PA46")->model,"Piper PA-46 Malibu")==0);
    assert(std::strcmp(find("F100")->model,"Fokker 100")==0);
    assert(std::strcmp(find("S92")->model,"Sikorsky S-92")==0);
    assert(find("C1")==nullptr);
    assert(find("XXXX")==nullptr);
    assert(find("B738XX")==nullptr);
    assert(find("")==nullptr);
    assert(find(nullptr)==nullptr);
    std::cout << "Aircraft type-to-model mapping passed\n";
}
