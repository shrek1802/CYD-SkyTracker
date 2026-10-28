#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Optional, live on-demand Wikimedia Commons thumbnail. Only searches for
// an exact aircraft registration and accepted freely reusable licences.
// No SD photo cache; temporary JPEG held in RAM only while details open.
class PhotoView {
public:
    PhotoView() = default;
    ~PhotoView() { clear(); }
    void clear();
    void searching() { status_ = "Searching aircraft photo..."; }
    void load(const char* registration);
    void draw(TFT_eSPI& display) const;
    void drawCredit(TFT_eSPI& display) const;
    const String& status() const { return status_; }
private:
    uint8_t* jpeg_ = nullptr;
    size_t size_ = 0;
    String status_ = "No photo loaded";
    String artist_;
    String licence_;
    String source_;
};
