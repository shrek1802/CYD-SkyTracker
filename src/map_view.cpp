#include "map_view.h"
#include "geo.h"
#include <SD.h>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace {
constexpr uint16_t kBg = 0x2104;
constexpr uint16_t kSea = 0x10A2;
constexpr uint16_t kPanel = 0x08A3;
constexpr int kSide = Geo::kTilePixels;
constexpr int kMaxZoom = 16;
uint16_t rowPixels[kSide]; // 512 bytes, allocated once
void missingTile(TFT_eSPI& t, int x, int y, int w, int h, int tx, int ty) {
    const uint16_t c = ((tx + ty) & 1) ? kBg : kSea;
    t.fillRect(x, y, w, h, c);
}
}

void MapView::begin(double homeLat, double homeLon, uint8_t zoom) {
    homeLat_ = Geo::clamp(homeLat, -Geo::kMaxLatitude, Geo::kMaxLatitude);
    homeLon_ = Geo::wrapLongitude(homeLon);
    centerLat_ = homeLat_;
    centerLon_ = homeLon_;
    zoom_ = (zoom < 3 || zoom > kMaxZoom) ? 10 : zoom;
    ready_ = true;
}
void MapView::setZoom(uint8_t value) {
    if (value >= 3 && value <= kMaxZoom) zoom_ = value;
}
void MapView::panPixels(int dx, int dy) {
    // Finger dragging a map to the right shifts the geographic centre left.
    const double wx = Geo::longitudeToX(centerLon_, zoom_) - dx;
    const double wy = Geo::latitudeToY(centerLat_, zoom_) - dy;
    centerLon_ = Geo::xToLongitude(wx, zoom_);
    centerLat_ = Geo::yToLatitude(Geo::clamp(wy, 0.0, Geo::worldSize(zoom_)), zoom_);
}
void MapView::tap(int x, int y, int w, int h) {
    if (x < w - 46 || y < 32 || y >= h - 20) return;
    if (y < 76) setZoom(zoom_ + 1);
    else if (y < 120) setZoom(zoom_ - 1);
    else if (y < 165) {
        centerLat_ = homeLat_;
        centerLon_ = homeLon_;
    }
}
bool MapView::project(double lat, double lon, int& x, int& y) const {
    if (!ready_ || width_ < 100 || bottom_ <= top_) return false;
    const double centerX = Geo::longitudeToX(centerLon_, zoom_);
    const double centerY = Geo::latitudeToY(centerLat_, zoom_);
    const double targetX = Geo::longitudeToX(lon, zoom_);
    const double targetY = Geo::latitudeToY(lat, zoom_);
    x = int(std::round(width_ * 0.5 + Geo::wrapDeltaX(targetX - centerX, zoom_)));
    y = int(std::round((top_ + bottom_) * 0.5 + targetY - centerY));
    return x >= 8 && x < width_ - 8 && y >= top_ + 8 && y < bottom_ - 8;
}
void MapView::drawTiles(TFT_eSPI& tft) {
    const double cx = Geo::longitudeToX(centerLon_, zoom_);
    const double cy = Geo::latitudeToY(centerLat_, zoom_);
    const int mapH = bottom_ - top_;
    const double originX = cx - double(width_) / 2;
    const double originY = cy - double(mapH) / 2;
    const int tiles = 1 << zoom_;
    const int txStart = int(std::floor(originX / kSide));
    const int tyStart = int(std::floor(originY / kSide));
    const int txEnd = int(std::floor((originX + width_ - 1) / kSide));
    const int tyEnd = int(std::floor((originY + mapH - 1) / kSide));
    char path[88];
    for (int ty = tyStart; ty <= tyEnd; ++ty) {
        for (int tx = txStart; tx <= txEnd; ++tx) {
            const int tileX = int(std::round(double(tx) * kSide - originX));
            const int tileY = top_ + int(std::round(double(ty) * kSide - originY));
            const int dstX = std::max(0, tileX);
            const int dstY = std::max(top_, tileY);
            const int dstRight = std::min(width_, tileX + kSide);
            const int dstBottom = std::min(bottom_, tileY + kSide);
            if (dstX >= dstRight || dstY >= dstBottom) continue;
            const int dstW = dstRight - dstX;
            const int dstH = dstBottom - dstY;
            if (ty < 0 || ty >= tiles) {
                missingTile(tft, dstX, dstY, dstW, dstH, tx, ty);
                ++missing_;
                continue;
            }
            const int wrappedX = (tx % tiles + tiles) % tiles;
            std::snprintf(path, sizeof(path), "/maps/%d/%d/%d.rgb565",
                          int(zoom_), wrappedX, ty);
            File f = SD.open(path, FILE_READ);
            if (!f || f.size() != size_t(kSide * kSide * 2)) {
                if (f) f.close();
                missingTile(tft, dstX, dstY, dstW, dstH, tx, ty);
                ++missing_;
                continue;
            }
            const int srcX = dstX - tileX;
            const int srcY = dstY - tileY;
            tft.startWrite();
            bool ok = true;
            for (int y = 0; y < dstH; ++y) {
                const uint32_t offset = (uint32_t(srcY + y) * kSide + srcX) * 2;
                if (!f.seek(offset) || f.read(reinterpret_cast<uint8_t*>(rowPixels),
                                           size_t(dstW) * 2) != size_t(dstW) * 2) {
                    ok = false;
                    break;
                }
                tft.pushImage(dstX, dstY + y, dstW, 1, rowPixels);
            }
            tft.endWrite();
            f.close();
            if (!ok) {
                missingTile(tft, dstX, dstY, dstW, dstH, tx, ty);
                ++missing_;
            }
            delay(0);
        }
    }
}

void MapView::drawUi(TFT_eSPI& tft, bool wifiConnected, bool sdReady, int missingCount) {
    tft.fillRect(0, 0, width_, top_, kPanel);
    tft.setTextSize(1);
    tft.setTextColor(TFT_CYAN, kPanel);
    tft.setCursor(6, 9);
    tft.print("SKYTRACKER");
    tft.setTextColor(TFT_WHITE, kPanel);
    tft.setCursor(width_ - 114, 9);
    tft.printf("z%d  %s", int(zoom_), wifiConnected ? "WiFi OK" : "Offline");
    const int right = width_ - 41;
    const int controlY[] = {38, 82, 126};
    const char* labels[] = {"+", "-", "HOME"};
    for (int i = 0; i < 3; ++i) {
        tft.fillRoundRect(right, controlY[i], 36, 37, 5, kPanel);
        tft.setTextColor(TFT_WHITE, kPanel);
        tft.setCursor(right + (i == 2 ? 5 : 14), controlY[i] + 12);
        tft.print(labels[i]);
    }
    tft.fillRect(0, bottom_, width_, tft.height() - bottom_, kPanel);
    tft.setCursor(5, bottom_ + 6);
    tft.setTextColor(TFT_WHITE, kPanel);
    if (!sdReady) tft.print("Insert FAT32 SD card with /maps/");
    else if (missingCount) tft.printf("%d tile(s) missing at zoom %u", missingCount, unsigned(zoom_));
    else tft.print("Local map - development build");
    // Always show attribution on the device when a provider has supplied it.
    // Read at most 75 bytes, avoiding unbounded SD file/string allocations.
    if (sdReady) {
        File attribution = SD.open("/maps/attribution.txt", FILE_READ);
        tft.setCursor(5, bottom_ + 19);
        tft.setTextColor(TFT_CYAN, kPanel);
        if (attribution) {
            char line[76] = {};
            const size_t n = attribution.readBytesUntil('\n', line, sizeof(line)-1);
            line[n] = 0;
            attribution.close();
            tft.print(String(line).substring(0, (width_ - 10) / 6));
        } else {
            tft.print("Map source attribution not supplied");
        }
    }
}
void MapView::draw(TFT_eSPI& tft, bool sdReady, bool wifiConnected) {
    if (!ready_) return;
    width_ = tft.width();
    top_ = 32;
    bottom_ = tft.height() - 32;
    missing_ = 0;
    tft.fillScreen(kBg);
    if (sdReady) drawTiles(tft);
    else missingTile(tft, 0, top_, width_, bottom_ - top_, 0, 0);
    // Air traffic still works when no offline map pack is installed.
    // Draw a simple grid so relative positions and bearing are visible.
    if (!sdReady || missing_ > 0) {
        for (int x=0;x<width_;x+=40)
            tft.drawFastVLine(x,top_,bottom_-top_,TFT_DARKGREY);
        for (int y=top_;y<bottom_;y+=40)
            tft.drawFastHLine(0,y,width_,TFT_DARKGREY);
    }

    // Fixed home marker; it correctly moves when the map is panned.
    const double centerX = Geo::longitudeToX(centerLon_, zoom_);
    const double centerY = Geo::latitudeToY(centerLat_, zoom_);
    const double homeX = Geo::longitudeToX(homeLon_, zoom_);
    const double homeY = Geo::latitudeToY(homeLat_, zoom_);
    const int hx = int(std::round(width_ * 0.5 + Geo::wrapDeltaX(homeX - centerX, zoom_)));
    const int hy = int(std::round((top_ + bottom_) * 0.5 + homeY - centerY));
    if (hx >= 0 && hx < width_ && hy >= top_ && hy < bottom_) {
        tft.drawCircle(hx, hy, 7, TFT_YELLOW);
        tft.drawFastHLine(hx - 4, hy, 9, TFT_YELLOW);
        tft.drawFastVLine(hx, hy - 4, 9, TFT_YELLOW);
    }
    drawUi(tft, wifiConnected, sdReady, missing_);
}
