#pragma once
// Pure geometry so touch hit areas cannot drift from the drawn widgets.
// The physically working ESP32-2432S028R v2 uses 320 x 240 landscape.
namespace DetailLayout {
struct Rect {
    int x, y, w, h;
    constexpr bool hit(int px,int py) const {
        return px>=x && py>=y && px<x+w && py<y+h;
    }
};
constexpr Rect kHero{8,47,304,110};
constexpr int kCreditY=158;
constexpr Rect kAltitude{8,172,148,19};
constexpr Rect kSpeed{164,172,148,19};
constexpr Rect kDistance{8,194,148,19};
constexpr Rect kTrack{164,194,148,19};
constexpr Rect kPhotoButton{8,216,146,23};
constexpr Rect kMapButton{164,216,148,23};

inline bool openPhoto(int x,int y) {
    return kHero.hit(x,y) || kPhotoButton.hit(x,y);
}
inline bool goToMap(int x,int y) {
    return kMapButton.hit(x,y);
}
inline const char* compass16(float bearing) {
    static constexpr const char* names[16] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSW","SW","WSW","W","WNW","NW","NNW"
    };
    int sector = (int(bearing + 11.25f) / 22) % 16;
    if(sector<0)sector+=16;
    return names[sector];
}
} // namespace DetailLayout
