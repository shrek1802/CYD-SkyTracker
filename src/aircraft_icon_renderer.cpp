#include "aircraft_icon_renderer.h"
#include "aircraft_icon.h"

namespace AircraftIcon {
void draw(TFT_eSPI& tft, int x, int y, float heading,
          uint16_t fillColour, bool highlighted) {
    Point p[kOutlineSize];
    polygon(x, y, heading, p, highlighted ? 1.05f : 0.92f);

    // Thin dark border separates white/yellow/cyan aircraft from even the
    // brightest offline map tile. An outer selection circle is optional.
    constexpr uint16_t kOutline = 0x0000;
    constexpr uint16_t kSelected = TFT_MAGENTA;
    if (highlighted) tft.drawCircle(x, y, 17, kSelected);

    // The silhouette is star-shaped around its fuselage. The triangle fan
    // fills wings and tail without allocating a full image or framebuffer.
    for (int i = 0; i < kOutlineSize; ++i) {
        const Point& a = p[i];
        const Point& b = p[(i + 1) % kOutlineSize];
        tft.fillTriangle(x, y, a.x, a.y, b.x, b.y, fillColour);
    }
    for (int i = 0; i < kOutlineSize; ++i) {
        const Point& a = p[i];
        const Point& b = p[(i + 1) % kOutlineSize];
        tft.drawLine(a.x, a.y, b.x, b.y, kOutline);
    }
    // A bright centre dot helps locate the exact touch target.
    tft.fillCircle(x, y, 1, fillColour);
}
}
