#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Photo lookup uses ICAO aircraft *type* first. The shown image is explicitly
// labelled "Representative" and never presented as the tracked airframe.
// Optional /aircraft/ SD JPEGs avoid dependence on internet image searches.
class PhotoView {
public:
    PhotoView() = default;
    ~PhotoView() { clear(); }
    void clear();
    void searching() { status_ = "Searching aircraft photo..."; }
    void load(const char* registration, const char* type, bool sdReady,
              bool sdOnly = false);
    void draw(TFT_eSPI& display) const; // Legacy PHOTO_TEST, keeps serial diagnosis
    void drawThumbnail(TFT_eSPI& display) const;
    void drawHero(TFT_eSPI& display) const; // 304x110 photo-led flight card
    void drawFullscreen(TFT_eSPI& display) const;
    void drawCreditAt(TFT_eSPI& display, int y) const;
    void drawCredit(TFT_eSPI& display) const;
    bool hasPhoto() const { return rgbReady_ || (jpeg_ && size_); }
    const String& status() const { return status_; }
private:
    uint8_t* jpeg_ = nullptr;
    size_t size_ = 0;
    String status_ = "No photo loaded";
    String artist_;
    String licence_;
    String source_;
    String model_;
    bool representative_ = false;
    bool rgbReady_ = false;
    String rgbPath_;
};
