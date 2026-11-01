#include "../src/aircraft_symbol.h"
#include <cassert>
#include <iostream>
int main() {
 using namespace AircraftSymbol;
 assert(fromType("B738")==Kind::Airliner);
 assert(fromType("A388")==Kind::Airliner);
 assert(fromType("C172")==Kind::Light);
 assert(fromType("PC12")==Kind::Light);
 assert(fromType("R44")==Kind::Helicopter);
 assert(fromType("A139")==Kind::Helicopter);
 assert(fromType("C25B")==Kind::BusinessJet);
 assert(fromType("GLF6")==Kind::BusinessJet);
 assert(scale(Kind::Airliner,5,false)<scale(Kind::Airliner,12,false));
 assert(scale(Kind::Helicopter,9,true)>scale(Kind::Helicopter,9,false));
 std::cout<<"Aircraft icon family/zoom tests passed\n";
}
