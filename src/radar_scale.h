#pragma once
// Geographic scale in nautical miles, derived from Web Mercator tile geometry.
// Independent of Arduino and TFT; host-tested at multiple zoom/latitudes.
#include <cmath>
#include <algorithm>
namespace RadarScale {
inline double pixelsPerNm(double latitude,int zoom) {
    const double lat=std::max(-80.0,std::min(80.0,latitude));
    const double worldPixels=256.0*double(1u<<zoom);
    const double earthNm=21638.777;  // Earth circumference at equator, NM
    return worldPixels/(earthNm*std::cos(lat*0.01745329251994329577));
}
inline double niceDistance(double pixelsNm,double targetPixels=74.0) {
    const double options[]={0.1,0.2,0.5,1,2,5,10,20,50,100,200,500,1000};
    double nearest=options[0],error=1e30;
    for(double val:options) {
        const double px=val*pixelsNm;
        const double gap=std::abs(px-targetPixels);
        if(gap<error) { nearest=val; error=gap; }
    }
    return nearest;
}
inline int barPixels(double latitude,int zoom,double nm) {
    return int(std::lround(pixelsPerNm(latitude,zoom)*nm));
}
} // namespace RadarScale
