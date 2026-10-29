#include "flights.h"
#include "map_view.h"
#include "geo.h"
#include "aircraft_icon_renderer.h"
#include "aircraft_type_photo.h"
#include "ui_style.h"
#include "detail_layout.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cmath>
#include <cstring>

namespace {
constexpr double kRad = Geo::kPi / 180.0;
constexpr double kEarthNm = 3440.065;
void copyText(char* destination, size_t size, const char* source) {
    if (!source) source = "";
    strncpy(destination, source, size - 1);
    destination[size - 1] = 0;
    // Remove trailing spaces often present in ADS-B callsigns.
    size_t len = strlen(destination);
    while (len && destination[len - 1] == ' ') destination[--len] = 0;
}
float distanceNm(double lat1, double lon1, double lat2, double lon2) {
    const double dLat = (lat2-lat1)*kRad, dLon = (lon2-lon1)*kRad;
    const double a = pow(sin(dLat/2),2) + cos(lat1*kRad)*cos(lat2*kRad)*pow(sin(dLon/2),2);
    return float(2.0 * kEarthNm * asin(sqrt(Geo::clamp(a,0.0,1.0))));
}
int bearingDeg(double lat1,double lon1,double lat2,double lon2) {
    const double dLon=(lon2-lon1)*kRad;
    const double y=sin(dLon)*cos(lat2*kRad);
    const double x=cos(lat1*kRad)*sin(lat2*kRad) -
                   sin(lat1*kRad)*cos(lat2*kRad)*cos(dLon);
    int b=int(std::round(atan2(y,x)/kRad));
    return (b+360)%360;
}
const char* compass(int b) {
    static const char* names[] = {"N","NE","E","SE","S","SW","W","NW"};
    return names[((b+22)/45)%8];
}
}

void FlightTracker::begin(double homeLat,double homeLon,int radiusNm) {
    homeLat_ = homeLat; homeLon_ = homeLon;
    radiusNm_ = constrain(radiusNm, 1, 100);
    count_ = 0; selected_ = -1;
    selectedHex_[0] = 0;
    trail_.clear();
    hasData_ = false;
    paintValid_ = false;
    paintedCount_ = paintedTrailCount_ = 0;
    status_ = "Waiting for ADS-B";
    lastPollMs_ = 0;
    lastGoodPollMs_ = 0;
}
bool FlightTracker::poll() {
    lastPollMs_ = millis();
    if (WiFi.status() != WL_CONNECTED) {
        status_ = "Wi-Fi disconnected";
        pollIntervalMs_ = 20000;
        // Keep the latest confirmed aircraft instead of flashing to empty.
        return false;
    }
    WiFiClientSecure client;
    // Public read-only aircraft data; do not use this insecure connection
    // for firmware, settings, credentials, or any executable payload.
    client.setInsecure();
    client.setTimeout(7000);
    HTTPClient http;
    http.setTimeout(7000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    String url = "https://api.adsb.lol/v2/lat/" + String(homeLat_, 5) +
                 "/lon/" + String(homeLon_, 5) + "/dist/" + String(radiusNm_);
    if (!http.begin(client, url)) {
        status_ = "ADS-B connection failed";
        pollIntervalMs_ = 20000;
        return false;
    }
    http.setUserAgent("CYD-SkyTracker/0.7 (github.com/shrek1802/CYD-SkyTracker)");
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        status_ = String("ADS-B HTTP error ") + code;
        http.end();
        // adsb.lol rate limits vary with load: respect 429 and server stress.
        pollIntervalMs_ = code == 429 ? 90000 :
                          (code == 503 || code == 502 ? 45000 : 20000);
        Serial.printf("[ADSB] HTTP=%d; next request in %u seconds\n",
                      code, unsigned(pollIntervalMs_ / 1000));
        return false;
    }

    JsonDocument filter;
    filter["ac"][0]["hex"] = true;
    filter["ac"][0]["flight"] = true;
    filter["ac"][0]["r"] = true;
    filter["ac"][0]["t"] = true;
    filter["ac"][0]["lat"] = true;
    filter["ac"][0]["lon"] = true;
    filter["ac"][0]["track"] = true;
    filter["ac"][0]["gs"] = true;
    filter["ac"][0]["alt_baro"] = true;
    filter["ac"][0]["alt_geom"] = true;
    filter["ac"][0]["seen_pos"] = true;

    JsonDocument doc;
    const DeserializationError error =
        deserializeJson(doc,http.getStream(),DeserializationOption::Filter(filter));
    http.end();
    if (error) {
        status_ = String("ADS-B JSON error: ") + error.c_str();
        pollIntervalMs_ = 20000;
        return false;
    }
    const JsonArrayConst aircraft=doc["ac"].as<JsonArrayConst>();
    if (aircraft.isNull()) {
        status_ = "ADS-B response has no aircraft list";
        pollIntervalMs_ = 20000;
        return false;
    }

    // Stage the new aircraft table independently. The old table stays
    // visible until the entire valid response has been processed.
    int staged=0;
    const uint32_t now=millis();
    for (JsonVariantConst item:aircraft) {
        if (staged>=MAX_AIRCRAFT) break;
        if (!item["lat"].is<double>() && !item["lat"].is<float>()) continue;
        if (!item["lon"].is<double>() && !item["lon"].is<float>()) continue;
        const float seen=item["seen_pos"] | 0.0f;
        if (!std::isfinite(seen) || seen<0 || seen>45.0f) continue;
        AircraftRecord incoming{};
        incoming.lat=item["lat"].as<double>();
        incoming.lon=item["lon"].as<double>();
        if (!std::isfinite(incoming.lat) || !std::isfinite(incoming.lon) ||
            std::fabs(incoming.lat)>85.05 || std::fabs(incoming.lon)>180.0) continue;
        if (distanceNm(homeLat_,homeLon_,incoming.lat,incoming.lon)>radiusNm_+1.0f)
            continue;
        copyText(incoming.hex,sizeof(incoming.hex),item["hex"] | "");
        if (!incoming.hex[0]) continue;
        copyText(incoming.flight,sizeof(incoming.flight),item["flight"] | "");
        copyText(incoming.registration,sizeof(incoming.registration),item["r"] | "");
        copyText(incoming.type,sizeof(incoming.type),item["t"] | "");
        incoming.heading=item["track"] | 0.0f;
        incoming.speedKt=item["gs"] | -1.0f;
        if (item["alt_baro"].is<int>())
            incoming.altitudeFt=item["alt_baro"].as<int>();
        else if (item["alt_geom"].is<int>())
            incoming.altitudeFt=item["alt_geom"].as<int>();
        incoming.distanceNm=distanceNm(homeLat_,homeLon_,incoming.lat,incoming.lon);
        incoming.bearing=bearingDeg(homeLat_,homeLon_,incoming.lat,incoming.lon);
        incoming.lastConfirmedMs=now;
        incoming.fixMs=now-uint32_t(seen*1000.0f);

        // Retain known identifying fields if an API snapshot omits them.
        for (int previous=0;previous<count_;++previous) {
            const AircraftRecord& before=records_[previous];
            if (strcmp(before.hex,incoming.hex)!=0) continue;
            if(!incoming.flight[0])
                copyText(incoming.flight,sizeof(incoming.flight),before.flight);
            if(!incoming.registration[0])
                copyText(incoming.registration,sizeof(incoming.registration),before.registration);
            if(!incoming.type[0])
                copyText(incoming.type,sizeof(incoming.type),before.type);
            break;
        }
        incoming_[staged++]=incoming;
        if (selectedHex_[0] && strcmp(incoming.hex,selectedHex_)==0)
            trail_.append(incoming.lat,incoming.lon);
    }

    const int confirmed=staged;
    // A plane that is briefly absent from one API snapshot should not blink
    // off the map. Keep it for up to 45 seconds since its last confirmation.
    for (int old=0;old<count_ && staged<MAX_AIRCRAFT;++old) {
        const AircraftRecord& before=records_[old];
        if (!before.hex[0] ||
            !FlightMotion::stillVisible(now,before.lastConfirmedMs)) continue;
        bool present=false;
        for (int n=0;n<staged;++n) {
            if(strcmp(before.hex,incoming_[n].hex)==0) {
                present=true;break;
            }
        }
        if(!present) incoming_[staged++]=before;
    }

    count_=staged;
    selected_=-1;
    for (int i=0;i<count_;++i) {
        records_[i]=incoming_[i];
        if (selectedHex_[0] && strcmp(selectedHex_,records_[i].hex)==0)
            selected_=i;
    }
    hasData_=true;
    lastGoodPollMs_=now;
    pollIntervalMs_=10000; // respectful 10-second snapshots, not streaming
    status_="Live ADS-B";
    Serial.printf("[ADSB] Confirmed %d; visible %d; next poll 10s\n",
                  confirmed,count_);
    return true;
}
void FlightTracker::capturePaint(const MapView& map, PaintedMarker* icons,
                                 int& iconCount, PaintedSegment* segments,
                                 int& segmentCount) const {
    iconCount=0; segmentCount=0;
    if(!hasData_)return;
    if(selected_>=0 && trail_.size()>1) {
        for(int i=1;i<trail_.size();++i) {
            int x1=0,y1=0,x2=0,y2=0;
            const auto& before=trail_.at(i-1);
            const auto& after=trail_.at(i);
            if(map.project(before.lat,before.lon,x1,y1)&&
               map.project(after.lat,after.lon,x2,y2)) {
                segments[segmentCount++]={int16_t(x1),int16_t(y1),
                                          int16_t(x2),int16_t(y2)};
            }
        }
    }
    const uint32_t now=millis();
    for(int i=0;i<count_;++i) {
        int x=0,y=0;
        const AircraftRecord& a=records_[i];
        if(!FlightMotion::stillVisible(now,a.lastConfirmedMs))continue;
        // Predicted movement is bounded to 12 seconds, and is not presented
        // as an additional confirmed ADS-B position.
        const auto position=FlightMotion::predict(
            a.lat,a.lon,a.speedKt,a.heading,uint32_t(now-a.fixMs));
        if(!map.project(position.lat,position.lon,x,y))continue;
        PaintedMarker& p=icons[iconCount++];
        memcpy(p.hex,a.hex,sizeof(p.hex));
        p.x=int16_t(x);p.y=int16_t(y);
        p.heading=int16_t(std::lround(a.heading));
        p.colour=uint32_t(now-a.lastConfirmedMs)>23000?SkyUI::kMuted:
                 a.altitudeFt<0?TFT_CYAN:
                 a.altitudeFt<10000?TFT_YELLOW:TFT_WHITE;
        p.highlighted=(i==selected_);
        p.kind=AircraftSymbol::fromType(a.type);
        p.scale=AircraftSymbol::scale(p.kind,map.zoom(),p.highlighted);
    }
}
bool FlightTracker::samePaint(const PaintedMarker& a,const PaintedMarker& b) {
    return strcmp(a.hex,b.hex)==0&&
           a.x==b.x&&a.y==b.y&&a.heading==b.heading&&
           a.colour==b.colour&&a.highlighted==b.highlighted&&
           a.kind==b.kind&&a.scale==b.scale;
}
void FlightTracker::drawMarkers(TFT_eSPI& tft,const MapView& map) {
    PaintedMarker fresh[MAX_AIRCRAFT];
    PaintedSegment segments[FlightTrail::kCapacity-1];
    int freshCount=0,segmentCount=0;
    capturePaint(map,fresh,freshCount,segments,segmentCount);
    for(int i=0;i<segmentCount;++i) {
        const auto& a=segments[i];
        // Most recent confirmed flight path is brightest; older legs fade.
        const uint16_t trailColour=i>segmentCount-5?0x77FF:
            i>segmentCount-11?0x45AC:0x2B88;
        tft.drawLine(a.x1,a.y1,a.x2,a.y2,trailColour);
    }
    for(int i=0;i<freshCount;++i) {
        const auto& a=fresh[i];
        AircraftIcon::draw(tft,a.x,a.y,float(a.heading),a.colour,a.highlighted,
                           a.kind,a.scale);
    }
    paintedCount_=freshCount;
    paintedTrailCount_=segmentCount;
    for(int i=0;i<freshCount;++i)painted_[i]=fresh[i];
    for(int i=0;i<segmentCount;++i)paintedTrail_[i]=segments[i];
    paintValid_=true;
}

void FlightTracker::drawChanges(TFT_eSPI& tft,MapView& map,bool sdReady) {
    if(!paintValid_) {
        drawMarkers(tft,map);
        return;
    }
    PaintedMarker fresh[MAX_AIRCRAFT];
    PaintedSegment freshTrail[FlightTrail::kCapacity-1];
    int count=0,segments=0;
    capturePaint(map,fresh,count,freshTrail,segments);

    // The old renderer first erased EVERY moved aircraft and only THEN drew
    // all the aircraft, so a slow SD card produced a visible empty sky.
    // Restore each small background patch and immediately paint any planes
    // and trails that intersect it; never leave the entire overlay blank.
    auto repaintPatch=[&](int x,int y,int w,int h) {
        if(w<=0||h<=0)return;
        map.restoreRect(tft,x,y,w,h,sdReady);
        const int right=x+w, bottom=y+h;
        for(int i=0;i<segments;++i) {
            const auto& line=freshTrail[i];
            if(std::max(int(line.x1),int(line.x2)) < x-2 ||
               std::min(int(line.x1),int(line.x2)) > right+2 ||
               std::max(int(line.y1),int(line.y2)) < y-2 ||
               std::min(int(line.y1),int(line.y2)) > bottom+2)continue;
            const uint16_t trailColour=i>segments-5?0x77FF:
                i>segments-11?0x45AC:0x2B88;
            tft.drawLine(line.x1,line.y1,line.x2,line.y2,trailColour);
        }
        for(int i=0;i<count;++i) {
            const auto& a=fresh[i];
            if(a.x+19<x||a.x-19>right||a.y+19<y||a.y-19>bottom)continue;
            AircraftIcon::draw(tft,a.x,a.y,float(a.heading),
                               a.colour,a.highlighted,a.kind,a.scale);
        }
    };

    bool changed=false;
    for(int i=0;i<paintedCount_;++i) {
        bool same=false;
        for(int j=0;j<count;++j) {
            if(samePaint(painted_[i],fresh[j])) {same=true;break;}
        }
        if(!same) {
            repaintPatch(painted_[i].x-20,painted_[i].y-20,40,40);
            changed=true;
        }
    }
    bool trailChanged=paintedTrailCount_!=segments;
    if(!trailChanged) {
        for(int i=0;i<segments;++i) {
            const auto& a=paintedTrail_[i];
            const auto& b=freshTrail[i];
            if(a.x1!=b.x1||a.y1!=b.y1||a.x2!=b.x2||a.y2!=b.y2) {
                trailChanged=true;break;
            }
        }
    }
    if(trailChanged) {
        for(int i=0;i<paintedTrailCount_;++i) {
            const auto& a=paintedTrail_[i];
            const int x=std::min(int(a.x1),int(a.x2))-2;
            const int y=std::min(int(a.y1),int(a.y2))-2;
            const int w=std::abs(int(a.x2)-int(a.x1))+5;
            const int h=std::abs(int(a.y2)-int(a.y1))+5;
            repaintPatch(x,y,w,h);
        }
        changed=true;
    }
    if(!changed) {
        if(count!=paintedCount_)changed=true;
        else for(int i=0;i<count;++i) {
            bool found=false;
            for(int j=0;j<paintedCount_;++j) {
                if(samePaint(fresh[i],painted_[j])) {found=true;break;}
            }
            if(!found){changed=true;break;}
        }
    }
    // Draw new positions and copy the tracked overlay state. Painting over
    // an unchanged icon is cheap and doesn't expose blank map pixels.
    if(changed)drawMarkers(tft,map);
}
bool FlightTracker::selectAt(int x,int y,const MapView& map) {
    int best=-1, bestDistance2=17*17;
    for (int i=0;i<count_;++i) {
        int px=0,py=0;
        const auto& rec=records_[i];
        if(!FlightMotion::stillVisible(millis(),rec.lastConfirmedMs))continue;
        const auto predicted=FlightMotion::predict(rec.lat,rec.lon,
                rec.speedKt,rec.heading,uint32_t(millis()-rec.fixMs));
        if (!map.project(predicted.lat,predicted.lon,px,py)) continue;
        int d=(px-x)*(px-x)+(py-y)*(py-y);
        if (d < bestDistance2) {best=i;bestDistance2=d;}
    }
    if (best < 0) {
        deselect();
        return false;
    }
    if (strcmp(selectedHex_, records_[best].hex) != 0) {
        trail_.clear();
        copyText(selectedHex_, sizeof(selectedHex_), records_[best].hex);
    }
    selected_ = best;
    trail_.append(records_[best].lat, records_[best].lon);
    return true;
}
void FlightTracker::deselect() {
    selected_ = -1;
    selectedHex_[0] = 0;
    trail_.clear();
}

void FlightTracker::updateDetailsTelemetry(TFT_eSPI& tft) const {
    if (!hasSelection()) return;
    const AircraftRecord& a = records_[selected_];

    // Compact two-row dashboard. On each ADS-B poll only these four cards
    // are repainted; the SD aircraft image and navigation stay untouched.
    struct Reading {
        DetailLayout::Rect box;
        const char* label;
        String value;
        uint16_t colour;
    };
    const Reading rows[] = {
        {DetailLayout::kAltitude, "ALT", a.altitudeFt>=0 ?
            String(a.altitudeFt)+" FT" : String("--- FT"), SkyUI::kWhite},
        {DetailLayout::kSpeed, "SPD", a.speedKt>=0 ?
            String(a.speedKt,0)+" KT" : String("--- KT"), SkyUI::kAccent},
        {DetailLayout::kDistance, "DIST",
            String(a.distanceNm,1)+" NM", SkyUI::kWhite},
        {DetailLayout::kTrack, "HDG", String(int(a.heading+0.5f)%360)+
            " "+DetailLayout::compass16(a.heading), SkyUI::kGold}
    };
    tft.setTextSize(1);
    for (const auto& card : rows) {
        const auto& b=card.box;
        tft.fillRoundRect(b.x,b.y,b.w,b.h,4,SkyUI::kPanel);
        tft.drawFastVLine(b.x+2,b.y+4,b.h-8,SkyUI::kBorder);
        tft.setTextColor(SkyUI::kMuted,SkyUI::kPanel);
        tft.setCursor(b.x+7,b.y+6);
        tft.print(card.label);
        const int valueX=b.x+(strcmp(card.label,"DIST")==0?43:37);
        tft.setTextColor(card.colour,SkyUI::kPanel);
        tft.setCursor(valueX,b.y+6);
        tft.print(card.value.substring(0,17));
    }
}
void FlightTracker::drawDetails(TFT_eSPI& tft) const {
    if (!hasSelection()) return;
    const AircraftRecord& a=records_[selected_];

    tft.fillScreen(SkyUI::kBackground);
    // Slim branded flight strip: large, readable callsign plus the real
    // registration. Neither text is copied onto an unrelated type image.
    tft.fillRect(0,0,tft.width(),29,SkyUI::kPanel);
    tft.fillRect(0,27,tft.width(),2,SkyUI::kAccent);
    tft.setTextSize(2);
    tft.setTextColor(SkyUI::kWhite,SkyUI::kPanel);
    tft.setCursor(8,6);
    tft.print(String(a.flight[0]?a.flight:a.hex).substring(0,12));
    tft.setTextSize(1);
    tft.setTextColor(SkyUI::kGold,SkyUI::kPanel);
    const String reg=a.registration[0]?String(a.registration):String("NO REG");
    tft.setCursor(tft.width()-8-6*int(std::min(size_t(15),reg.length())),12);
    tft.print(reg.substring(0,15));

    const auto* model=AircraftTypePhoto::find(a.type);
    const String modelText=model?String(model->model):
        (a.type[0] ? String("ICAO ")+a.type : String("AIRCRAFT TYPE UNKNOWN"));
    tft.setTextColor(SkyUI::kAccent,SkyUI::kBackground);
    tft.setCursor(9,34);
    // 50 characters at 6px wide fit the tested 320px display.
    tft.print(modelText.substring(0,49));

    // 304x110 photograph is drawn by PhotoView::drawHero, not here.
    // Keep photo, credit, four stats and navigation in fixed disjoint areas.
    updateDetailsTelemetry(tft);
    SkyUI::button(tft,DetailLayout::kPhotoButton.x,DetailLayout::kPhotoButton.y,
                  DetailLayout::kPhotoButton.w,DetailLayout::kPhotoButton.h,
                  "ENLARGE PHOTO",true);
    SkyUI::button(tft,DetailLayout::kMapButton.x,DetailLayout::kMapButton.y,
                  DetailLayout::kMapButton.w,DetailLayout::kMapButton.h,
                  "BACK TO MAP");
}
