#include "flights.h"
#include "map_view.h"
#include "geo.h"
#include "aircraft_icon_renderer.h"
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
}
bool FlightTracker::poll() {
    lastPollMs_ = millis();
    if (WiFi.status() != WL_CONNECTED) { status_ = "Wi-Fi disconnected"; return false; }
    WiFiClientSecure client;
    // Development build: server CA/pinned trust is not yet provisioned.
    // Read-only aircraft locations, never accept configuration or firmware
    // over this insecure data transport.
    client.setInsecure();
    client.setTimeout(9000);
    HTTPClient http;
    http.setTimeout(9000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    String url = "https://api.adsb.lol/v2/lat/" + String(homeLat_, 5) +
                 "/lon/" + String(homeLon_, 5) + "/dist/" + String(radiusNm_);
    if (!http.begin(client, url)) { status_ = "ADS-B connection failed"; return false; }
    http.setUserAgent("CYD-SkyTracker-test/0.1 (github.com/shrek1802/CYD-SkyTracker)");
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        status_ = String("ADS-B HTTP error ") + code;
        http.end();
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
    DeserializationError error = deserializeJson(doc, http.getStream(),
                                                  DeserializationOption::Filter(filter));
    http.end();
    if (error) {
        status_ = String("ADS-B JSON error: ") + error.c_str();
        return false;
    }
    JsonArrayConst aircraft = doc["ac"].as<JsonArrayConst>();
    if (aircraft.isNull()) {
        status_ = "ADS-B response has no aircraft list";
        return false;
    }
    // Successful zero-aircraft response is not an error.
    count_ = 0; selected_ = -1;
    for (JsonVariantConst item : aircraft) {
        if (count_ >= MAX_AIRCRAFT) break;
        if (!item["lat"].is<double>() && !item["lat"].is<float>()) continue;
        if (!item["lon"].is<double>() && !item["lon"].is<float>()) continue;
        if (!item["seen_pos"].isNull() &&
            item["seen_pos"].as<float>() > 45.0f) continue;
        AircraftRecord& a = records_[count_];
        a = AircraftRecord{};
        a.lat = item["lat"].as<double>();
        a.lon = item["lon"].as<double>();
        if (!isfinite(a.lat) || !isfinite(a.lon) || fabs(a.lat) > 85.05 ||
            fabs(a.lon) > 180.0) continue;
        a.distanceNm = distanceNm(homeLat_, homeLon_, a.lat, a.lon);
        if (a.distanceNm > radiusNm_ + 1.0f) continue;
        a.bearing = bearingDeg(homeLat_,homeLon_,a.lat,a.lon);
        copyText(a.hex,sizeof(a.hex),item["hex"] | "");
        copyText(a.flight,sizeof(a.flight),item["flight"] | "");
        copyText(a.registration,sizeof(a.registration),item["r"] | "");
        copyText(a.type,sizeof(a.type),item["t"] | "");
        // Preserve a tapped aircraft through the next API refresh.
        // Earlier builds reset selection to -1 every 30 seconds.
        if (selectedHex_[0] && strcmp(a.hex, selectedHex_) == 0) {
            selected_ = count_;
            trail_.append(a.lat, a.lon);
        }
        a.heading = item["track"] | 0.0f;
        a.speedKt = item["gs"] | -1.0f;
        if (item["alt_baro"].is<int>()) a.altitudeFt = item["alt_baro"].as<int>();
        else if (item["alt_geom"].is<int>()) a.altitudeFt = item["alt_geom"].as<int>();
        ++count_;
    }
    hasData_ = true;
    status_ = "Live ADS-B";
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
    for(int i=0;i<count_;++i) {
        int x=0,y=0;
        const AircraftRecord& a=records_[i];
        if(!map.project(a.lat,a.lon,x,y))continue;
        PaintedMarker& p=icons[iconCount++];
        memcpy(p.hex,a.hex,sizeof(p.hex));
        p.x=int16_t(x);p.y=int16_t(y);
        p.heading=int16_t(std::lround(a.heading));
        p.colour=a.altitudeFt<0?TFT_CYAN:
                 a.altitudeFt<10000?TFT_YELLOW:TFT_WHITE;
        p.highlighted=(i==selected_);
    }
}
bool FlightTracker::samePaint(const PaintedMarker& a,const PaintedMarker& b) {
    return strcmp(a.hex,b.hex)==0&&
           a.x==b.x&&a.y==b.y&&a.heading==b.heading&&
           a.colour==b.colour&&a.highlighted==b.highlighted;
}
void FlightTracker::drawMarkers(TFT_eSPI& tft,const MapView& map) {
    PaintedMarker fresh[MAX_AIRCRAFT];
    PaintedSegment segments[FlightTrail::kCapacity-1];
    int freshCount=0,segmentCount=0;
    capturePaint(map,fresh,freshCount,segments,segmentCount);
    for(int i=0;i<segmentCount;++i) {
        const auto& a=segments[i];
        tft.drawLine(a.x1,a.y1,a.x2,a.y2,TFT_CYAN);
    }
    for(int i=0;i<freshCount;++i) {
        const auto& a=fresh[i];
        AircraftIcon::draw(tft,a.x,a.y,float(a.heading),a.colour,a.highlighted);
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
    bool change=false;
    for(int i=0;i<paintedCount_;++i) {
        bool same=false;
        for(int j=0;j<count;++j) {
            if(samePaint(painted_[i],fresh[j])) {same=true;break;}
        }
        if(!same) {
            // A small (~40x40) background repair, rather than wiping 320x240
            // and re-streaming every tile from microSD each 30 seconds.
            map.restoreRect(tft,painted_[i].x-20,painted_[i].y-20,
                            40,40,sdReady);
            change=true;
        }
    }
    bool trailChanged=paintedTrailCount_!=segments;
    if(!trailChanged) {
        for(int i=0;i<segments;++i) {
            const auto& a=paintedTrail_[i];const auto& b=freshTrail[i];
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
            const int w=abs(int(a.x2)-int(a.x1))+5;
            const int h=abs(int(a.y2)-int(a.y1))+5;
            map.restoreRect(tft,x,y,w,h,sdReady);
        }
        change=true;
    }
    if(!change) {
        if(count!=paintedCount_)change=true;
        else for(int i=0;i<count;++i) {
            bool found=false;
            for(int j=0;j<paintedCount_;++j) {
                if(samePaint(fresh[i],painted_[j])) {found=true;break;}
            }
            if(!found) {change=true;break;}
        }
    }
    if(change)drawMarkers(tft,map);
}

bool FlightTracker::selectAt(int x,int y,const MapView& map) {
    int best=-1, bestDistance2=17*17;
    for (int i=0;i<count_;++i) {
        int px=0,py=0;
        if (!map.project(records_[i].lat,records_[i].lon,px,py)) continue;
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
    if(!hasSelection())return;
    const AircraftRecord& a=records_[selected_];
    // The entire upper 160px (title/photo) is intentionally untouched.
    // Only three thin rows of values are updated as ADS-B data changes.
    tft.fillRect(0,161,tft.width(),51,TFT_BLACK);
    tft.setTextSize(1);
    auto item=[&](int x,int y,const char* label,const String& value) {
        tft.setTextColor(TFT_CYAN,TFT_BLACK);
        tft.setCursor(x,y);
        tft.print(label);
        tft.setTextColor(TFT_WHITE,TFT_BLACK);
        tft.print(value.substring(0,19));
    };
    item(8,165,"REG ",a.registration[0]?a.registration:"Unknown");
    item(165,165,"ALT ",a.altitudeFt>=0?String(a.altitudeFt)+"ft":"Unknown");
    item(8,181,"SPD ",a.speedKt>=0?String(a.speedKt,0)+"kt":"Unknown");
    item(165,181,"DIST ",String(a.distanceNm,1)+"NM");
    item(8,197,"TYPE ",a.type[0]?a.type:"Unknown");
    item(165,197,"HDG ",String(a.heading,0));
}
void FlightTracker::drawDetails(TFT_eSPI& tft) const {
    if(!hasSelection())return;
    const AircraftRecord& a=records_[selected_];
    tft.fillScreen(TFT_BLACK);
    tft.fillRect(0,0,tft.width(),30,0x08A3);
    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN,0x08A3);
    tft.setCursor(8,6);
    tft.print(String(a.flight[0]?a.flight:a.hex).substring(0,11));
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW,0x08A3);
    tft.setCursor(tft.width()-49,11);
    tft.print("< BACK");

    updateDetailsTelemetry(tft);

    tft.fillRect(0,230,tft.width(),10,0x08A3);
    tft.setTextColor(TFT_WHITE,0x08A3);
    tft.setCursor(8,231);
    tft.print("PHOTO IS A MODEL EXAMPLE - NOT THIS AIRFRAME");
}
