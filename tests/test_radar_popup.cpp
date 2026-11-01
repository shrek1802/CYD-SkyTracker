#include "../src/radar_popup.h"
#include <cassert>
#include <iostream>
int main() {
 using namespace RadarPopup;
 assert(card.x==7 && card.y>27 && card.y+card.h<=216);
 assert(card.x+card.w<281);
 assert(details.hit(180,158));
 assert(follow.hit(180,187));
 assert(!details.hit(180,187));
 assert(!follow.hit(180,158));
 assert(dismiss.hit(251,143));
 assert(!card.hit(280,170));
 std::cout<<"Radar popup 320x240 touch geometry OK\n";
}
