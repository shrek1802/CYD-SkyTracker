// Standalone minimal display test. No SD, Wi-Fi, touch or aircraft code.
// Designed to isolate a white/striped CYD display from SkyTracker startup.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft;
uint32_t lastChange = 0;
int phase = -1;
const uint16_t fills[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_BLACK, TFT_WHITE};
const char *names[] = {"RED", "GREEN", "BLUE", "BLACK", "WHITE"};
void show() {
    phase = (phase + 1) % 5;
    Serial.printf("DIAG: phase=%d colour=%s screen=%dx%d free_heap=%u\n",
        phase, names[phase], tft.width(), tft.height(), ESP.getFreeHeap());
    tft.fillScreen(fills[phase]);
    if (phase == 3 || phase == 4) {
        tft.drawRect(5, 5, tft.width()-10, tft.height()-10, TFT_CYAN);
        tft.drawLine(0,0,tft.width()-1,tft.height()-1,TFT_YELLOW);
        tft.drawLine(tft.width()-1,0,0,tft.height()-1,TFT_YELLOW);
        tft.setTextColor(phase==3 ? TFT_WHITE : TFT_BLACK, fills[phase]);
        tft.setTextSize(2);
        tft.setCursor(15,25);
        tft.print("CYD DISPLAY TEST");
        tft.setTextSize(1);
        tft.setCursor(15,60);
        tft.print("Serial 115200");
        tft.setCursor(15,74);
        tft.print("No Wi-Fi / SD / Touch");
        tft.setCursor(15,88);
#if defined(ST7789_DRIVER)
        tft.print("Driver: ST7789");
#elif defined(ILI9341_2_DRIVER)
        tft.print("Driver: ILI9341");
#else
        tft.print("Driver: unknown");
#endif
        tft.drawCircle(tft.width()/2,tft.height()/2+30,42,TFT_MAGENTA);
    }
    lastChange = millis();
}
void setup() {
    Serial.begin(115200);
    delay(650);
    Serial.println("SKYTRACKER_DISPLAY_DIAG_BOOT v0.1");
    Serial.printf("Chip=%s flash=%u heap=%u\n", ESP.getChipModel(), ESP.getFlashChipSize(), ESP.getFreeHeap());
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);
    Serial.println("Backlight: GPIO21=HIGH, starting TFT init...");
    tft.init();
    tft.setRotation(1);
    Serial.println("TFT.init complete; first screen incoming.");
    show();
}
void loop() {
    if(millis()-lastChange >= 2700) show();
    delay(10);
}
