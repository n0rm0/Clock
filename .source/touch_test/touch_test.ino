// Clock touch test for the Hosyond 4-inch ESP32-32 display.
//
// Open Serial Monitor at 115200 baud, then touch and release each numbered target.
// Send the printed RAW values back so the clock touch mapping can be fixed.
// This sketch is standalone and is not part of the normal Clock firmware build.
//
// Before compiling in Arduino IDE, configure TFT_eSPI for the display:
//   TFT_CS=15, TFT_DC=2, TFT_RST=-1/EN, TFT_SCLK=14,
//   TFT_MOSI=13, TFT_MISO=12, TOUCH_CS=33, TOUCH_IRQ=36,
// and use HSPI. The installer supplies these same values automatically.
// TFT_eSPI's documented default pressure threshold is Z=350.

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

struct Target {
  int16_t x;
  int16_t y;
  const char* name;
};

const Target targets[] = {
  {4, 4, "TOP-LEFT"},
  {475, 4, "TOP-RIGHT"},
  {475, 315, "BOTTOM-RIGHT"},
  {4, 315, "BOTTOM-LEFT"},
  {240, 160, "CENTER"}
};
const size_t targetCount = sizeof(targets) / sizeof(targets[0]);
size_t targetIndex = 0;
uint32_t lastTouch = 0;
bool wasPressed = false;

void drawTarget() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Touch " + String(targetIndex + 1) + "/" + String(targetCount), 240, 18, 2);
  tft.drawString(targets[targetIndex].name, 240, 300, 2);
  tft.drawLine(targets[targetIndex].x - 18, targets[targetIndex].y,
               targets[targetIndex].x + 18, targets[targetIndex].y, TFT_RED);
  tft.drawLine(targets[targetIndex].x, targets[targetIndex].y - 18,
               targets[targetIndex].x, targets[targetIndex].y + 18, TFT_RED);
  tft.drawCircle(targets[targetIndex].x, targets[targetIndex].y, 12, TFT_YELLOW);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("CLOCK TOUCH TEST");
  Serial.println("Rotation: 3 (180 degrees)");
  Serial.println("Touch and release each target; send all RAW_X/RAW_Y/RAW_Z lines back.");
  Serial.println("Screen limits at rotation 3 are X=0..479 and Y=0..319.");

  tft.init();
  tft.setRotation(3);
  drawTarget();
}

void loop() {
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  uint16_t rawZ = tft.getTouchRawZ();
  bool pressed = rawZ > 350;
  if (!pressed) {
    wasPressed = false;
    delay(10);
    return;
  }
  if (!wasPressed && millis() - lastTouch > 500) {
    tft.getTouchRaw(&rawX, &rawY);
    lastTouch = millis();
    wasPressed = true;
    Serial.printf("TARGET %u %s: RAW_X=%u RAW_Y=%u RAW_Z=%u SCREEN_X=%d SCREEN_Y=%d\n",
                  (unsigned)(targetIndex + 1), targets[targetIndex].name,
                  rawX, rawY, rawZ, targets[targetIndex].x, targets[targetIndex].y);
    tft.fillCircle(targets[targetIndex].x, targets[targetIndex].y, 8, TFT_GREEN);
    targetIndex = (targetIndex + 1) % targetCount;
    delay(250);
    drawTarget();
  }
}
