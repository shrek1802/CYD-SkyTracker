#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "flight_trail.h"
class MapView;

struct AircraftRecord {
    char hex[9] = {};
    char flight[12] = {};
    char registration[16] = {};
    char type[12] = {};
    double lat = 0, lon = 0;
    float heading = 0;
    float speedKt = -1;
    int altitudeFt = -1;
    float distanceNm = 0;
    int bearing = 0;
};

class FlightTracker {
public:
    static constexpr int MAX_AIRCRAFT = 60;
    void begin(double homeLat, double homeLon, int radiusNm = 60);
    bool poll();
    void drawMarkers(TFT_eSPI& tft, const MapView& map) const;
    bool selectAt(int x, int y, const MapView& map);
    bool hasSelection() const { return selected_ >= 0 && selected_ < count_; }
    void deselect();
    void drawDetails(TFT_eSPI& tft) const;
    const AircraftRecord* selectedRecord() const { return hasSelection() ? &records_[selected_] : nullptr; }
    int count() const { return count_; }
    bool hasData() const { return hasData_; }
    const String& status() const { return status_; }
    uint32_t lastPollMs() const { return lastPollMs_; }
private:
    AircraftRecord records_[MAX_AIRCRAFT] = {};
    int count_ = 0, selected_ = -1;
    char selectedHex_[9] = {};
    FlightTrail trail_;
    double homeLat_ = 0, homeLon_ = 0;
    int radiusNm_ = 60;
    bool hasData_ = false;
    String status_ = "Waiting for ADS-B";
    uint32_t lastPollMs_ = 0;
};
