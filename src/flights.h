#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "flight_trail.h"
#include "flight_motion.h"
#include "aircraft_symbol.h"
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
    uint32_t fixMs = 0;          // time of the measured position
    uint32_t lastConfirmedMs = 0; // last successful sighting in the API
};

class FlightTracker {
public:
    static constexpr int MAX_AIRCRAFT = 60;
    void begin(double homeLat, double homeLon, int radiusNm = 60);
    bool poll();
    uint32_t pollIntervalMs() const { return pollIntervalMs_; }
    void drawMarkers(TFT_eSPI& tft, const MapView& map);
    // On aircraft polls restore only changed icon/trail footprints.
    void drawChanges(TFT_eSPI& tft, MapView& map, bool sdReady);
    void invalidatePaint() { paintValid_ = false; }
    bool selectAt(int x, int y, const MapView& map);
    bool hasSelection() const { return selected_ >= 0 && selected_ < count_; }
    void deselect();
    void drawDetails(TFT_eSPI& tft) const;
    void updateDetailsTelemetry(TFT_eSPI& tft) const;
    const AircraftRecord* selectedRecord() const { return hasSelection() ? &records_[selected_] : nullptr; }
    int count() const { return count_; }
    bool hasData() const { return hasData_; }
    const String& status() const { return status_; }
    uint32_t lastPollMs() const { return lastPollMs_; }
    uint32_t lastGoodPollMs() const { return lastGoodPollMs_; }
private:
    AircraftRecord records_[MAX_AIRCRAFT] = {};
    AircraftRecord incoming_[MAX_AIRCRAFT] = {}; // stage data without blanking old planes
    int count_ = 0, selected_ = -1;
    char selectedHex_[9] = {};
    FlightTrail trail_;
    struct PaintedMarker {
        char hex[9] = {};
        int16_t x = 0, y = 0, heading = 0;
        uint16_t colour = 0;
        bool highlighted = false;
        AircraftSymbol::Kind kind = AircraftSymbol::Kind::Airliner;
        float scale = 0.86f;
    };
    struct PaintedSegment { int16_t x1, y1, x2, y2; };
    PaintedMarker painted_[MAX_AIRCRAFT] = {};
    PaintedSegment paintedTrail_[FlightTrail::kCapacity - 1] = {};
    int paintedCount_ = 0, paintedTrailCount_ = 0;
    bool paintValid_ = false;
    void capturePaint(const MapView& map, PaintedMarker* icons,
                      int& iconCount, PaintedSegment* segments,
                      int& segmentCount) const;
    static bool samePaint(const PaintedMarker& a, const PaintedMarker& b);
    double homeLat_ = 0, homeLon_ = 0;
    int radiusNm_ = 60;
    bool hasData_ = false;
    String status_ = "Waiting for ADS-B";
    uint32_t lastPollMs_ = 0;
    uint32_t lastGoodPollMs_ = 0;
    uint32_t pollIntervalMs_ = 10000; // 10-sec API polls, back off on failures
};
