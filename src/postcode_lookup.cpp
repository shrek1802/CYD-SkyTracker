#include "postcode_lookup.h"
#include "postcode_ca.h"
#include "uk_postcode.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cmath>
#include <ctime>

namespace PostcodeLookup {
bool resolve(const String& input, Location& output, String& reason) {
    std::string compact=UkPostcode::compact(std::string(input.c_str()));
    if(!UkPostcode::validCompact(compact)) {
        reason="Enter a full UK postcode";
        return false;
    }
    if(WiFi.status()!=WL_CONNECTED) {
        reason="Connect to home Wi-Fi first";
        return false;
    }
    // CA validation requires a correct UTC clock. Fail closed if unavailable.
    if(std::time(nullptr)<1700000000) {
        configTime(0,0,"pool.ntp.org","time.google.com");
        const uint32_t started=millis();
        while(std::time(nullptr)<1700000000 && millis()-started<7500)
            delay(150);
    }
    if(std::time(nullptr)<1700000000) {
        reason="Clock not ready for secure postcode lookup";
        return false;
    }
    WiFiClientSecure client;
    client.setCACert(PostcodeTLS::kCa);
    client.setTimeout(8000);
    HTTPClient http;
    http.setTimeout(8000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    // Pass only the postcode. Never send saved Wi-Fi credentials.
    const String url=String("https://api.postcodes.io/postcodes/")+compact.c_str();
    if(!http.begin(client,url)) {
        reason="Postcode service connection failed";
        return false;
    }
    http.setUserAgent("CYD-SkyTracker-UK-Setup/1.0");
    const int status=http.GET();
    if(status!=HTTP_CODE_OK) {
        reason=status==404?"UK postcode not found":
               String("Lookup service or TLS error ")+status;
        http.end();
        return false;
    }
    JsonDocument filter;
    filter["status"]=true;
    filter["result"]["postcode"]=true;
    filter["result"]["latitude"]=true;
    filter["result"]["longitude"]=true;
    JsonDocument doc;
    const auto err=deserializeJson(doc,http.getStream(),
                                   DeserializationOption::Filter(filter));
    http.end();
    if(err || doc["status"].as<int>()!=200 ||
       !doc["result"]["latitude"].is<double>() ||
       !doc["result"]["longitude"].is<double>()) {
        reason="Postcode response has no location";
        return false;
    }
    const double lat=doc["result"]["latitude"].as<double>();
    const double lon=doc["result"]["longitude"].as<double>();
    if(!std::isfinite(lat)||!std::isfinite(lon) ||
       lat<49 || lat>61.5 || lon< -11 || lon>3.5) {
        reason="Postcode centre is outside supported UK coverage";
        return false;
    }
    output.latitude=float(lat);
    output.longitude=float(lon);
    output.postcode=doc["result"]["postcode"].as<String>();
    if(output.postcode.length()==0)
        output.postcode=UkPostcode::format(compact).c_str();
    reason="";
    return true;
}
}
