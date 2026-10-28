#include "photo_view.h"
#include "photo_match.h"
#include "aircraft_type_photo.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <SD.h>
#include <TJpg_Decoder.h>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {
constexpr size_t kMaxJpegBytes = 90000;
constexpr int kX=7, kY=39, kW=150, kH=99;
TFT_eSPI* currentDisplay = nullptr;

bool drawJpegBlock(int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t* bitmap) {
    if (!currentDisplay || x >= kX+kW || y >= kY+kH) return false;
    if (x < kX || y < kY) return false;
    const uint16_t visibleRows = uint16_t(std::min<int>(int(h),kY+kH-y));
    currentDisplay->pushImage(x,y,w,visibleRows,bitmap);
    return true;
}
String plainText(const String& html) {
    String out;out.reserve(78);
    bool tag=false;
    for (size_t i=0;i<html.length()&&out.length()<70;++i) {
        char c=html[i];
        if (c=='<') {tag=true;continue;}
        if (c=='>') {tag=false;continue;}
        if (!tag && c>=' ' && c<='~') out+=c;
    }
    out.replace("&amp;","&"); out.replace("&quot;","'");
    out.replace("&#39;","'"); out.replace("&nbsp;"," ");
    return out;
}
bool acceptedLicence(String licence) {
    licence.trim();licence.toUpperCase();
    return licence=="CC0"||licence.startsWith("CC0 ")||
           licence=="PUBLIC DOMAIN"||licence.startsWith("PUBLIC DOMAIN")||
           licence.startsWith("CC BY ")||licence.startsWith("CC BY-SA ")||
           licence.startsWith("CC-BY-")||licence.startsWith("CC-BY-SA-");
}
bool allowedImageUrl(const String& url) {
    return url.startsWith("https://upload.wikimedia.org/")&&url.length()<1024;
}
String encode(const String& input) {
    String out;out.reserve(input.length()*3);
    char hex[4]={};
    for(size_t i=0;i<input.length();++i) {
        unsigned char c=(unsigned char)input[i];
        if (std::isalnum(c)||c=='-'||c=='_'||c=='.') out+=(char)c;
        else {snprintf(hex,sizeof(hex),"%%%02X",(unsigned)c);out+=hex;}
    }
    return out;
}
bool validRegistration(const String& reg) {
    if(reg.length()<4||reg.length()>14)return false;
    for(size_t i=0;i<reg.length();++i) {
        char c=reg[i];
        if (!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'))return false;
    }
    return true;
}
bool photoLooksAppropriate(String title) {
    title.toLowerCase();
    static const char* bad[]={"cockpit","interior","cabin","safety card",
        "seat map","flight deck","logo","diagram","simulation","illustration",
        "engine detail","museum display","scale model","wing view"};
    for(const char* word:bad) if(title.indexOf(word)>=0)return false;
    return true;
}
}

// Free the currently displayed photo when exiting the selected aircraft.
void PhotoView::clear() {
    if(jpeg_)free(jpeg_);
    jpeg_=nullptr;size_=0;artist_="";licence_="";source_="";
    model_="";representative_=false;status_="No photo loaded";
}
void PhotoView::load(const char* registration,const char* type,bool sdReady) {
    clear();
    const auto* model=AircraftTypePhoto::find(type);
    String reg=registration?String(registration):String("");
    reg.toUpperCase();reg.trim();
    const bool haveReg=validRegistration(reg);
    if(model) {
        model_=model->model;
        representative_=true;
        Serial.printf("[PHOTO] Type %s -> %s; registration %s\n",
            model->code,model->model,haveReg?reg.c_str():"unknown");
    }

    // Prefer an optional self-contained SD photo package. This is the most
    // reliable method, and it works without an extra Wikimedia web request.
    // Every JPEG must have a matching three-line attribution file.
    if(model&&sdReady) {
        const String file=String("/aircraft/")+model->code+".jpg";
        const String meta=String("/aircraft/")+model->code+".txt";
        File credit=SD.open(meta,FILE_READ);
        if(credit) {
            const String artist=credit.readStringUntil('\n');
            const String license=credit.readStringUntil('\n');
            const String source=credit.readStringUntil('\n');
            credit.close();
            File picture=SD.open(file,FILE_READ);
            const size_t bytes=picture?picture.size():0;
            if(picture&&bytes>=16&&bytes<=kMaxJpegBytes&&
               acceptedLicence(license)&&source.startsWith("https://commons.wikimedia.org/")&&
               ESP.getFreeHeap()>bytes+40000) {
                uint8_t* data=(uint8_t*)malloc(bytes);
                if(data) {
                    const size_t got=picture.readBytes((char*)data,bytes);
                    if(got==bytes&&data[0]==0xff&&data[1]==0xd8) {
                        jpeg_=data;size_=bytes;artist_=artist;licence_=license;
                        source_=source;status_="Representative type photo";
                        Serial.printf("[PHOTO] SD type photo loaded %s (%u bytes)\n",file.c_str(),unsigned(bytes));
                        picture.close();return;
                    }
                    free(data);
                }
            }
            if(picture)picture.close();
            Serial.printf("[PHOTO] SD type photo unavailable/invalid: %s\n",file.c_str());
        }
    }
    if(WiFi.status()!=WL_CONNECTED) {
        status_=model?"Type photo needs Wi-Fi/SD":"No model photo";
        return;
    }

    String thumbnailUrl;
    bool matchedExact=false;
    int checked=0;
    // Result meanings: -1 = HTTP error, 0 = no matches, 1 = valid metadata.
    auto lookup=[&](const String& query,bool exact,bool category)->int{
        WiFiClientSecure client;
        client.setInsecure(); // public read-only media only; production requires CAs
        client.setTimeout(7000);
        HTTPClient http;
        http.setTimeout(7000);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        const String url=String("https://commons.wikimedia.org/w/api.php?action=query")+
            query+"&prop=imageinfo&iiprop=url%7Cextmetadata%7Cmime"+
            "&iiextmetadatafilter=Artist%7CLicenseShortName"+
            "&iiurlwidth=280&format=json&formatversion=2";
        if(!http.begin(client,url)) {status_="Photo server unavailable";return -1;}
        http.setUserAgent("CYD-SkyTracker/0.4 (github.com/shrek1802/CYD-SkyTracker)");
        int code=http.GET();
        if(code!=HTTP_CODE_OK) {
            status_=String("Photo server HTTP ")+code;
            Serial.printf("[PHOTO] Commons metadata HTTP %d\n",code);
            http.end();return -1;
        }
        JsonDocument filter;
        JsonObject page=filter["query"]["pages"][0].to<JsonObject>();
        page["title"]=true;
        JsonObject info=page["imageinfo"][0].to<JsonObject>();
        info["mime"]=true;info["thumburl"]=true;info["descriptionurl"]=true;
        info["extmetadata"]["Artist"]["value"]=true;
        info["extmetadata"]["LicenseShortName"]["value"]=true;
        JsonDocument doc;
        const auto error=deserializeJson(doc,http.getStream(),DeserializationOption::Filter(filter));
        http.end();
        if(error) {
            status_="Photo metadata error";
            Serial.printf("[PHOTO] JSON error: %s\n",error.c_str());return -1;
        }
        const JsonArrayConst pages=doc["query"]["pages"].as<JsonArrayConst>();
        int candidates=0;
        for(JsonVariantConst item:pages) {
            ++checked;++candidates;
            const String title=item["title"].as<String>();
            if(exact&&!category&&!PhotoMatch::titleHasRegistration(title.c_str(),reg.c_str()))
                continue;
            if(!photoLooksAppropriate(title))continue;
            const JsonObjectConst image=item["imageinfo"][0].as<JsonObjectConst>();
            if(image["mime"].as<String>()!="image/jpeg")continue;
            const JsonObjectConst md=image["extmetadata"].as<JsonObjectConst>();
            String licence=md["LicenseShortName"]["value"].as<String>();
            if(!acceptedLicence(licence))continue;
            const String candidate=image["thumburl"].as<String>();
            if(!allowedImageUrl(candidate))continue;
            thumbnailUrl=candidate;
            artist_=plainText(md["Artist"]["value"].as<String>());
            if(artist_.isEmpty())artist_="Wikimedia contributor";
            licence_=licence;
            source_=image["descriptionurl"].as<String>();
            matchedExact=exact;
            Serial.printf("[PHOTO] %s photo match: %s | %s\n",
                          exact?"Exact airframe":"Representative type",
                          title.c_str(),licence.c_str());
            return 1;
        }
        Serial.printf("[PHOTO] %s candidates %d, accepted 0\n",category?"Category":"Search",candidates);
        return 0;
    };
    int result=0;
    if(model) {
        // A type-specific Wikimedia Commons category can contain images whose
        // filenames omit the model entirely. Category membership supplies the
        // type, so this path does not require an exact filename match.
        const String catQuery="&generator=categorymembers&gcmtitle="+
            encode(String("Category:")+model->commonsCategory)+
            "&gcmnamespace=6&gcmlimit=20";
        result=lookup(catQuery,false,true);
        if(result==0) {
            const String searchQuery="&generator=search&gsrnamespace=6&gsrlimit=12&gsrsearch="+
                encode(String("intitle:\"")+model->model+"\"");
            result=lookup(searchQuery,false,false);
        }
    } else if(haveReg) {
        const String catQuery="&generator=categorymembers&gcmtitle="+
            encode(String("Category:")+reg+" (aircraft)")+
            "&gcmnamespace=6&gcmlimit=20";
        result=lookup(catQuery,true,true);
        if(result==0) {
            const String searchQuery="&generator=search&gsrnamespace=6&gsrlimit=12&gsrsearch="+
                encode(String("\"")+reg+"\"");
            result=lookup(searchQuery,true,false);
        }
    }
    if(thumbnailUrl.isEmpty()) {
        if(result>=0)status_=model?"No model photo found":"No exact aircraft photo";
        Serial.printf("[PHOTO] %s (type=%s, reg=%s, examined=%d)\n",
                      status_.c_str(),type?type:"",reg.c_str(),checked);
        return;
    }

    // Download one bounded JPEG thumbnail, not an unbounded full-size image.
    WiFiClientSecure imageClient;
    imageClient.setInsecure(); // development only; no executables, settings or OTA
    imageClient.setTimeout(8000);
    HTTPClient img;
    img.setTimeout(8000);img.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    if(!img.begin(imageClient,thumbnailUrl)) {status_="Photo image connection failed";return;}
    img.setUserAgent("CYD-SkyTracker/0.4 (github.com/shrek1802/CYD-SkyTracker)");
    int code=img.GET();
    int length=img.getSize();
    Serial.printf("[PHOTO] JPEG download HTTP=%d bytes=%d heap=%u\n",
                  code,length,unsigned(ESP.getFreeHeap()));
    if(code!=HTTP_CODE_OK||length<16||length>int(kMaxJpegBytes)) {
        status_=length<0?"Photo download length unknown":"Photo download failed";
        img.end();return;
    }
    if(ESP.getFreeHeap()<uint32_t(length+40000)) {
        status_="Not enough photo RAM";img.end();return;
    }
    uint8_t* buffer=(uint8_t*)malloc(size_t(length));
    if(!buffer) {status_="Photo allocation failed";img.end();return;}
    const size_t got=img.getStream().readBytes((char*)buffer,size_t(length));
    img.end();
    if(got!=size_t(length)||buffer[0]!=0xff||buffer[1]!=0xd8) {
        Serial.printf("[PHOTO] Invalid JPEG/short read: %u/%d\n",unsigned(got),length);
        free(buffer);status_="Photo JPEG read failed";return;
    }
    jpeg_=buffer;size_=got;
    representative_=!matchedExact;
    status_=representative_?"Representative type photo":"Exact aircraft photo";
    Serial.printf("[PHOTO] PHOTO READY %u bytes (%s)\n",unsigned(got),status_.c_str());
    Serial.printf("[PHOTO] Source: %s | %s | %s\n",
                  artist_.c_str(),licence_.c_str(),source_.c_str());
}
void PhotoView::draw(TFT_eSPI& tft) const {
    tft.fillRoundRect(kX-2,kY-2,kW+4,kH+4,5,0x10A4);
    tft.drawRoundRect(kX-2,kY-2,kW+4,kH+4,5,TFT_DARKGREY);
    if(!jpeg_||!size_) {
        tft.setTextColor(TFT_WHITE,0x10A4);
        tft.setTextSize(1);
        tft.drawCentreString(status_.substring(0,25),kX+kW/2,kY+kH/2-5,1);
        return;
    }
    currentDisplay=&tft;
    TJpgDec.setJpgScale(2);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(drawJpegBlock);
    const int result=TJpgDec.drawJpg(kX+1,kY+1,jpeg_,uint32_t(size_));
    currentDisplay=nullptr;
    Serial.printf("[PHOTO] JPEG decode result=%d\n",result);
    if(representative_) {
        tft.fillRect(kX+1,kY+kH-13,kW-2,13,0x08A3);
        tft.setTextColor(TFT_YELLOW,0x08A3);
        tft.setCursor(kX+5,kY+kH-10);
        tft.print("Representative type photo");
    }
}
void PhotoView::drawCredit(TFT_eSPI& tft) const {
    tft.setTextSize(1);tft.setTextColor(TFT_CYAN,TFT_BLACK);
    tft.setCursor(7,184);
    if(!jpeg_) {
        tft.print("Photo: "+status_);
        return;
    }
    tft.print(representative_?"Example: ":"Aircraft: ");
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.print(model_.substring(0,34));
    tft.setCursor(7,198);
    tft.setTextColor(TFT_CYAN,TFT_BLACK);tft.print("Photo: ");
    tft.setTextColor(TFT_WHITE,TFT_BLACK);tft.print(artist_.substring(0,38));
    tft.setCursor(7,211);
    tft.setTextColor(TFT_CYAN,TFT_BLACK);
    tft.print(licence_.substring(0,17)+"  Wikimedia Commons");
}
