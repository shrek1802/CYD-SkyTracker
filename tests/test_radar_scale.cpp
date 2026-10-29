#include "../src/radar_scale.h"
#include <cassert>
#include <iostream>
int main(){
 for(int zoom=5;zoom<=13;++zoom){
     const double p=RadarScale::pixelsPerNm(52.95,zoom);
     const double d=RadarScale::niceDistance(p);
     assert(p>0 && d>0);
     assert(RadarScale::barPixels(52.95,zoom,d)>0);
     if(zoom<13)assert(RadarScale::pixelsPerNm(52.95,zoom+1)>p);
 }
 assert(RadarScale::pixelsPerNm(52,10)>RadarScale::pixelsPerNm(0,10));
 assert(RadarScale::niceDistance(20.0)==5.0 ||
        RadarScale::niceDistance(20.0)==2.0);
 std::cout<<"Nautical-mile range-scale tests passed\n";
}
