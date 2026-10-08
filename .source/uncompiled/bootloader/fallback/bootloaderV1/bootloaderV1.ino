// Bootloader V0.00 for the Hosyond 4-inch ESP32-32E.
// This sketch is kept in a same-name folder so Arduino IDE can open it directly.
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include "config.h"
#include "bootloader.h"

String ssid;
String password;

bool loadCredentials() {
  File f = SD.open(WIFI_FILE, FILE_READ);
  if (!f) return false;
  ssid = f.readStringUntil('\n');
  password = f.readStringUntil('\n');
  ssid.trim();
  password.trim();
  f.close();
  return ssid.length() > 0;
}

void setup() {
  Serial.begin(115200);
  SPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
  if (!SD.begin(SD_PIN_CS) || !loadCredentials()) {
    Serial.println("Bootloader: no SD credentials");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 15000) delay(100);
  if (BL::run()) ESP.restart();
  Serial.println("Bootloader: no update installed");
}

void loop() {
  delay(1000);
}
