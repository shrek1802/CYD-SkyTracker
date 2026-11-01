#pragma once
// Radar selection overlay geometry, tested against the 320x240 CYD.
// No Arduino dependencies, and controls never overlap the right map toolbar.
namespace RadarPopup {
struct Rect {
    int x,y,w,h;
    constexpr bool hit(int px,int py) const {
        return px>=x && py>=y && px<x+w && py<y+h;
    }
};
constexpr Rect card{7,139,249,72};
constexpr Rect details{172,148,74,22};
constexpr Rect follow{172,178,74,22};
constexpr Rect dismiss{242,137,17,12};
}
