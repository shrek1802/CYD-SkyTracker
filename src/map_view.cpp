#include "map_view.h"
#include "geo.h"
#include "radar_scale.h"
#include "ui_style.h"
#include "rgb565_display.h"
#include <SD.h>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace {
constexpr uint16_t kBg = 0x10A4;
constexpr uint16_t kSea = 0x10A2;
constexpr uint16_t kPanel = SkyUI::kPanel;
constexpr int kSide = Geo::kTilePixels;
constexpr int kMaxZoom = 16;
uint16_t rowPixels[kSide]; // 512 bytes, allocated once
// Preserve source RGB565 colours; no on-device desaturation/recolouring.
// The diagnostic tests byte order independently of the map cartography.
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
    nightMode_ = nightAvailable_ = themeInitialised_ = false;
    showRangeRings_ = true;
    ready_ = true;
}
void MapView::setZoom(uint8_t value) {
    if (value < 5 || value > 13 || value == zoom_) return;
    // Never let the +/- controls enter a zoom level without an installed tile
    // at the current map centre. Legacy z9-z13 SD cards remain safe, while
    // the optional wide-area z5-z8 pack unlocks further zoom-out levels.
    const double worldX = Geo::longitudeToX(centerLon_, value);
    const double worldY = Geo::latitudeToY(centerLat_, value);
    const int n = 1 << value;
    const int tileX = (int(std::floor(worldX / 256.0)) % n + n) % n;
    const int tileY = int(std::floor(worldY / 256.0));
    char path[92];
    std::snprintf(path, sizeof(path),
                  nightMode_ ? "/maps/night/%d/%d/%d.rgb565" :
                               "/maps/%d/%d/%d.rgb565",
                  int(value), tileX, tileY);
    File tile = SD.open(path, FILE_READ);
    const bool available = tile && tile.size() == size_t(256*256*2);
    if (tile) tile.close();
    if (!available) {
        Serial.printf("[MAP] Zoom %d unavailable: missing %s\\n", int(value), path);
        return;
    }
    zoom_ = value;
    Serial.printf("[MAP] Zoom changed to %d\\n", int(zoom_));
}
void MapView::panPixels(int dx, int dy) {
    // Finger dragging a map to the right shifts the geographic centre left.
    const double wx = Geo::longitudeToX(centerLon_, zoom_) - dx;
    const double wy = Geo::latitudeToY(centerLat_, zoom_) - dy;
    centerLon_ = Geo::xToLongitude(wx, zoom_);
    centerLat_ = Geo::yToLatitude(Geo::clamp(wy, 0.0, Geo::worldSize(zoom_)), zoom_);
}
void MapView::tap(int x, int y, int w, int h) {
    if (!SkyUI::mapControlHit(x,y,w))return;
    if (y < 69) setZoom(zoom_ + 1);
    else if (y < 104) setZoom(zoom_ - 1);
    else if (y < SkyUI::kThemeY) {
        centerLat_ = homeLat_;
        centerLon_ = homeLon_;
    } else if (y <= SkyUI::kThemeY + SkyUI::kControlH && nightAvailable_) {
        nightMode_ = !nightMode_;
        Serial.printf("[MAP] Map style: %s\\n", nightMode_ ? "NIGHT" : "DAY");
    } else if (y >= SkyUI::kRingsY - 3 &&
               y <= SkyUI::kRingsY + SkyUI::kControlH + 3) {
        showRangeRings_ = !showRangeRings_;
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
void MapView::drawRadarOverlay(TFT_eSPI& tft,int left,int top,int right,int bottom) const {
    if(!ready_||width_<=0||bottom_<=top_)return;
    auto inside=[&](int x,int y) {
        return x>=left && x<right && y>=top && y<bottom &&
               x>=0 && x<width_ && y>=top_ && y<bottom_;
    };
    // Map is north-up; a small compass does not obscure the live flight icons.
    if(inside(16,36)) {
        tft.setTextSize(1);
        tft.setTextColor(SkyUI::kAccent); // transparent over real map pixels
        tft.setCursor(12,33); tft.print("N");
    }
    if(inside(16,47)) {
        tft.drawFastVLine(16,44,12,SkyUI::kAccent);
        tft.fillTriangle(12,45,20,45,16,40,SkyUI::kAccent);
    }
    if(!showRangeRings_)return;
    const double homeX=Geo::longitudeToX(homeLon_,zoom_);
    const double homeY=Geo::latitudeToY(homeLat_,zoom_);
    const double cx=Geo::longitudeToX(centerLon_,zoom_);
    const double cy=Geo::latitudeToY(centerLat_,zoom_);
    const int x=int(std::lround(width_*0.5+Geo::wrapDeltaX(homeX-cx,zoom_)));
    const int y=int(std::lround((top_+bottom_)*0.5+homeY-cy));
    const double pxNm=RadarScale::pixelsPerNm(homeLat_,zoom_);
    const double nm=RadarScale::niceDistance(pxNm);
    const uint16_t ring=nightMode_?0x39A8:0xA534; // subdued grey/cyan
    for(int multiple=1;multiple<=2;++multiple) {
        const int radius=RadarScale::barPixels(homeLat_,zoom_,nm*multiple);
        if(radius<=0||radius>1800)continue;
        if(x+radius<left||x-radius>=right||
           y+radius<top||y-radius>=bottom)continue;
        for(int degrees=0;degrees<360;degrees+=6) {
            const double a=double(degrees)*Geo::kPi/180.0;
            const int px=x+int(std::lround(radius*std::cos(a)));
            const int py=y+int(std::lround(radius*std::sin(a)));
            if(inside(px,py))tft.drawPixel(px,py,ring);
        }
    }
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
            std::snprintf(path, sizeof(path), nightMode_ ? "/maps/night/%d/%d/%d.rgb565" : "/maps/%d/%d/%d.rgb565",
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
                SkyRgb565::pushRow(tft, dstX, dstY + y, dstW, rowPixels);
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

// Only repair the screen pixels obscured by aircraft from the previous poll.
// This keeps roads/town names stable and avoids the old 30s whole-screen flash.
void MapView::restoreRect(TFT_eSPI& tft, int x, int y, int w, int h, bool sdReady) {
    if (!ready_ || width_ < 1 || bottom_ <= top_ || w <= 0 || h <= 0) return;
    const int l=std::max(0,x), r=std::min(width_,x+w);
    const int t=std::max(top_,y), b=std::min(bottom_,y+h);
    if (l>=r || t>=b) return;
    if (!sdReady) {
        missingTile(tft,l,t,r-l,b-t,0,0);
    } else {
        const double cx=Geo::longitudeToX(centerLon_,zoom_);
        const double cy=Geo::latitudeToY(centerLat_,zoom_);
        const double originX=cx-double(width_)/2;
        const double originY=cy-double(bottom_-top_)/2;
        const int n=1<<zoom_;
        const int tx0=int(std::floor((originX+l)/kSide));
        const int tx1=int(std::floor((originX+r-1)/kSide));
        const int ty0=int(std::floor((originY+(t-top_))/kSide));
        const int ty1=int(std::floor((originY+(b-top_)-1)/kSide));
        char path[88];
        for (int ty=ty0;ty<=ty1;++ty) {
            for (int tx=tx0;tx<=tx1;++tx) {
                const int px=int(std::round(double(tx)*kSide-originX));
                const int py=top_+int(std::round(double(ty)*kSide-originY));
                const int dl=std::max(l,px), dt=std::max(t,py);
                const int dr=std::min(r,px+kSide), db=std::min(b,py+kSide);
                if(dl>=dr||dt>=db)continue;
                if(ty<0||ty>=n) {
                    missingTile(tft,dl,dt,dr-dl,db-dt,tx,ty);
                    continue;
                }
                const int wrap=(tx%n+n)%n;
                std::snprintf(path,sizeof(path),nightMode_ ? "/maps/night/%d/%d/%d.rgb565" : "/maps/%d/%d/%d.rgb565",
                              int(zoom_),wrap,ty);
                File tile=SD.open(path,FILE_READ);
                if(!tile||tile.size()!=size_t(kSide*kSide*2)) {
                    if(tile)tile.close();
                    missingTile(tft,dl,dt,dr-dl,db-dt,tx,ty);
                    continue;
                }
                const int sx=dl-px, sy=dt-py, count=dr-dl;
                tft.startWrite();
                bool ok=true;
                for(int line=0;line<db-dt;++line) {
                    const uint32_t offset=(uint32_t(sy+line)*kSide+sx)*2;
                    if(!tile.seek(offset) ||
                       tile.read(reinterpret_cast<uint8_t*>(rowPixels),size_t(count)*2)!=size_t(count)*2) {
                        ok=false;
                        break;
                    }
                    SkyRgb565::pushRow(tft,dl,dt+line,count,rowPixels);
                }
                tft.endWrite();
                tile.close();
                if(!ok)missingTile(tft,dl,dt,dr-dl,db-dt,tx,ty);
            }
        }
    }

    // The full map's fallback grid is only drawn when missing tiles were
    // found at load. Restore lines in the clipped rectangle as well.
    if(!sdReady||missing_>0) {
        for(int gx=((l+39)/40)*40;gx<r;gx+=40)
            tft.drawFastVLine(gx,t,b-t,TFT_DARKGREY);
        const int first=top_+(((t-top_+39)/40)*40);
        for(int gy=first;gy<b;gy+=40)
            tft.drawFastHLine(l,gy,r-l,TFT_DARKGREY);
    }

    // Range overlay must be restored BEFORE repainting aircraft icons.
    drawRadarOverlay(tft,l,t,r,b);

    // Restore fixed home cross if the aircraft footprint intersects it.
    const double worldX=Geo::longitudeToX(centerLon_,zoom_);
    const double worldY=Geo::latitudeToY(centerLat_,zoom_);
    const int hx=int(std::round(width_*0.5+
               Geo::wrapDeltaX(Geo::longitudeToX(homeLon_,zoom_)-worldX,zoom_)));
    const int hy=int(std::round((top_+bottom_)*0.5+
               Geo::latitudeToY(homeLat_,zoom_)-worldY));
    if(hx+8>=l&&hx-8<r&&hy+8>=t&&hy-8<b) {
        tft.drawCircle(hx,hy,7,TFT_YELLOW);
        tft.drawFastHLine(hx-4,hy,9,TFT_YELLOW);
        tft.drawFastVLine(hx,hy-4,9,TFT_YELLOW);
    }

    // Redraw only controls intersecting the repaired map patch.
    for(int i=0;i<5;++i) {
        if(i==3 && !nightAvailable_)continue;
        const int yy=i==4?SkyUI::kRingsY:
                     i==3?SkyUI::kThemeY:SkyUI::kControlY[i];
        if(r>=width_-SkyUI::kControlW-5 && b>=yy && t<=yy+SkyUI::kControlH) {
            SkyUI::button(tft,width_-SkyUI::kControlW-5,yy,
                          SkyUI::kControlW,SkyUI::kControlH,
                          i==0?"+":(i==1?"-":i==2?"H":
                          i==3?(nightMode_?"D":"N"):(showRangeRings_?"R":"r")));
        }
    }
}

void MapView::drawFlightStatus(TFT_eSPI& tft,int count,const String& status,
                               int ageSeconds,const char* selectedFlight) {
    const bool live=status.startsWith("Live");
    // Small top-panel repairs only. No full map redraw during ADS-B polls.
    tft.fillRect(99,0,137,SkyUI::kTop,SkyUI::kPanel);
    tft.fillCircle(105,13,3,live?SkyUI::kGreen:SkyUI::kGold);
    tft.setTextSize(1);
    tft.setTextColor(SkyUI::kWhite,SkyUI::kPanel);
    tft.setCursor(113,10);
    tft.print(live?"LIVE":"WAIT");
    tft.setTextColor(SkyUI::kGold,SkyUI::kPanel);
    tft.setCursor(163,10);
    tft.printf("%d ACFT",count);
    // A selected plane's real flight callsign, without cluttering every icon.
    tft.fillRect(160,bottom_+1,87,12,SkyUI::kPanel);
    if(selectedFlight && selectedFlight[0]) {
        tft.setTextColor(SkyUI::kAccent,SkyUI::kPanel);
        tft.setCursor(162,bottom_+3);
        tft.print(String(selectedFlight).substring(0,13));
    }
    // One short status slot, updated by time without touching the tile area.
    tft.fillRect(width_-69,bottom_+1,66,12,SkyUI::kPanel);
    tft.setTextColor(live?SkyUI::kGreen:SkyUI::kGold,SkyUI::kPanel);
    tft.setCursor(width_-68,bottom_+3);
    if(ageSeconds<0)tft.print("WAIT DATA");
    else if(ageSeconds>999)tft.print("STALE");
    else tft.printf("%ds AGO",ageSeconds);
}
void MapView::drawUi(TFT_eSPI& tft,bool wifiConnected,bool sdReady,int missingCount) {
    tft.fillRect(0,0,width_,top_,SkyUI::kPanel);
    tft.drawFastHLine(0,top_-1,width_,SkyUI::kAccent);
    tft.setTextSize(1);
    tft.setTextColor(SkyUI::kAccent,SkyUI::kPanel);
    tft.setCursor(6,10);
    tft.print("SKYTRACKER");
    tft.setTextColor(SkyUI::kWhite,SkyUI::kPanel);
    tft.setCursor(width_-83,10);
    tft.printf("Z%u",unsigned(zoom_));
    tft.setTextColor(wifiConnected?SkyUI::kGreen:SkyUI::kGold,SkyUI::kPanel);
    tft.setCursor(width_-39,10);
    tft.print(wifiConnected?"NET":"OFF");

    for(int i=0;i<3;++i) {
        SkyUI::button(tft,width_-SkyUI::kControlW-5,SkyUI::kControlY[i],
                      SkyUI::kControlW,SkyUI::kControlH,
                      i==0?"+":(i==1?"-":"H"));
    }
    if(nightAvailable_) {
        SkyUI::button(tft,width_-SkyUI::kControlW-5,SkyUI::kThemeY,
                      SkyUI::kControlW,SkyUI::kControlH,
                      nightMode_?"D":"N");
    }
    SkyUI::button(tft,width_-SkyUI::kControlW-5,SkyUI::kRingsY,
                  SkyUI::kControlW,SkyUI::kControlH,
                  showRangeRings_?"R":"r");

    tft.fillRect(0,bottom_,width_,tft.height()-bottom_,SkyUI::kPanel);
    tft.setTextSize(1);
    const double distance=RadarScale::niceDistance(
        RadarScale::pixelsPerNm(centerLat_,zoom_));
    const int pixels=RadarScale::barPixels(centerLat_,zoom_,distance);
    const int bar=std::max(5,std::min(90,pixels));
    tft.drawFastHLine(8,bottom_+8,bar,SkyUI::kAccent);
    tft.drawFastVLine(8,bottom_+5,7,SkyUI::kAccent);
    tft.drawFastVLine(8+bar,bottom_+5,7,SkyUI::kAccent);
    tft.setTextColor(SkyUI::kWhite,SkyUI::kPanel);
    tft.setCursor(13+bar,bottom_+4);
    tft.print(String(distance,distance<1.0?1:0)+" NM");
    tft.setTextColor(SkyUI::kMuted,SkyUI::kPanel);
    tft.setCursor(6,bottom_+15);
    // OpenStreetMap contributors are credited for all detailed map styles.
    tft.print("(c) OpenStreetMap contributors");
    if(!sdReady||missingCount)
        Serial.printf("[MAP] SD=%d missing=%d\n",sdReady?1:0,missingCount);
}

void MapView::draw(TFT_eSPI& tft, bool sdReady, bool wifiConnected) {
    if (!ready_) return;
    width_ = tft.width();
    top_ = SkyUI::kTop;
    bottom_ = tft.height() - SkyUI::kBottom;
    missing_ = 0;
    nightAvailable_ = sdReady && SD.exists("/maps/night");
    if(!themeInitialised_) {
        // Newly installed dual-theme packs start on the darker aviation map.
        // Legacy SD cards remain on their original day tiles unchanged.
        nightMode_ = nightAvailable_;
        themeInitialised_ = true;
    }
    if(!nightAvailable_) nightMode_ = false;
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

    drawRadarOverlay(tft,0,top_,width_,bottom_);

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
