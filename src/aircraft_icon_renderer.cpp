#include "aircraft_icon_renderer.h"
#include "aircraft_icon.h"
#include <cmath>
#include <algorithm>

namespace {
using Kind=AircraftSymbol::Kind;
struct Pt { float right, forward; };
void paintPolygon(TFT_eSPI& t,int x,int y,float heading,float scale,
                  const Pt* vertices,int count,uint16_t color) {
    const float angle=std::isfinite(heading)?heading*0.01745329251994329577f:0;
    const float sn=std::sin(angle), cs=std::cos(angle);
    int16_t px[20],py[20];
    if(count>20)return;
    for(int i=0;i<count;++i) {
        px[i]=int16_t(std::lround(x+scale*(vertices[i].right*cs+
                                            vertices[i].forward*sn)));
        py[i]=int16_t(std::lround(y+scale*(vertices[i].right*sn-
                                            vertices[i].forward*cs)));
    }
    for(int i=0;i<count;++i) {
        const int next=(i+1)%count;
        t.fillTriangle(x,y,px[i],py[i],px[next],py[next],color);
    }
    for(int i=0;i<count;++i) {
        const int next=(i+1)%count;
        t.drawLine(px[i],py[i],px[next],py[next],TFT_BLACK);
    }
}
constexpr Pt kLight[]={
    {0,11},{2,7},{2,3},{13,1},{13,-1},{2,-2},{2,-9},
    {6,-11},{6,-12},{0,-10},{-6,-12},{-6,-11},{-2,-9},
    {-2,-2},{-13,-1},{-13,1},{-2,3},{-2,7}
};
constexpr Pt kJet[]={
    {0,14},{2,8},{3,2},{12,-2},{12,-4},{3,-5},{2,-10},
    {5,-13},{5,-14},{0,-12},{-5,-14},{-5,-13},{-2,-10},
    {-3,-5},{-12,-4},{-12,-2},{-3,2},{-2,8}
};
void helicopter(TFT_eSPI& t,int x,int y,float heading,float factor,uint16_t colour) {
    // Oval cabin + straight rotor + tail, rotated in track direction.
    float angle=std::isfinite(heading)?heading*0.01745329251994329577f:0;
    float sn=std::sin(angle),cs=std::cos(angle);
    auto line=[&](float r1,float f1,float r2,float f2,uint16_t c) {
        t.drawLine(int(std::lround(x+factor*(r1*cs+f1*sn))),
                   int(std::lround(y+factor*(r1*sn-f1*cs))),
                   int(std::lround(x+factor*(r2*cs+f2*sn))),
                   int(std::lround(y+factor*(r2*sn-f2*cs))),c);
    };
    t.fillCircle(x,y,3,colour);
    t.drawCircle(x,y,3,TFT_BLACK);
    line(0,-5,0,-13,colour);
    line(-3,-13,3,-13,colour);
    line(-12,0,12,0,TFT_BLACK);
    line(-12,-1,12,-1,colour);
}
}
namespace AircraftIcon {
void draw(TFT_eSPI& t,int x,int y,float heading,uint16_t colour,
          bool selected,AircraftSymbol::Kind kind,float factor) {
    if(selected) {
        t.drawCircle(x,y,17,TFT_MAGENTA);
        t.drawCircle(x,y,19,TFT_CYAN);
    }
    if(kind==Kind::Helicopter)helicopter(t,x,y,heading,factor,colour);
    else if(kind==Kind::Light)
        paintPolygon(t,x,y,heading,factor,kLight,
                     sizeof(kLight)/sizeof(kLight[0]),colour);
    else if(kind==Kind::BusinessJet)
        paintPolygon(t,x,y,heading,factor,kJet,
                     sizeof(kJet)/sizeof(kJet[0]),colour);
    else {
        Point p[kOutlineSize];
        polygon(x,y,heading,p,factor);
        for(int i=0;i<kOutlineSize;++i) {
            const auto& a=p[i];
            const auto& b=p[(i+1)%kOutlineSize];
            t.fillTriangle(x,y,a.x,a.y,b.x,b.y,colour);
        }
        for(int i=0;i<kOutlineSize;++i) {
            const auto& a=p[i];
            const auto& b=p[(i+1)%kOutlineSize];
            t.drawLine(a.x,a.y,b.x,b.y,TFT_BLACK);
        }
    }
    t.fillCircle(x,y,1,colour);
}
}
