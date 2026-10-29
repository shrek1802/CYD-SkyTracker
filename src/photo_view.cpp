#include "photo_view.h"
#include "photo_match.h"
#include "aircraft_type_photo.h"
#include "ui_style.h"
#include "rgb565_display.h"
#include "aircraft_icon.h"
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
// A 144x96 little-endian RGB565 model image is a fixed 27,648-byte stream.
constexpr int kRawW = 304, kRawH = 128;
constexpr size_t kRawBytes = size_t(kRawW) * size_t(kRawH) * 2;
constexpr int kX=8, kY=32, kW=304, kH=128;
TFT_eSPI* currentDisplay = nullptr;
// The JPEG callback must follow the target panel: the previous fixed detail
// viewport (8,32,304,128) silently hid photos in the other two UI views.
int jpegX=kX, jpegY=kY, jpegW=kW, jpegH=kH;

bool drawJpegBlock(int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t* bitmap) {
    if (!currentDisplay || x < jpegX || y < jpegY ||
        x >= jpegX+jpegW || y >= jpegY+jpegH) return false;
    const uint16_t visibleRows = uint16_t(std::min<int>(int(h),jpegY+jpegH-y));
    const uint16_t visibleCols = uint16_t(std::min<int>(int(w),jpegX+jpegW-x));
    if (visibleCols != w) {
        // JPEG MCU blocks are normally already clipped by TJpg_Decoder.
        // Avoid treating a cropped row as tightly packed pixel data.
        for (uint16_t line=0;line<visibleRows;++line)
            currentDisplay->pushImage(x,y+line,visibleCols,1,bitmap+size_t(line)*w);
    } else currentDisplay->pushImage(x,y,w,visibleRows,bitmap);
    return true;
}

// Decode a licensed JPEG into either the small card or enlarged photograph.
// Bounded thumbnail source, no large framebuffer, and do not stretch an image
// beyond its real resolution. This was missing for downloaded JPEGs.
bool drawJpegFitted(TFT_eSPI& tft,const uint8_t* data,size_t bytes,
                    int x,int y,int width,int height) {
    if(!data || bytes<16) return false;
    uint16_t sourceW=0,sourceH=0;
    const int probe=TJpgDec.getJpgSize(&sourceW,&sourceH,data,uint32_t(bytes));
    if(probe!=JDR_OK || !sourceW || !sourceH) {
        Serial.printf("[PHOTO] JPEG header failed: %d\\n",probe); return false;
    }
    uint8_t scale=1;
    while(scale<8 && (int(sourceW)>width*scale || int(sourceH)>height*scale))
        scale*=2;
    if(int(sourceW)>width*scale || int(sourceH)>height*scale) {
        Serial.printf("[PHOTO] JPEG cannot fit %dx%d: %ux%u\\n",
                      width,height,unsigned(sourceW),unsigned(sourceH));
        return false;
    }
    const int imageW=(int(sourceW)+scale-1)/scale;
    const int imageH=(int(sourceH)+scale-1)/scale;
    const int drawX=x+(width-imageW)/2, drawY=y+(height-imageH)/2;
    jpegX=x; jpegY=y; jpegW=width; jpegH=height;
    currentDisplay=&tft;
    TJpgDec.setJpgScale(scale);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(drawJpegBlock);
    const int result=TJpgDec.drawJpg(drawX,drawY,data,uint32_t(bytes));
    currentDisplay=nullptr;
    Serial.printf("[PHOTO] JPEG drawn in %dx%d card: %ux%u scale=%u, result=%d\\n",
                  width,height,unsigned(sourceW),unsigned(sourceH),unsigned(scale),result);
    return result==JDR_OK;
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
    return (url.startsWith("https://upload.wikimedia.org/") ||
            url.startsWith("https://thumb.wikimedia.org/")) && url.length()<1024;
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
        "engine detail","museum display","scale model","wing view",
        "gear bay","passagierbereich","hangar","contrail","sunspot",
        "winglet","tail of","sunset","moon","hold előtt"};
    for(const char* word:bad) if(title.indexOf(word)>=0)return false;
    return true;
}
}

// Free the currently displayed photo when exiting the selected aircraft.
void PhotoView::clear() {
    if(jpeg_)free(jpeg_);
    jpeg_=nullptr;size_=0;artist_="";licence_="";source_="";
    model_="";representative_=false;rgbReady_=false;rgbPath_="";
    status_="No photo loaded";
}
void PhotoView::load(const char* registration,const char* type,
                     bool sdReady,bool sdOnly) {
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

    // Direct RGB565: pre-rendered on a computer, identical display format
    // to /maps/*.rgb565. Exact size and credit required; no decoder involved.
    if(model&&sdReady) {
        const String imagePath=String("/aircraft/")+model->code+".rgb565";
        const String creditPath=String("/aircraft/")+model->code+".txt";
        File attribution=SD.open(creditPath,FILE_READ);
        if(attribution) {
            String artist=attribution.readStringUntil('\n');
            String license=attribution.readStringUntil('\n');
            String source=attribution.readStringUntil('\n');
            attribution.close();
            artist.trim();license.trim();source.trim();
            File pixelFile=SD.open(imagePath,FILE_READ);
            const size_t fileBytes=pixelFile?pixelFile.size():0;
            Serial.printf("[PHOTO] SD RGB565 %s: %s, %u bytes, licensed=%d\n",
                imagePath.c_str(),pixelFile?"present":"missing",
                unsigned(fileBytes),acceptedLicence(license)?1:0);
            if(pixelFile&&fileBytes==kRawBytes&&acceptedLicence(license)&&
               source.startsWith("https://commons.wikimedia.org/")) {
                rgbReady_=true;
                rgbPath_=imagePath;
                artist_=artist;licence_=license;source_=source;
                status_="Representative type photo";
                representative_=true;
                pixelFile.close();
                Serial.printf("[PHOTO_TEST] SD RGB565 READY %s\n",imagePath.c_str());
                return;
            }
            if(pixelFile)pixelFile.close();
        } else {
            Serial.printf("[PHOTO] Missing SD attribution: %s\n",creditPath.c_str());
        }
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
    if(sdOnly) {
        status_ = sdReady ? "SD RGB565 photo missing" : "SD card not mounted";
        Serial.printf("[PHOTO_TEST] %s, model=%s\n",
                      status_.c_str(), model?model->code:"unknown");
        return;
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
        // Search named model photos first: category lists sometimes begin
        // with cabin interiors, wings or aircraft lost in the distance.
        const String searchQuery="&generator=search&gsrnamespace=6&gsrlimit=20&gsrsearch="+
            encode(String("intitle:\"")+model->model+"\"");
        result=lookup(searchQuery,false,false);
        if(result==0) {
            const String catQuery="&generator=categorymembers&gcmtitle="+
                encode(String("Category:")+model->commonsCategory)+
                "&gcmnamespace=6&gcmlimit=20";
            result=lookup(catQuery,false,true);
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

    if(rgbReady_) {
        File raw=SD.open(rgbPath_,FILE_READ);
        if (!raw || raw.size()!=kRawBytes) {
            Serial.printf("[PHOTO] RGB565 file unavailable: %s\n",rgbPath_.c_str());
            tft.setTextColor(TFT_YELLOW,0x10A4);
            tft.drawCentreString("SD image missing",kX+kW/2,kY+kH/2-5,1);
            if(raw)raw.close();
            return;
        }
        // Identical byte order and TFT_eSPI row path as our offline maps.
        // No JPEG decoder, heap allocation, or Wi-Fi image request at draw time.
        static uint16_t pixels[kRawW];
        bool ok=true;
        for(int row=0;row<kRawH;++row) {
            const size_t bytes=raw.read(reinterpret_cast<uint8_t*>(pixels),kRawW*2);
            if(bytes!=size_t(kRawW*2)) {ok=false;break;}
            SkyRgb565::pushRow(tft,kX,kY+row,kRawW,pixels);
        }
        raw.close();
        Serial.printf("[PHOTO] RGB565 drawing: %s, %d rows\n",
                      ok?"PASS":"SHORT READ",kRawH);
        if(representative_) {
            tft.fillRect(kX+1,kY+kH-13,kW-2,13,0x08A3);
            tft.setTextColor(TFT_YELLOW,0x08A3);
            tft.setCursor(kX+5,kY+kH-10);
            tft.print("Representative type photo");
        }
        return;
    }
    if(!jpeg_||!size_) {
        tft.setTextColor(TFT_WHITE,0x10A4);
        tft.setTextSize(1);
        tft.drawCentreString(status_.substring(0,25),kX+kW/2,kY+kH/2-5,1);
        return;
    }
    // Wikimedia may serve 330px thumbnails even when 280px was requested.
    // Scale to the available photo window instead of overpainting the plane
    // registration/altitude panel on the right of this 320x240 display.
    uint16_t sourceW=0,sourceH=0;
    const int probe=TJpgDec.getJpgSize(&sourceW,&sourceH,jpeg_,uint32_t(size_));
    if(probe!=JDR_OK||!sourceW||!sourceH) {
        Serial.printf("[PHOTO] JPEG header/decode error=%d\n",probe);
        tft.setTextColor(TFT_YELLOW,0x10A4);
        tft.drawCentreString("JPEG decode failed",kX+kW/2,kY+kH/2-5,1);
        return;
    }
    uint8_t scale=1;
    while(scale<8 &&
          (int(sourceW)>int(kW-2)*scale||int(sourceH)>int(kH-2)*scale))
        scale*=2;
    if(int(sourceW)>int(kW-2)*scale||int(sourceH)>int(kH-2)*scale) {
        Serial.println("[PHOTO] JPEG is too large for the detail card");
        return;
    }
    Serial.printf("[PHOTO] JPEG source %ux%u, scale %u\n",
                  unsigned(sourceW),unsigned(sourceH),unsigned(scale));
    currentDisplay=&tft;
    TJpgDec.setJpgScale(scale);
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
    // Credit stays visible beneath the full-width photo and live telemetry.
    // Full Commons file URL remains available through [PHOTO] USB diagnostics.
    tft.fillRect(8,216,tft.width()-16,13,TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(TFT_CYAN,TFT_BLACK);
    tft.setCursor(8,218);
    if(!rgbReady_ && !jpeg_) {
        tft.print(status_.substring(0,49));
        return;
    }
    tft.print("Photo: ");
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    String credit = artist_ + "  |  " + licence_;
    tft.print(credit.substring(0,43));
}

namespace {
// For ICAO types without a licensed SD photograph, draw an original
// top-down silhouette; never mislabel another model as the selected airframe.
// Reuse the exact same 20-point aircraft outline as the radar markers.
void drawTypeSilhouette(TFT_eSPI& tft,int x,int y,int w,int h) {
    AircraftIcon::Point outline[AircraftIcon::kOutlineSize];
    const int cx=x+w/2, cy=y+h/2;
    const float scale=std::min(float(w)/34.0f,float(h)/36.0f);
    AircraftIcon::polygon(cx,cy,0.0f,outline,scale);
    for(int i=0;i<AircraftIcon::kOutlineSize;++i) {
        const auto& a=outline[i];
        const auto& b=outline[(i+1)%AircraftIcon::kOutlineSize];
        tft.drawLine(a.x,a.y,b.x,b.y,SkyUI::kAccent);
    }
    tft.fillCircle(cx,cy,2,SkyUI::kGold);
}
// Row-stream an SD RGB565 image at its native size with a hard byte-count
// and bounded 640-byte RAM row. No full-screen allocation or JPEG decoder.
bool renderNative(TFT_eSPI& tft,const String& path,int x,int y,int w,int h) {
    File file=SD.open(path,FILE_READ);
    if(!file || file.size()!=size_t(w)*size_t(h)*2) {
        if(file)file.close();
        Serial.printf("[PHOTO] Native asset unavailable %s (wanted %d bytes)\n",
                      path.c_str(),w*h*2);
        return false;
    }
    static uint16_t row[320];
    if(w>320) {file.close();return false;}
    bool ok=true;
    for(int line=0;line<h;++line) {
        if(file.read(reinterpret_cast<uint8_t*>(row),size_t(w)*2)!=size_t(w)*2) {
            ok=false;break;
        }
        SkyRgb565::pushRow(tft,x,y+line,w,row);
    }
    file.close();
    Serial.printf("[PHOTO] Fullscreen RGB565 %s (%dx%d) %s\n",
                  path.c_str(),w,h,ok?"PASS":"SHORT READ");
    return ok;
}
}
void PhotoView::drawThumbnail(TFT_eSPI& tft) const {
    constexpr int x=10, y=39, w=152, h=64;
    tft.fillRoundRect(x-2,y-2,w+4,h+4,5,SkyUI::kPanelAlt);
    if(!rgbReady_ && jpeg_ && size_) {
        if(!drawJpegFitted(tft,jpeg_,size_,x,y,w,h-15))
            drawTypeSilhouette(tft,x,y,w,h-14);
    } else if(!rgbReady_) {
        drawTypeSilhouette(tft,x,y,w,h-14);
    } else {
        File raw=SD.open(rgbPath_,FILE_READ);
        if(!raw||raw.size()!=kRawBytes) {
            if(raw)raw.close();
            tft.setTextColor(SkyUI::kGold,SkyUI::kPanelAlt);
            tft.drawCentreString("Photo unavailable",x+w/2,y+24,1);
        } else {
            // 304x128 source -> 152x64 display (every second pixel/row).
            // Never scale via a second full-size framebuffer.
            static uint16_t sourceRow[kRawW];
            static uint16_t outputRow[w];
            bool ok=true;
            for(int line=0;line<h;++line) {
                if(!raw.seek(uint32_t(line*2*kRawW*2)) ||
                   raw.read(reinterpret_cast<uint8_t*>(sourceRow),kRawW*2)!=size_t(kRawW*2)) {
                    ok=false;break;
                }
                for(int col=0;col<w;++col)outputRow[col]=sourceRow[col*2];
                SkyRgb565::pushRow(tft,x,y+line,w,outputRow);
            }
            raw.close();
            if(!ok)Serial.println("[PHOTO] Thumbnail SD short read");
        }
    }
    tft.fillRect(x,y+h-15,w,15,SkyUI::kPanel);
    tft.setTextColor(SkyUI::kGold,SkyUI::kPanel);
    tft.setCursor(x+10,y+h-12);
    tft.print(hasPhoto()?"TAP TO ENLARGE":"TYPE SILHOUETTE");
}

void PhotoView::drawFullscreen(TFT_eSPI& tft) const {
    // 320x166 image fills nearly the whole 2.8-inch CYD; the remaining
    // area is reserved for credits and two reliable touch buttons.
    const int wide=320, tall=166, x=(tft.width()-wide)/2, y=28;
    tft.fillRect(0,y,tft.width(),tall,SkyUI::kBackground);
    if(!rgbReady_) {
        if(jpeg_ && size_ && drawJpegFitted(tft,jpeg_,size_,x,y,wide,tall)) {
            // A registration-specific search may be exact, while an ICAO type
            // search is explicitly a representative model photograph.
            tft.fillRect(0,y+tall-14,tft.width(),14,SkyUI::kPanel);
            tft.setTextColor(SkyUI::kGold,SkyUI::kPanel);
            tft.setCursor(8,y+tall-11);
            tft.print(representative_?"Representative aircraft type photo":"Aircraft registration photo");
            return;
        }
        drawTypeSilhouette(tft,0,y,tft.width(),tall-30);
        tft.setTextColor(SkyUI::kGold,SkyUI::kBackground);
        tft.drawCentreString(status_.substring(0,45),tft.width()/2,y+tall-24,1);
        return;
    }
    String full=rgbPath_;
    full.replace(".rgb565",".full.rgb565");
    if(SD.exists(full) && renderNative(tft,full,x,y,wide,tall))return;
    // Backwards compatible with the previous SD pack: show the old
    // 304x128 asset centred, no bogus upscaling.
    if(!renderNative(tft,rgbPath_,(tft.width()-kRawW)/2,y+19,kRawW,kRawH)) {
        tft.setTextColor(SkyUI::kGold,SkyUI::kBackground);
        tft.drawCentreString("SD photo read failed",tft.width()/2,103,2);
    }
}
void PhotoView::drawCreditAt(TFT_eSPI& tft,int y) const {
    tft.fillRect(0,y,tft.width(),12,SkyUI::kBackground);
    tft.setTextColor(SkyUI::kMuted,SkyUI::kBackground);
    tft.setTextSize(1);
    tft.setCursor(8,y+1);
    if(!hasPhoto()) {
        tft.print(status_.substring(0,44));
        return;
    }
    const String credit=String("Photo: ")+artist_+" | "+licence_;
    tft.print(credit.substring(0,49));
}
