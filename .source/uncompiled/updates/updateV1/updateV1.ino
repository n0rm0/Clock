// ====================================================================
//  clock.ino - Hosyond 4" ESP32-32E (480x320 ST7796S, XPT2046 touch)
//  Libraries: TFT_eSPI (display + its built-in touch), PNGdec, ArduinoJson
//  Build flags for TFT_eSPI come from the installer (see config.h).
//  Flow: Welcome -> pick WiFi -> password keyboard -> connect -> Home
//  Saved on the SD card: /.source/data/wifi.txt, touch.txt (calibration)
//  Icons: wifi loader + battery redrawn from Uiverse.io designs
//         (Erasmus001 wifi-loader, Yaya12085 battery), keys = patrick_2593,
//         weather = Meteocons PNGs from /.source/icons
//  Home includes live weather, calendar navigation, Wi-Fi/offline controls,
//  settings, manual clock entry, and an on-device alarm editor.
// ====================================================================
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>
#include <TFT_eSPI.h>
#include <PNGdec.h>
#include "config.h"
#include "bootloader.h"   // GitHub updater

// Arduino's automatic prototype generation sees keyLabel() before the full
// declaration below. Keep the type visible to that generated prototype.
struct Key;
struct AlarmState;
void alarmButton(int x, int y, int w, const String& label, bool selected = false);

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

// ---------------- colors ----------------
#define C(r,g,b) tft.color565(r,g,b)
uint16_t UI_BLUE, UI_BACK, UI_TEXT, UI_KEY, UI_LIP, UI_RED, UI_YEL, UI_GREEN;

enum Screen { S_WELCOME, S_SCAN, S_KEYS, S_HOME, S_ALARM, S_CLOCK, S_SETTINGS, S_WIFI, S_WEATHER, S_CALENDAR, S_UPDATE, S_SDCARD, S_SDFILES, S_PROFILE };
Screen screen = S_WELCOME;

// ClockOS release identity. The sketch folder remains updateV1 for installer
// compatibility, while the firmware/update feed is now ClockOS 3.1.
static const char* CLOCKOS_NAME = "ClockOS";
static const char* CLOCKOS_VERSION = "3.1";
static const char* SYNC_PROTOCOL_VERSION = "3.1";
String syncPeerVersion = "";

bool syncVersionCompatible(const String& peer) {
  int dot = peer.indexOf('.');
  int localDot = String(SYNC_PROTOCOL_VERSION).indexOf('.');
  if (dot < 1 || localDot < 1) return false;
  int peerMajor = peer.substring(0, dot).toInt();
  int peerMinor = peer.substring(dot + 1).toInt();
  int localMajor = String(SYNC_PROTOCOL_VERSION).substring(0, localDot).toInt();
  int localMinor = String(SYNC_PROTOCOL_VERSION).substring(localDot + 1).toInt();
  return peerMajor == localMajor && abs(peerMinor - localMinor) <= 1;
}

String syncCompatibilityText() {
  if (!syncPeerVersion.length()) return "Waiting for another ClockOS display";
  if (syncVersionCompatible(syncPeerVersion)) return "Compatible display: ClockOS " + syncPeerVersion;
  return "Warning: ClockOS " + syncPeerVersion + " is not compatible";
}

void drawProfileIcon(int cx, int cy, uint16_t color) {
  tft.fillCircle(cx, cy - 7, 7, color);
  tft.fillRoundRect(cx - 14, cy + 2, 28, 16, 12, color);
}

String selSsid, password, errMsg;
String profileName = "Not signed in";
String ssids[5]; int rssis[5]; int nNets = 0;
bool showPass = false; uint8_t layer = 0;
bool sdOk = false;       // valid ClockOS-formatted SD card
bool sdPresent = false;  // SD hardware/card responds, but may be unformatted
String sdCardMessage = "";
bool screenSleeping = false;
bool factoryResetArmed = false;
uint32_t lastActivity = 0;
bool swipeBackDetected = false;

// ====================================================================
//  text helper (GFX free fonts)
// ====================================================================
void txt(const GFXfont* f, uint16_t col, uint8_t datum, const String& s, int x, int y) {
  tft.setFreeFont(f);
  tft.setTextColor(col);
  tft.setTextDatum(datum);
  tft.drawString(s, x, y);
}
#define F12B (&FreeSansBold12pt7b)
#define F18B (&FreeSansBold18pt7b)
#define F24B (&FreeSansBold24pt7b)
#define F12  (&FreeSans12pt7b)

// ====================================================================
//  touch (TFT_eSPI built-in XPT2046 support + 4-corner calibration)
// ====================================================================
uint16_t calData[5] = {
  TOUCH_CAL_X0, TOUCH_CAL_X1, TOUCH_CAL_Y0, TOUCH_CAL_Y1, TOUCH_CAL_ROTATION
};

bool loadCal() {
  if (!sdOk) return false;
  File f = SD.open(TOUCH_FILE);
  if (!f) return false;
  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return false; }
  for (int i = 0; i < 5; i++) calData[i] = doc["cal"][i] | calData[i];
  f.close();
  return calData[0] || calData[1] || calData[2] || calData[3];
}

void saveCal() {
  if (!sdOk) return;
  SD.mkdir(DATA_DIR); SD.mkdir(PREFERENCES_DIR); SD.remove(TOUCH_FILE);
  File f = SD.open(TOUCH_FILE, FILE_WRITE);
  if (!f) return;
  DynamicJsonDocument doc(512);
  JsonArray values = doc.createNestedArray("cal");
  for (int i = 0; i < 5; i++) values.add(calData[i]);
  serializeJson(doc, f);
  f.close();
}

void ensureCalibration() {
  // Use the official TFT_eSPI calibration measured for rotation 3. Do not
  // launch the calibration screen or replace these values from the SD card.
  tft.setTouch(calData);
}

bool readTouch(int &x, int &y) {
  static uint32_t last = 0;
  static int startX = -1, startY = -1;
  static uint32_t startAt = 0;
  uint16_t tx, ty;
  // TFT_eSPI documents Z=350 as the default pressure threshold. Passing it
  // explicitly prevents the disconnected/idle XPT2046 readings from acting
  // like touches (the test sketch previously exposed this as RAW_X=0).
  if (tft.getTouch(&tx, &ty, 350) && millis() - last > 220) {
    uint32_t now = millis();
    if (startX < 0 || now - startAt > 900) { startX = tx; startY = ty; startAt = now; }
    swipeBackDetected = (tx > startX + 90 && abs((int)ty - startY) < 70 && now - startAt < 900);
    last = now; lastActivity = now; x = tx; y = ty;
    if (swipeBackDetected) { startX = -1; startY = -1; }
    return true;
  }
  if (!tft.getTouch(&tx, &ty, 350)) { startX = -1; startY = -1; }
  return false;
}

// ====================================================================
//  PNG from SD (weather icons)
// ====================================================================
static PNG png;
static File pngFile;
static int pngX, pngY;
static uint16_t lineBuf[480];

struct WeatherState {
  bool valid;
  float temperatureF;
  int code;
  String city;
  String icon;
  String label;
};
WeatherState weather = {false, 0.0f, -1, "", "not-available", "Weather offline"};
uint32_t lastWeatherFetch = 0;

struct AlarmState {
  bool enabled;
  uint8_t hour;
  uint8_t minute;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t repeatMask; // bit 0=Sunday ... bit 6=Saturday; 0 means one-time
};
AlarmState clockAlarm = {false, 7, 0, 2026, 1, 1, 0};
AlarmState alarmDraft = {false, 7, 0, 2026, 1, 1, 0};
AlarmState clockDraft = {true, 12, 0, 2026, 1, 1, 0};
bool alarmRinging = false;

struct AppSettings {
  bool offline;
  bool wifiEnabled;
  bool use24Hour;
  bool showBatteryPercent;
  bool notifications;
  bool calendarEnabled;
  bool manualWeather;
  bool classroomEnabled;
  bool setupComplete;
  bool syncEnabled;
  bool syncMain;
};
AppSettings settings = {false, true, false, true, true, true, false, false, false, false, true};
struct SavedNetwork { String ssid; String password; };
SavedNetwork savedNetworks[5];
uint8_t savedNetworkCount = 0;
String manualCity = "Philadelphia";
double manualLat = WEATHER_LAT;
double manualLon = WEATHER_LON;
bool editingCity = false;
bool firstSetupClassroom = false;
bool firstSetupSync = false;
String nextAssignment = "No Classroom assignments";
String nextAssignmentCourse = "";
String nextAssignmentDue = "";
int classroomAssignmentCount = 0;
String assignmentTitles[8], assignmentCourses[8], assignmentDues[8];
time_t assignmentEpochs[8] = {};

static void* pngOpen(const char* fn, int32_t* size) { pngFile = SD.open(fn); *size = pngFile ? pngFile.size() : 0; return &pngFile; }
static void pngClose(void*) { if (pngFile) pngFile.close(); }
static int32_t pngRead(PNGFILE*, uint8_t* buf, int32_t len) { return pngFile ? pngFile.read(buf, len) : 0; }
static int32_t pngSeek(PNGFILE*, int32_t pos) { return pngFile ? pngFile.seek(pos) : 0; }
static int pngDraw(PNGDRAW* d) {
  png.getLineAsRGB565(d, lineBuf, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  tft.pushImage(pngX, pngY + d->y, d->iWidth, 1, lineBuf);
  return 0;
}

bool drawPng(const char* path, int x, int y) {
  if (!sdOk || !SD.exists(path)) return false;
  pngX = x; pngY = y;
  if (png.open(path, pngOpen, pngClose, pngRead, pngSeek, pngDraw) != PNG_SUCCESS) return false;
  tft.startWrite(); png.decode(NULL, 0); tft.endWrite();
  png.close();
  return true;
}

String weatherIconForCode(int code) {
  if (code == 0) return "clear-day";
  if (code <= 3) return "partly-cloudy-day";
  if (code <= 48) return "fog";
  if (code <= 57) return "drizzle";
  if (code <= 67 || (code >= 80 && code <= 82)) return "rain";
  if (code <= 77) return "snow";
  if (code >= 95) return "thunderstorms";
  return "cloudy";
}

String weatherLabelForCode(int code) {
  if (code == 0) return "Clear";
  if (code <= 3) return "Partly cloudy";
  if (code <= 48) return "Fog";
  if (code <= 57) return "Drizzle";
  if (code <= 67 || (code >= 80 && code <= 82)) return "Rain";
  if (code <= 77) return "Snow";
  if (code >= 95) return "Storm";
  return "Cloudy";
}

String urlEncodeCity(String value) {
  value.replace(" ", "%20"); value.replace(",", "%2C");
  return value;
}

bool geocodeManualCity() {
  if (manualCity.length() == 0 || WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client; client.setInsecure(); HTTPClient http;
  String url = "https://geocoding-api.open-meteo.com/v1/search?name=" + urlEncodeCity(manualCity) + "&count=1&language=en&format=json";
  if (!http.begin(client, url) || http.GET() != HTTP_CODE_OK) { http.end(); return false; }
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, http.getString()); http.end();
  if (error || !doc["results"][0]) return false;
  manualLat = doc["results"][0]["latitude"] | manualLat;
  manualLon = doc["results"][0]["longitude"] | manualLon;
  manualCity = (const char*)(doc["results"][0]["name"] | manualCity.c_str());
  saveSettings();
  return true;
}

bool fetchWeather() {
  if (settings.offline || WiFi.status() != WL_CONNECTED) return false;
  lastWeatherFetch = millis();
  WiFiClientSecure client;
  client.setInsecure(); // ESP32 has no bundled CA store; data is non-sensitive.
  HTTPClient http;
  double lat = WEATHER_LAT, lon = WEATHER_LON;
  String city = "Local";

  if (settings.manualWeather) {
    lat = manualLat; lon = manualLon; city = manualCity;
  } else if (http.begin(client, "https://ipapi.co/json/")) {
    int status = http.GET();
    if (status == HTTP_CODE_OK) {
      DynamicJsonDocument geo(1536);
      if (deserializeJson(geo, http.getString()) == DeserializationError::Ok) {
        lat = geo["latitude"] | WEATHER_LAT;
        lon = geo["longitude"] | WEATHER_LON;
        city = (const char*)(geo["city"] | "Local");
      }
    }
    http.end();
  }

  char url[300];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f&current=temperature_2m,weather_code&temperature_unit=fahrenheit&timezone=auto",
           lat, lon);
  if (!http.begin(client, url)) return false;
  int status = http.GET();
  if (status != HTTP_CODE_OK) { http.end(); return false; }
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, http.getString());
  http.end();
  if (error) return false;

  weather.temperatureF = doc["current"]["temperature_2m"] | 0.0f;
  weather.code = doc["current"]["weather_code"] | -1;
  weather.city = city;
  weather.icon = weatherIconForCode(weather.code);
  weather.label = weatherLabelForCode(weather.code);
  weather.valid = weather.code >= 0;
  return weather.valid;
}

void drawWeatherFallback(int x, int y, int code) {
  // Always show a visible icon even when SD icons were not installed.
  if (code >= 51 && code <= 82) {
    tft.fillCircle(x + 42, y + 38, 20, COL_WHITE);
    tft.fillCircle(x + 62, y + 34, 25, COL_WHITE);
    tft.fillRoundRect(x + 20, y + 36, 72, 28, 12, COL_WHITE);
    for (int i = 0; i < 3; i++) tft.drawLine(x + 28 + i * 22, y + 74, x + 20 + i * 22, y + 88, COL_BLUE);
  } else if (code >= 95) {
    tft.fillCircle(x + 48, y + 45, 28, UI_YEL);
    tft.drawLine(x + 42, y + 78, x + 30, y + 94, UI_YEL);
    tft.drawLine(x + 62, y + 78, x + 74, y + 94, UI_YEL);
  } else {
    tft.fillCircle(x + 52, y + 48, 28, UI_YEL);
    tft.drawCircle(x + 52, y + 48, 35, UI_YEL);
  }
}

void drawWeatherIcon(int x, int y) {
  char path[96];
  snprintf(path, sizeof(path), "%s/%s.png", ICON_DIR, weather.icon.c_str());
  if (!drawPng(path, x, y)) drawWeatherFallback(x, y, weather.code);
}

// ====================================================================
//  uiverse key styles
// ====================================================================
// raised key with a lip (buttons on the blue screens)
void drawKey(int x, int y, int w, int h, const char* label, bool pressed = false, uint16_t face = 0) {
  if (!face) face = UI_KEY;
  tft.fillRoundRect(x, y + 3, w, h, 7, UI_LIP);
  int off = pressed ? 3 : 0;
  tft.fillRoundRect(x, y + off, w, h, 7, face);
  txt(F12B, UI_TEXT, MC_DATUM, label, x + w / 2, y + off + h / 2);
}

// uiverse keyboard key (patrick_2593): dark key, green border, teal text + glow
void drawKbKey(int x, int y, int w, int h, const char* label, bool pressed = false) {
  uint16_t face   = pressed ? C(0x16, 0x2A, 0x26) : C(0x1F, 0x1E, 0x1E);
  uint16_t border = pressed ? C(0, 255, 191)      : C(0, 59, 32);
  uint16_t glow   = pressed ? C(0, 200, 150)      : C(9, 64, 52);
  uint16_t col    = pressed ? C(140, 255, 235)    : C(0, 255, 191);
  tft.fillRoundRect(x, y + 2, w, h, 6, 0x0000);
  tft.fillRoundRect(x, y, w, h, 6, border);
  tft.fillRoundRect(x + 1, y + 1, w - 2, h - 2, 5, face);
  tft.drawFastHLine(x + 4, y + h - 2, w - 8, glow);
  txt(F12B, col, MC_DATUM, label, x + w / 2, y + h / 2);
}

// ====================================================================
//  uiverse wifi loader (Erasmus001): 3 rings, light "back" + dark "front"
// ====================================================================
float dashFrac(float p) {                       // keyframes 0,25,65,80,100 %
  static const float t[] = {0, .25f, .65f, .8f, 1};
  static const float v[] = {.1f, 0, 1.2f, 1.1f, 1.1f};
  p -= floorf(p);
  for (int i = 0; i < 4; i++)
    if (p <= t[i + 1]) return v[i] + (v[i + 1] - v[i]) * (p - t[i]) / (t[i + 1] - t[i]);
  return v[4];
}

// arc in "clockwise from 3 o'clock" degrees; TFT_eSPI wants clockwise from 6 o'clock
void arcLovy(TFT_eSPI& g, int cx, int cy, int r, int th, float a0, float len, uint16_t col, uint16_t bg) {
  a0 = fmodf(a0, 360.0f); if (a0 < 0) a0 += 360.0f;
  float a1 = a0 + len;
  auto seg = [&](float s, float e) {
    int ts = (int)fmodf(s - 90.0f + 360.0f, 360.0f);
    int te = (int)fmodf(e - 90.0f + 360.0f, 360.0f);
    if (e - s >= 359.0f) { ts = 0; te = 360; }
    if (ts == te) return;
    if (ts < te) g.drawArc(cx, cy, r + th / 2, r - th / 2, ts, te, col, bg, true);
    else {
      g.drawArc(cx, cy, r + th / 2, r - th / 2, ts, 360, col, bg, true);
      if (te > 0) g.drawArc(cx, cy, r + th / 2, r - th / 2, 0, te, col, bg, true);
    }
  };
  if (a1 <= 360.0f) seg(a0, a1); else { seg(a0, 360.0f); seg(0.0f, a1 - 360.0f); }
}

void drawLoaderFrame(TFT_eSPI& g, int cx, int cy, uint16_t bg, float tSec) {
  const int R[3]    = {40, 27, 14};
  const float bd[3] = {0.30f, 0.25f, 0.20f};
  const float fd[3] = {0.15f, 0.10f, 0.05f};
  for (int i = 0; i < 3; i++) {
    float p = (tSec - bd[i]) / 1.8f;
    arcLovy(g, cx, cy, R[i], 6, -100.0f - 360.0f * dashFrac(p), 90.0f, UI_BACK, bg);
  }
  for (int i = 0; i < 3; i++) {
    float p = (tSec - fd[i]) / 1.8f;
    arcLovy(g, cx, cy, R[i], 6, -100.0f - 360.0f * dashFrac(p), 90.0f, 0x0000, bg);
  }
}

void animateLoader(int cx, int cy) {            // call every ~16 ms
  static uint32_t t0 = millis();
  spr.fillSprite(UI_BLUE);
  drawLoaderFrame(spr, 50, 50, UI_BLUE, (millis() - t0) / 1000.0f);
  spr.pushSprite(cx - 50, cy - 50);
}

// ====================================================================
//  wifi signal icon (loader rings as a fan) + battery (Yaya12085)
// ====================================================================
int rssiLevel(int rssi) { return rssi > -60 ? 3 : rssi > -70 ? 2 : rssi > -80 ? 1 : 0; }

void drawWifiSignal(int cx, int by, int level, uint16_t bg, uint16_t lit, uint16_t dim) {
  const int R[3] = {7, 13, 19};
  tft.fillRect(cx - 24, by - 25, 48, 30, bg);
  for (int i = 0; i < 3; i++)
    arcLovy(tft, cx, by, R[i], 4, 225.0f, 90.0f, (level > i) ? lit : dim, bg);
  tft.fillCircle(cx, by - 1, 3, level > 0 ? lit : dim);
}

void drawBattery(int x, int y, int pct, uint16_t bg, uint16_t line) {
  tft.fillRect(x - 2, y - 2, 54, 22, bg);
  tft.fillRoundRect(x, y, 44, 16, 3, line);
  tft.fillRoundRect(x + 2, y + 2, 40, 12, 2, bg);
  tft.fillRoundRect(x + 42, y + 4, 3, 8, 2, line);
  uint16_t col = pct <= 20 ? UI_RED : pct <= 45 ? UI_YEL : UI_GREEN;
  int w = max(1, pct * 34 / 100);
  tft.fillRect(x + 5, y + 4, w, 8, col);
}

int readBatteryPct() {
  int raw = 0;
  for (int i = 0; i < 10; i++) { raw += analogRead(BAT_ADC_PIN); delay(2); }
  raw /= 10;
  float v = (raw * 3.3f) / 4095.0f;
  if (raw > 2000) v += 0.09f; else if (raw > 1000) v += 0.05f;
  v *= 2.0f;
  v = constrain(v, 3.2f, 4.2f);
  return (int)((v - 3.2f) / 1.0f * 100.0f);
}

// ====================================================================
//  WiFi helpers
// ====================================================================
void saveCreds() {
  if (!sdOk) return;
  SD.mkdir(DATA_DIR); SD.mkdir(PREFERENCES_DIR);
  SD.remove(WIFI_FILE);
  File f = SD.open(WIFI_FILE, FILE_WRITE);
  if (!f) return;
  DynamicJsonDocument doc(768);
  JsonArray networks = doc.createNestedArray("networks");
  int existing = -1;
  for (int i = 0; i < savedNetworkCount; i++) if (savedNetworks[i].ssid == selSsid) existing = i;
  if (existing < 0 && savedNetworkCount < 5) {
    savedNetworks[savedNetworkCount].ssid = selSsid;
    savedNetworks[savedNetworkCount].password = password;
    savedNetworkCount++;
  } else if (existing >= 0) savedNetworks[existing].password = password;
  for (int i = 0; i < savedNetworkCount; i++) {
    JsonObject network = networks.createNestedObject();
    network["ssid"] = savedNetworks[i].ssid;
    network["password"] = savedNetworks[i].password;
  }
  serializeJson(doc, f); f.close();
}

bool loadCreds() {
  if (!sdOk) return false;
  File f = SD.open(WIFI_FILE);
  if (!f) return false;
  DynamicJsonDocument doc(768);
  if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return false; }
  savedNetworkCount = 0;
  JsonArray networks = doc["networks"].as<JsonArray>();
  if (!networks.isNull()) {
    for (JsonObject network : networks) {
      if (savedNetworkCount >= 5) break;
      savedNetworks[savedNetworkCount].ssid = (const char*)(network["ssid"] | "");
      savedNetworks[savedNetworkCount].password = (const char*)(network["password"] | "");
      if (savedNetworks[savedNetworkCount].ssid.length()) savedNetworkCount++;
    }
  } else {
    // Migrate the v2 single-network JSON format.
    selSsid = (const char*)(doc["ssid"] | "");
    password = (const char*)(doc["password"] | "");
    if (selSsid.length()) {
      savedNetworks[0] = {selSsid, password}; savedNetworkCount = 1;
    }
  }
  if (savedNetworkCount) { selSsid = savedNetworks[0].ssid; password = savedNetworks[0].password; }
  f.close();
  return savedNetworkCount > 0;
}

bool trySavedNetworks(uint32_t timeoutMs) {
  if (!savedNetworkCount) return false;
  for (int i = 0; i < savedNetworkCount; i++) {
    selSsid = savedNetworks[i].ssid; password = savedNetworks[i].password;
    if (tryConnect(timeoutMs)) return true;
  }
  return false;
}

void saveAlarm() {
  if (!sdOk) return;
  SD.mkdir(DATA_DIR); SD.mkdir(PREFERENCES_DIR); SD.remove(ALARM_FILE);
  File f = SD.open(ALARM_FILE, FILE_WRITE);
  if (!f) return;
  DynamicJsonDocument doc(768);
  doc["enabled"] = clockAlarm.enabled; doc["hour"] = clockAlarm.hour; doc["minute"] = clockAlarm.minute;
  doc["year"] = clockAlarm.year; doc["month"] = clockAlarm.month; doc["day"] = clockAlarm.day;
  doc["repeatMask"] = clockAlarm.repeatMask;
  serializeJson(doc, f);
  f.close();
}

bool loadAlarm() {
  if (!sdOk) return false;
  File f = SD.open(ALARM_FILE);
  if (!f) return false;
  DynamicJsonDocument doc(768);
  if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return false; }
  clockAlarm.enabled = doc["enabled"] | false;
  clockAlarm.hour = constrain((int)(doc["hour"] | 7), 0, 23);
  clockAlarm.minute = constrain((int)(doc["minute"] | 0), 0, 59);
  clockAlarm.year = constrain((int)(doc["year"] | 2026), 2024, 2099);
  clockAlarm.month = constrain((int)(doc["month"] | 1), 1, 12);
  clockAlarm.day = constrain((int)(doc["day"] | 1), 1, 31);
  clockAlarm.repeatMask = (doc["repeatMask"] | 0) & 0x7F;
  f.close();
  return true;
}

void saveSettings() {
  if (!sdOk) return;
  SD.mkdir(DATA_DIR); SD.mkdir(PREFERENCES_DIR); SD.remove(SETTINGS_FILE);
  File f = SD.open(SETTINGS_FILE, FILE_WRITE);
  if (!f) return;
  DynamicJsonDocument doc(1024);
  doc["offline"] = settings.offline; doc["wifiEnabled"] = settings.wifiEnabled; doc["use24Hour"] = settings.use24Hour;
  doc["showBatteryPercent"] = settings.showBatteryPercent; doc["notifications"] = settings.notifications;
  doc["calendarEnabled"] = settings.calendarEnabled; doc["manualWeather"] = settings.manualWeather;
  doc["classroomEnabled"] = settings.classroomEnabled; doc["setupComplete"] = settings.setupComplete;
  doc["syncEnabled"] = settings.syncEnabled; doc["syncMain"] = settings.syncMain;
  doc["manualCity"] = manualCity; doc["manualLat"] = manualLat; doc["manualLon"] = manualLon;
  serializeJson(doc, f);
  f.close();
}

void loadSettings() {
  if (!sdOk) return;
  File f = SD.open(SETTINGS_FILE);
  if (!f) return;
  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return; }
  settings.offline = doc["offline"] | false;
  settings.wifiEnabled = doc["wifiEnabled"] | true;
  settings.use24Hour = doc["use24Hour"] | false;
  settings.showBatteryPercent = doc["showBatteryPercent"] | true;
  settings.notifications = doc["notifications"] | true;
  settings.calendarEnabled = doc["calendarEnabled"] | true;
  settings.manualWeather = doc["manualWeather"] | false;
  settings.classroomEnabled = doc["classroomEnabled"] | false;
  settings.setupComplete = doc["setupComplete"] | false;
  settings.syncEnabled = doc["syncEnabled"] | false;
  settings.syncMain = doc["syncMain"] | true;
  String savedCity = (const char*)(doc["manualCity"] | "");
  if (savedCity.length()) manualCity = savedCity;
  manualLat = doc["manualLat"] | manualLat;
  manualLon = doc["manualLon"] | manualLon;
  f.close();
}

void loadClassroomCache() {
  if (!sdOk || !settings.classroomEnabled || !SD.exists(CLASSROOM_CACHE_FILE)) return;
  File f = SD.open(CLASSROOM_CACHE_FILE);
  if (!f) return;
  DynamicJsonDocument doc(16384);
  if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return; }
  f.close();
  JsonArray items = doc["assignments"].as<JsonArray>();
  classroomAssignmentCount = items.size();
  if (items.isNull() || !items.size()) return;
  for (int i = 0; i < min(classroomAssignmentCount, 8); i++) {
    JsonObject item = items[i];
    assignmentTitles[i] = (const char*)(item["title"] | "Assignment");
    assignmentCourses[i] = (const char*)(item["course"] | "");
    JsonObject due = item["dueDate"].as<JsonObject>();
    if (!due.isNull()) {
      char d[24]; snprintf(d, sizeof(d), "%04d-%02d-%02d", (int)(due["year"] | 0),
                           (int)(due["month"] | 0), (int)(due["day"] | 0));
      assignmentDues[i] = d;
    }
  }
  nextAssignment = assignmentTitles[0]; nextAssignmentCourse = assignmentCourses[0]; nextAssignmentDue = assignmentDues[0];
}

String alarmTimeText(const AlarmState& a) {
  char b[12]; int h = a.hour % 12; if (!h) h = 12;
  snprintf(b, sizeof(b), "%d:%02d %s", h, a.minute, a.hour < 12 ? "AM" : "PM");
  return String(b);
}

String nextAlarmText() {
  if (!clockAlarm.enabled) return "No alarm";
  return alarmTimeText(clockAlarm) + (clockAlarm.repeatMask ? "  Repeating" : "  One-time");
}

bool alarmDueNow() {
  if (!clockAlarm.enabled) return false;
  struct tm ti;
  if (!getLocalTime(&ti, 0) || ti.tm_hour != clockAlarm.hour || ti.tm_min != clockAlarm.minute) return false;
  if (clockAlarm.repeatMask) return (clockAlarm.repeatMask & (1 << ti.tm_wday)) != 0;
  return ti.tm_year + 1900 == clockAlarm.year && ti.tm_mon + 1 == clockAlarm.month && ti.tm_mday == clockAlarm.day;
}

void drawAlarmIcon(int cx, int cy, uint16_t color) {
  tft.drawCircle(cx, cy, 9, color);
  tft.drawLine(cx - 7, cy - 8, cx - 11, cy - 13, color);
  tft.drawLine(cx + 7, cy - 8, cx + 11, cy - 13, color);
  tft.drawLine(cx, cy, cx, cy - 5, color);
  tft.drawLine(cx, cy, cx + 4, cy + 3, color);
  tft.drawFastHLine(cx - 12, cy + 11, 24, color);
}

void loaderScreen(const char* msg) {
  tft.fillScreen(UI_BLUE);
  txt(F12B, UI_TEXT, TC_DATUM, msg, 240, 235);
}

bool tryConnect(uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(selSsid.c_str(), password.c_str());
  loaderScreen("connecting");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
    animateLoader(240, 130); delay(16);
  }
  return WiFi.status() == WL_CONNECTED;
}

// ====================================================================
//  screens
// ====================================================================
void showWelcome() {
  screen = S_WELCOME;
  tft.fillScreen(UI_BLUE);
  txt(F24B, 0x0000, TC_DATUM, "Welcome to ClockOS", 240, 78);
  txt(F12B, UI_TEXT, TC_DATUM, "Let's set up your clock", 240, 122);
  txt(F12, UI_TEXT, TC_DATUM, "Would you like to turn Wi-Fi on?", 240, 158);
  alarmButton(24, 214, 200, "Wi-Fi On", true);
  alarmButton(256, 214, 200, "Wi-Fi Off");
}

void showClassroomSetup() {
  screen = S_SETTINGS;
  firstSetupClassroom = true;
  tft.fillScreen(UI_BACK);
  txt(F18B, 0x0000, TC_DATUM, "Google Classroom", 240, 42);
  txt(F12, 0x0000, TC_DATUM, "Show assignments and calendar reminders?", 240, 92);
  alarmButton(28, 144, 190, "Enable", settings.classroomEnabled);
  alarmButton(262, 144, 190, "Not now", !settings.classroomEnabled);
  txt(F12, 0x0000, TC_DATUM, "You can change this in Settings.", 240, 230);
}

void showSyncSetup() {
  screen = S_SETTINGS;
  firstSetupClassroom = false; firstSetupSync = true;
  tft.fillScreen(UI_BACK);
  txt(F18B, 0x0000, TC_DATUM, "Sync displays", 240, 42);
  txt(F12, 0x0000, TC_DATUM, "Share time, alarms, and settings?", 240, 92);
  alarmButton(20, 132, 140, "Off", !settings.syncEnabled);
  alarmButton(170, 132, 140, "Main device", settings.syncEnabled && settings.syncMain);
  alarmButton(320, 132, 140, "Side device", settings.syncEnabled && !settings.syncMain);
  txt(F12, 0x0000, TC_DATUM, "Main sends time and alarms; Side receives them.", 240, 220);
  txt(F12, syncPeerVersion.length() && !syncVersionCompatible(syncPeerVersion) ? UI_RED : 0x0000,
      TC_DATUM, syncCompatibilityText(), 240, 264);
}

void finishFirstSetup() {
  firstSetupClassroom = false;
  showSyncSetup();
}

void scanNetworks() {
  tft.fillScreen(UI_BLUE);
  txt(F12B, UI_TEXT, TC_DATUM, "scanning...", 240, 200);
  WiFi.mode(WIFI_STA); WiFi.disconnect();
  int n = WiFi.scanNetworks();
  nNets = 0;
  for (int i = 0; i < n && nNets < 5; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) continue;
    bool dup = false;
    for (int j = 0; j < nNets; j++) if (ssids[j] == s) dup = true;
    if (dup) continue;
    ssids[nNets] = s; rssis[nNets] = WiFi.RSSI(i); nNets++;
  }
}

void showScan() {
  screen = S_SCAN;
  scanNetworks();
  tft.fillScreen(UI_BLUE);
  drawKey(6, 4, 72, 30, "Back", false, UI_BACK);
  txt(F12B, 0x0000, TC_DATUM, "Choose your WiFi", 270, 8);
  for (int i = 0; i < nNets; i++) {
    String name = ssids[i]; if (name.length() > 26) name = name.substring(0, 26);
    drawKey(10, 42 + i * 46, 460, 38, name.c_str());
  }
  drawKey(10, 276, 460, 34, "Rescan", false, UI_BACK);
}

// ---- keyboard ----
struct Key { int16_t x, y, w, h; char c; uint8_t t; };  // t: 0 char,1 shift,2 back,3 layer,4 space,5 enter,6 show,7 cancel
Key keys[48]; int nk = 0;
const char* ROWS[3][4] = {
  {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"},
  {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"},
  {"!@#$%^&*()", "-_=+[]{};:", ".,<>/?'\"\\|", "~`^_+=%"}
};

void addKey(int x, int y, int w, int h, char c, uint8_t t) { keys[nk++] = {(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, c, t}; }

void keyLabel(const Key& k, char* out) {
  out[0] = 0;
  switch (k.t) {
    case 0: out[0] = k.c; out[1] = 0; break;
    case 1: strcpy(out, "Aa"); break;
    case 2: strcpy(out, "<-"); break;
    case 3: strcpy(out, layer == 2 ? "abc" : "?123"); break;
    case 4: strcpy(out, "space"); break;
    case 5: strcpy(out, "Enter"); break;
    case 6: strcpy(out, showPass ? "Hide" : "Show"); break;
    case 7: strcpy(out, "<"); break;
  }
}

void drawAnyKey(const Key& k, const char* lb, bool pressed) {
  if (k.t >= 6) drawKey(k.x, k.y, k.w, k.h, lb, pressed);      // Show/Hide + back sit on the blue area
  else          drawKbKey(k.x, k.y, k.w, k.h, lb, pressed);
}

// one stretched row: keys share the full width by weight (last key snaps to the edge)
void addRow(int y, int h, int n, const float* wt, const char* ch, const uint8_t* ty) {
  const int X0 = 4, W = 472, G = 3;
  float units = 0; for (int i = 0; i < n; i++) units += wt[i];
  float avail = W - G * (n - 1); int x = X0;
  for (int i = 0; i < n; i++) {
    int wi = (i == n - 1) ? (X0 + W - x) : (int)(avail * wt[i] / units);
    addKey(x, y, wi, h, ch[i], ty[i]);
    x += wi + G;
  }
}

void drawPasswordField() {
  tft.fillRoundRect(60, 44, 330, 38, 8, 0xFFFF);
  String shown;
  if (editingCity || showPass) shown = password; else for (size_t i = 0; i < password.length(); i++) shown += '*';
  if (shown.length() > 20) shown = shown.substring(shown.length() - 20);
  txt(F12B, 0x0000, ML_DATUM, shown, 70, 63);
}

void showKeyboard() {
  screen = S_KEYS;
  tft.fillScreen(UI_BLUE);

  // keyboard body: gradient panel, top corners rounded, bottom + sides run off-screen (square)
  uint16_t top = C(0x38, 0x38, 0x38), bot = C(0x1F, 0x1F, 0x1F);
  tft.fillRoundRect(-20, 112, 520, 40, 16, top);
  tft.fillRectVGradient(0, 132, 480, 188, top, bot);

  nk = 0;
  const int y0 = 120, rh = 36, g = 4;
  float w[12]; char ch[12]; uint8_t ty[12];
  for (int r = 0; r < 3; r++) {
    int n = strlen(ROWS[layer][r]);
    for (int i = 0; i < n; i++) { w[i] = 1; ch[i] = ROWS[layer][r][i]; ty[i] = 0; }
    addRow(y0 + r * (rh + g), rh, n, w, ch, ty);
  }
  int n3 = strlen(ROWS[layer][3]);
  w[0] = 1.5f; ch[0] = 0; ty[0] = 1;
  for (int i = 0; i < n3; i++) { w[i + 1] = 1; ch[i + 1] = ROWS[layer][3][i]; ty[i + 1] = 0; }
  w[n3 + 1] = 1.5f; ch[n3 + 1] = 0; ty[n3 + 1] = 2;
  addRow(y0 + 3 * (rh + g), rh, n3 + 2, w, ch, ty);
  { float bw[3] = {1.5f, 6, 2.5f}; char bc[3] = {0, ' ', 0}; uint8_t bt[3] = {3, 4, 5};
    addRow(y0 + 4 * (rh + g), rh, 3, bw, bc, bt); }
  addKey(398, 44, 78, 38, 0, 6);
  addKey(2, 6, 50, 32, 0, 7);

  String t = editingCity ? "Manual US city" : "Password: " + selSsid; if (t.length() > 30) t = t.substring(0, 30);
  txt(F12B, 0x0000, TC_DATUM, t, 255, 12);
  if (errMsg.length()) txt(F12B, UI_RED, TC_DATUM, errMsg, 240, 92);
  drawPasswordField();
  char lb[8];
  for (int i = 0; i < nk; i++) { keyLabel(keys[i], lb); drawAnyKey(keys[i], lb, false); }
}

void showCityKeyboard() {
  editingCity = true;
  password = manualCity;
  selSsid = "Manual city";
  errMsg = ""; showPass = true; layer = 0;
  showKeyboard();
}

// ---- home (from the sketch) ----
const char* DEMO_EVENT        = "No calendar connected";

// big 7-segment digits (drawn, so they can be any size)
//  bits: 0 top, 1 top-right, 2 bottom-right, 3 bottom, 4 bottom-left, 5 top-left, 6 middle
const uint8_t SEG_MASK[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

void drawDigit(int x, int y, int w, int h, int t, int d, uint16_t col) {
  const int g = 2, vh = h / 2 - 2 * g - 1; uint8_t m = SEG_MASK[d];
  if (m & 0x01) tft.fillRoundRect(x + g, y, w - 2 * g, t, t / 2, col);
  if (m & 0x08) tft.fillRoundRect(x + g, y + h - t, w - 2 * g, t, t / 2, col);
  if (m & 0x40) tft.fillRoundRect(x + g, y + (h - t) / 2, w - 2 * g, t, t / 2, col);
  if (m & 0x20) tft.fillRoundRect(x, y + g + 2, t, vh, t / 2, col);
  if (m & 0x02) tft.fillRoundRect(x + w - t, y + g + 2, t, vh, t / 2, col);
  if (m & 0x10) tft.fillRoundRect(x, y + h / 2 + g, t, vh, t / 2, col);
  if (m & 0x04) tft.fillRoundRect(x + w - t, y + h / 2 + g, t, vh, t / 2, col);
}

void drawBigTime(const char* s, int areaW, int y) {
  const int w = 44, h = 92, t = 10, gap = 8, cw = 20;
  int total = 0, n = strlen(s);
  for (int i = 0; i < n; i++) total += (s[i] == ':' ? cw : w) + (i < n - 1 ? gap : 0);
  int x = (areaW - total) / 2;
  for (int i = 0; i < n; i++) {
    if (s[i] == ':') {
      tft.fillRoundRect(x + cw / 2 - t / 2, y + h / 3 - t / 2, t, t, 3, COL_WHITE);
      tft.fillRoundRect(x + cw / 2 - t / 2, y + 2 * h / 3 - t / 2, t, t, 3, COL_WHITE);
      x += cw + gap;
    } else if (s[i] >= '0' && s[i] <= '9') {
      drawDigit(x, y, w, h, t, s[i] - '0', COL_WHITE);
      x += w + gap;
    } else if (s[i] == '-') {
      tft.fillRoundRect(x + 2, y + (h - t) / 2, w - 4, t, t / 2, COL_WHITE);
      x += w + gap;
    }
  }
}

void drawTimeBlock() {
  tft.fillRect(0, 0, 240, 224, 0x0000);
  struct tm ti; bool ok = getLocalTime(&ti, 0) && ti.tm_year > 120;
  char buf[8];
  if (ok) {
    int h = settings.use24Hour ? ti.tm_hour : ti.tm_hour % 12;
    if (!settings.use24Hour && !h) h = 12;
    snprintf(buf, sizeof(buf), settings.use24Hour ? "%02d:%02d" : "%d:%02d", h, ti.tm_min);
  }
  else strcpy(buf, "--:--");
  drawBigTime(buf, 240, 36);
  if (ok && !settings.use24Hour) {                    // only the current one shows
    if (ti.tm_hour < 12) txt(F24B, COL_WHITE, MC_DATUM, "AM", 70, 175);
    else                 txt(F24B, COL_WHITE, MC_DATUM, "PM", 170, 175);
  }
  if (ok) {
    char date[16]; snprintf(date, sizeof(date), "%02d/%02d/%04d", ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900);
    txt(F12B, COL_DIM, MC_DATUM, date, 120, 208);
  }
}

void drawSdCardIcon(int cx, int cy, bool inserted, bool invalid);

void drawSettingsGear(int cx, int cy, uint16_t color) {
  tft.drawCircle(cx, cy, 9, color); tft.drawCircle(cx, cy, 3, color);
  for (int i = 0; i < 8; i++) {
    float a = i * 0.785398f;
    tft.drawLine(cx + cosf(a) * 9, cy + sinf(a) * 9,
                 cx + cosf(a) * 13, cy + sinf(a) * 13, color);
  }
}

void drawStatusBlock() {
  tft.fillRect(0, 226, 240, 94, 0x0000);
  int lvl = (WiFi.status() == WL_CONNECTED) ? rssiLevel(WiFi.RSSI()) : 0;
  drawWifiSignal(28, 307, lvl, 0x0000, COL_WHITE, 0x39E7);
  drawSdCardIcon(104, 307, sdOk, sdPresent && !sdOk);
  drawProfileIcon(136, 307, COL_DIM);
  int pct = readBatteryPct();
  drawBattery(170, 298, pct, 0x0000, COL_WHITE);
  if (settings.showBatteryPercent) {
    char b[8]; snprintf(b, sizeof(b), "%d%%", pct);
    txt(F12, COL_DIM, ML_DATUM, b, 218, 301);
  }
  drawSettingsGear(229, 307, COL_DIM);
}

void refreshSdState() {
  static uint32_t lastCheck = 0;
  static bool previousOk = false, previousPresent = false;
  if (millis() - lastCheck < 3000) return;
  lastCheck = millis();
  bool present = SD.begin(SD_PIN_CS);
  bool valid = present && SD.exists("/.source") && SD.exists(PREFERENCES_DIR);
  bool changed = present != previousPresent || valid != previousOk;
  sdPresent = present; sdOk = valid;
  previousPresent = present; previousOk = valid;
  if (changed && !screenSleeping) {
    if (screen == S_HOME) drawStatusBlock();
    else if (screen == S_SETTINGS) showSettingsPage();
    else if (screen == S_WIFI) showWifiPage();
    else if (screen == S_WEATHER) showWeatherPage();
  }
}

void drawRightPanel() {
  tft.fillRect(241, 0, 239, 320, 0x0000);
  drawWeatherIcon(312, 8);
  String weatherWord = weather.valid ? weather.label + "  " + String((int)roundf(weather.temperatureF)) : "Weather offline";
  tft.setFreeFont(F18B);
  int tw = tft.textWidth(weatherWord);
  int x0 = 360 - (tw + 14) / 2;
  txt(F18B, COL_WHITE, TL_DATUM, weatherWord, x0, 116);
  tft.drawCircle(x0 + tw + 7, 124, 4, COL_WHITE); tft.drawCircle(x0 + tw + 7, 124, 3, COL_WHITE);
  String place = weather.valid ? weather.city : "Connect WiFi for local weather";
  if (place.length() > 24) place = place.substring(0, 24);
  txt(F12, COL_DIM, TC_DATUM, place, 360, 148);
  tft.drawFastHLine(241, 167, 239, 0x7BEF);
  txt(F18B, COL_WHITE, TL_DATUM, "Calendar", 256, 182);
  if (settings.calendarEnabled && settings.notifications && classroomAssignmentCount > 0) {
    String title = nextAssignment; if (title.length() > 25) title = title.substring(0, 25);
    txt(F12, COL_WHITE, TL_DATUM, title, 256, 228);
    String due = nextAssignmentDue; if (due.length() > 22) due = due.substring(0, 22);
    txt(F12, COL_DIM, TL_DATUM, due, 256, 258);
  } else {
    txt(F12, COL_DIM, TL_DATUM, DEMO_EVENT, 256, 228);
  }
}

int daysInMonth(uint16_t year, uint8_t month) {
  const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) return 29;
  return days[constrain(month, 1, 12) - 1];
}

void adjustAlarmDate(int field, int amount) {
  if (field == 0) {
    int d = alarmDraft.day + amount;
    if (d < 1) d = daysInMonth(alarmDraft.year, alarmDraft.month);
    if (d > daysInMonth(alarmDraft.year, alarmDraft.month)) d = 1;
    alarmDraft.day = d;
  } else if (field == 1) {
    int m = alarmDraft.month + amount;
    if (m < 1) m = 12; if (m > 12) m = 1;
    alarmDraft.month = m;
    alarmDraft.day = min((int)alarmDraft.day, daysInMonth(alarmDraft.year, m));
  } else {
    int y = constrain((int)alarmDraft.year + amount, 2024, 2099);
    alarmDraft.year = y;
    alarmDraft.day = min((int)alarmDraft.day, daysInMonth(y, alarmDraft.month));
  }
}

void adjustClockDate(int field, int amount) {
  if (field == 0) {
    int d = clockDraft.day + amount;
    if (d < 1) d = daysInMonth(clockDraft.year, clockDraft.month);
    if (d > daysInMonth(clockDraft.year, clockDraft.month)) d = 1;
    clockDraft.day = d;
  } else if (field == 1) {
    int m = clockDraft.month + amount;
    if (m < 1) m = 12; if (m > 12) m = 1;
    clockDraft.month = m;
    clockDraft.day = min((int)clockDraft.day, daysInMonth(clockDraft.year, m));
  } else {
    int y = constrain((int)clockDraft.year + amount, 2024, 2099);
    clockDraft.year = y;
    clockDraft.day = min((int)clockDraft.day, daysInMonth(y, clockDraft.month));
  }
}

void alarmButton(int x, int y, int w, const String& label, bool selected) {
  tft.fillRoundRect(x, y, w, 30, 6, selected ? UI_BLUE : UI_KEY);
  tft.drawRoundRect(x, y, w, 30, 6, selected ? UI_BLUE : UI_LIP);
  txt(F12B, selected ? UI_TEXT : 0x0000, MC_DATUM, label, x + w / 2, y + 15);
}

void drawAlarmEditor() {
  tft.fillScreen(UI_BACK);
  txt(F12B, 0x0000, TC_DATUM, "New Alarm", 240, 16);
  txt(F24B, 0x0000, TC_DATUM, alarmTimeText(alarmDraft), 240, 52);
  alarmButton(18, 78, 78, "Hour -"); alarmButton(106, 78, 78, "Hour +");
  alarmButton(296, 78, 78, "Min -"); alarmButton(384, 78, 78, "Min +");

  char date[20]; snprintf(date, sizeof(date), "%02u/%02u/%04u", alarmDraft.month, alarmDraft.day, alarmDraft.year);
  txt(F18B, 0x0000, TC_DATUM, date, 240, 124);
  alarmButton(8, 143, 72, "Day -"); alarmButton(86, 143, 72, "Day +");
  alarmButton(164, 143, 78, "Month -"); alarmButton(248, 143, 78, "Month +");
  alarmButton(332, 143, 68, "Year -"); alarmButton(406, 143, 68, "Year +");

  txt(F12, 0x0000, ML_DATUM, "Repeat", 8, 190);
  const char* names[] = {"S","M","T","W","T","F","S"};
  for (int i = 0; i < 7; i++) alarmButton(8 + i * 67, 205, 60, names[i], alarmDraft.repeatMask & (1 << i));
  alarmButton(8, 266, 112, alarmDraft.enabled ? "Disable" : "Enable");
  alarmButton(250, 266, 100, "Save", true);
  alarmButton(360, 266, 110, "Cancel");
  txt(F12, 0x0000, TC_DATUM, "Tap Save to keep this alarm", 240, 307);
}

void showAlarmEditor() {
  alarmDraft = clockAlarm;
  if (!alarmDraft.enabled) {
    struct tm ti;
    if (getLocalTime(&ti, 0)) {
      alarmDraft.year = ti.tm_year + 1900;
      alarmDraft.month = ti.tm_mon + 1;
      alarmDraft.day = ti.tm_mday;
      alarmDraft.hour = (ti.tm_hour + 1) % 24;
      alarmDraft.minute = 0;
    }
  }
  screen = S_ALARM;
  drawAlarmEditor();
}

void drawClockEditor() {
  tft.fillScreen(UI_BACK);
  txt(F12B, 0x0000, TC_DATUM, "Set date and time", 240, 18);
  txt(F24B, 0x0000, TC_DATUM, alarmTimeText(clockDraft), 240, 55);
  alarmButton(18, 82, 78, "Hour -"); alarmButton(106, 82, 78, "Hour +");
  alarmButton(296, 82, 78, "Min -"); alarmButton(384, 82, 78, "Min +");
  char date[20]; snprintf(date, sizeof(date), "%02u/%02u/%04u", clockDraft.day, clockDraft.month, clockDraft.year);
  txt(F18B, 0x0000, TC_DATUM, date, 240, 128);
  alarmButton(8, 148, 72, "Day -"); alarmButton(86, 148, 72, "Day +");
  alarmButton(164, 148, 78, "Month -"); alarmButton(248, 148, 78, "Month +");
  alarmButton(332, 148, 68, "Year -"); alarmButton(406, 148, 68, "Year +");
  alarmButton(145, 244, 90, "Save", true); alarmButton(250, 244, 100, "Cancel");
  txt(F12, 0x0000, TC_DATUM, "Offline mode asks for this after every restart", 240, 292);
}

void showClockEditor() {
  struct tm ti;
  clockDraft = {true, 12, 0, 2026, 1, 1, 0};
  if (getLocalTime(&ti, 0) && ti.tm_year > 120) {
    clockDraft.hour = ti.tm_hour; clockDraft.minute = ti.tm_min;
    clockDraft.year = ti.tm_year + 1900; clockDraft.month = ti.tm_mon + 1; clockDraft.day = ti.tm_mday;
  }
  screen = S_CLOCK; drawClockEditor();
}

void applyClockDraft() {
  struct tm ti = {};
  ti.tm_year = clockDraft.year - 1900; ti.tm_mon = clockDraft.month - 1;
  ti.tm_mday = clockDraft.day; ti.tm_hour = clockDraft.hour; ti.tm_min = clockDraft.minute; ti.tm_sec = 0;
  time_t epoch = mktime(&ti); struct timeval tv = {epoch, 0}; settimeofday(&tv, nullptr);
}

void showAlarmRinging() {
  alarmRinging = true;
  digitalWrite(LED_G, LOW);
  tft.fillScreen(UI_RED);
  drawAlarmIcon(240, 72, 0xFFFF);
  txt(F24B, 0xFFFF, TC_DATUM, "ALARM", 240, 132);
  txt(F12B, 0xFFFF, TC_DATUM, alarmTimeText(clockAlarm), 240, 184);
  alarmButton(150, 245, 180, "Dismiss", false);
}

void dismissAlarm() {
  alarmRinging = false;
  digitalWrite(LED_G, HIGH);
  if (!clockAlarm.repeatMask) { clockAlarm.enabled = false; saveAlarm(); }
  showHome();
}

void drawSdCardIcon(int cx, int cy, bool inserted, bool invalid = false) {
  uint16_t blue = C(0, 122, 255);
  uint16_t fill = inserted ? blue : UI_BACK;
  tft.fillRoundRect(cx - 13, cy - 10, 26, 20, 3, fill);
  tft.drawRoundRect(cx - 13, cy - 10, 26, 20, 3, blue);
  for (int i = 0; i < 3; i++) tft.drawFastVLine(cx - 7 + i * 5, cy - 7, 5, inserted ? 0xFFFF : blue);
  tft.drawFastHLine(cx - 8, cy + 6, 16, inserted ? 0xFFFF : blue);
  if (invalid) {
    tft.drawLine(cx - 8, cy - 6, cx + 8, cy + 6, UI_RED);
    tft.drawLine(cx + 8, cy - 6, cx - 8, cy + 6, UI_RED);
  }
}

void showSdSaveWarning() {
  tft.fillRoundRect(34, 236, 412, 60, 14, C(255, 244, 224));
  drawSdCardIcon(64, 266, false, sdPresent);
  txt(F12B, C(130, 80, 10), ML_DATUM, sdPresent ? "SD card is not ClockOS-ready" : "SD card not inserted", 94, 254);
  txt(F12, C(130, 80, 10), ML_DATUM, "Changes will not be saved", 94, 276);
  delay(1600);
}

void leaveSettingsPage() {
  if (!sdOk) showSdSaveWarning();
  showHome();
}

bool prepareSdCard() {
  if (!sdPresent && !SD.begin(SD_PIN_CS)) {
    sdCardMessage = "No SD card detected.";
    return false;
  }
  sdPresent = true;
  // Prepare the ClockOS layout without erasing unrelated user files.
  bool ok = SD.mkdir("/.source") || SD.exists("/.source");
  ok = (SD.mkdir(DATA_DIR) || SD.exists(DATA_DIR)) && ok;
  ok = (SD.mkdir(PREFERENCES_DIR) || SD.exists(PREFERENCES_DIR)) && ok;
  ok = (SD.mkdir("/.source/data/secrets") || SD.exists("/.source/data/secrets")) && ok;
  ok = (SD.mkdir("/.source/icons") || SD.exists("/.source/icons")) && ok;
  ok = (SD.mkdir("/.source/compiled") || SD.exists("/.source/compiled")) && ok;
  ok = (SD.mkdir("/.source/compiled/updates") || SD.exists("/.source/compiled/updates")) && ok;
  if (!ok) { sdOk = false; sdCardMessage = "Could not prepare this SD card."; return false; }
  sdOk = true;
  saveSettings(); saveAlarm(); saveCreds(); saveCal();
  sdCardMessage = "ClockOS storage is ready. Current settings saved.";
  return true;
}

void showSdCardPage() {
  screen = S_SDCARD; drawBackHeader("SD Card");
  drawSdCardIcon(240, 78, sdOk, sdPresent && !sdOk);
  txt(F18B, 0x0000, TC_DATUM, sdOk ? "ClockOS card ready" : (sdPresent ? "Card needs setup" : "No card detected"), 240, 124);
  if (sdPresent) {
    uint64_t total = SD.cardSize() / (1024ULL * 1024ULL);
    uint64_t used = SD.usedBytes() / (1024ULL * 1024ULL);
    char storage[64]; snprintf(storage, sizeof(storage), "Storage: %llu MB used of %llu MB", used, total);
    txt(F12, 0x0000, TC_DATUM, storage, 240, 150);
  }
  alarmButton(20, 174, 215, "Format / prepare", true);
  alarmButton(245, 174, 215, "View files");
  txt(F12, 0x0000, TC_DATUM, "Creates folders and saves current settings", 240, 232);
  txt(F12, 0x0000, TC_DATUM, "without deleting unrelated files.", 240, 252);
  if (sdCardMessage.length()) txt(F12, sdOk ? UI_GREEN : UI_RED, TC_DATUM, sdCardMessage, 240, 286);
}

void showSdFilesPage() {
  screen = S_SDFILES; drawBackHeader("SD Card Files");
  if (!sdPresent) {
    txt(F12B, UI_RED, TC_DATUM, "No SD card detected", 240, 96);
    return;
  }
  txt(F12, 0x0000, TC_DATUM, "Read-only viewer", 240, 58);
  File root = SD.open("/");
  int row = 0;
  while (root && row < 9) {
    File entry = root.openNextFile();
    if (!entry) break;
    String name = String(entry.name());
    if (name.length() > 30) name = name.substring(name.length() - 30);
    String line = entry.isDirectory() ? "[DIR] " + name : name + "  " + String((unsigned long)entry.size()) + " B";
    txt(F12, 0x0000, ML_DATUM, line, 24, 88 + row * 22);
    entry.close(); row++;
  }
  if (root) root.close();
  if (!row) txt(F12, 0x0000, TC_DATUM, "No readable files", 240, 110);
}

void showProfilePage() {
  screen = S_PROFILE; drawBackHeader("Profile");
  tft.fillRoundRect(34, 50, 412, 190, 22, 0xFFFF);
  drawProfileIcon(105, 116, C(120, 125, 138));
  txt(F18B, C(25, 26, 32), ML_DATUM, profileName, 145, 104);
  txt(F12, C(100, 102, 112), ML_DATUM, settings.classroomEnabled ? "Google Classroom sync enabled" : "Classroom sync disabled", 145, 132);
  alarmButton(82, 176, 316, "Sign in with Clock Setup", true);
  txt(F12, C(85, 88, 98), TC_DATUM, "Use the Windows setup app to authorize Google.", 240, 270);
}

void drawBackHeader(const String& title) {
  tft.fillScreen(UI_BACK);
  alarmButton(8, 8, 78, "Back");
  txt(F18B, 0x0000, TC_DATUM, title, 240, 24);
  drawProfileIcon(405, 24, C(80, 84, 96));
  drawSdCardIcon(438, 24, sdOk, sdPresent && !sdOk);
}

void settingsRow(int x, int y, const String& label, const String& value = "", bool accent = false) {
  uint16_t blue = C(0, 122, 255);
  tft.fillRoundRect(x, y, 210, 34, 10, 0xFFFF);
  txt(F12, C(30, 32, 38), ML_DATUM, label, x + 12, y + 17);
  if (value.length()) txt(F12, accent ? blue : C(110, 112, 120), MR_DATUM, value, x + 198, y + 17);
}

void showSettingsPage() {
  screen = S_SETTINGS; drawBackHeader("Settings");
  settingsRow(14, 50, "Offline mode", settings.offline ? "ON" : "OFF", settings.offline);
  settingsRow(256, 50, "Time", settings.use24Hour ? "24 hour" : "12 hour", settings.use24Hour);
  settingsRow(14, 92, "Battery percent", settings.showBatteryPercent ? "ON" : "OFF", settings.showBatteryPercent);
  settingsRow(256, 92, "Classroom", settings.classroomEnabled ? "ON" : "OFF", settings.classroomEnabled);
  settingsRow(14, 134, "Calendar", settings.calendarEnabled ? "ON" : "OFF", settings.calendarEnabled);
  settingsRow(256, 134, "Weather", settings.manualWeather ? "Manual" : "Automatic", settings.manualWeather);
  settingsRow(14, 184, "SD Card", sdOk ? "Ready" : (sdPresent ? "Needs setup" : "None"), sdOk);
  settingsRow(256, 184, "Wi-Fi", WiFi.status() == WL_CONNECTED ? "Connected" : "Offline", WiFi.status() == WL_CONNECTED);
  settingsRow(14, 226, "Classroom", settings.classroomEnabled ? "ON" : "OFF", settings.classroomEnabled);
  settingsRow(256, 226, "Weather settings", ">", false);
  alarmButton(120, 266, 240, factoryResetArmed ? "Tap again to reset" : "Factory reset", factoryResetArmed);
  txt(F12, 0x0000, TC_DATUM, "Orientation: permanent 180 degrees", 240, 311);
}

void drawAppleUpdateIcon(int cx, int cy, uint16_t blue) {
  tft.fillRoundRect(cx - 28, cy - 34, 56, 68, 12, blue);
  tft.fillRoundRect(cx - 18, cy - 24, 36, 48, 7, 0xFFFF);
  tft.fillRoundRect(cx - 10, cy - 6, 20, 12, 5, blue);
  tft.fillCircle(cx, cy + 17, 2, blue);
  tft.drawLine(cx + 8, cy - 30, cx + 18, cy - 40, blue);
  tft.drawLine(cx + 18, cy - 40, cx + 24, cy - 34, blue);
}

void drawAppleButton(int x, int y, int w, int h, const String& label, bool primary = false) {
  uint16_t blue = C(0, 122, 255);
  tft.fillRoundRect(x, y, w, h, h / 2, primary ? blue : C(232, 237, 245));
  txt(F12B, primary ? 0xFFFF : blue, MC_DATUM, label, x + w / 2, y + h / 2);
}

void drawUpdatePage(int pct, const String& status) {
  screen = S_UPDATE;
  uint16_t bg = C(242, 244, 248), card = 0xFFFF, blue = C(0, 122, 255);
  tft.fillScreen(bg);
  tft.fillRoundRect(34, 22, 412, 276, 22, card);
  drawAppleUpdateIcon(240, 76, blue);
  txt(F18B, C(20, 20, 24), TC_DATUM, "Software Update", 240, 128);
  txt(F12, C(100, 105, 115), TC_DATUM, String(CLOCKOS_NAME) + " " + CLOCKOS_VERSION, 240, 153);
  String line = status;
  if (line.length() > 38) line = line.substring(0, 38);
  txt(F12, C(75, 78, 88), TC_DATUM, line, 240, 181);
  tft.fillRoundRect(78, 203, 324, 12, 6, C(220, 225, 234));
  int fill = constrain(pct, 0, 100) * 320 / 100;
  if (fill > 0) tft.fillRoundRect(80, 205, fill, 8, 4, blue);
  char progress[8]; snprintf(progress, sizeof(progress), "%d%%", constrain(pct, 0, 100));
  txt(F12B, C(50, 52, 60), TC_DATUM, progress, 240, 235);
  txt(F12, C(115, 120, 130), TC_DATUM, "Updates keep your clock secure and current.", 240, 258);
}

void factoryResetClock() {
  if (sdOk) {
    SD.remove(SETTINGS_FILE); SD.remove(ALARM_FILE); SD.remove(WIFI_FILE); SD.remove(TOUCH_FILE);
    SD.rmdir(PREFERENCES_DIR);
    SD.remove(CLASSROOM_CACHE_FILE); SD.remove(CLASSROOM_TOKEN_FILE);
    SD.remove("/.source/data/secrets/classroom_token.json");
    SD.remove("/.source/data/secrets/classroom_client.json");
    settings.setupComplete = false; settings.classroomEnabled = false;
  }
  factoryResetArmed = false;
  delay(250);
  ESP.restart();
}

void sleepDisplay() {
  if (screenSleeping) return;
  tft.writecommand(0x28); delay(20); tft.writecommand(0x10);
  screenSleeping = true;
}

void wakeDisplay() {
  tft.writecommand(0x11); delay(120); tft.writecommand(0x29);
  screenSleeping = false; lastActivity = millis(); showHome();
}

void showWifiPage() {
  screen = S_WIFI; drawBackHeader("Wi-Fi");
  txt(F12B, 0x0000, TC_DATUM, WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "Not connected", 240, 62);
  txt(F12, 0x0000, TC_DATUM, settings.offline ? "Offline clock mode is active" : "Online mode", 240, 88);
  alarmButton(25, 120, 190, "Change network");
  alarmButton(265, 120, 190, settings.offline ? "Go online" : "Go offline", settings.offline);
  alarmButton(25, 170, 190, "Refresh networks");
  txt(F12, 0x0000, TC_DATUM, "Offline mode keeps the clock running without Wi-Fi.", 240, 228);
  txt(F12, 0x0000, TC_DATUM, "A restart in offline mode uses saved local settings.", 240, 250);
}

void showWeatherPage() {
  screen = S_WEATHER; drawBackHeader("Weather location");
  txt(F12B, 0x0000, TC_DATUM, weather.valid ? weather.city : "No weather loaded", 240, 62);
  alarmButton(25, 100, 190, "Automatic Wi-Fi location", !settings.manualWeather);
  alarmButton(265, 100, 190, "Manual US city", settings.manualWeather);
  alarmButton(25, 145, 430, String("City: ") + manualCity);
  txt(F12, 0x0000, TC_DATUM, "Automatic uses approximate public-IP location.", 240, 166);
  txt(F12, 0x0000, TC_DATUM, "Manual city geocoding will use Open-Meteo.", 240, 188);
  alarmButton(150, 240, 180, "Refresh weather", true);
}

void showCalendarPage() {
  screen = S_CALENDAR; drawBackHeader("Calendar");
  struct tm ti; bool ok = getLocalTime(&ti, 0);
  int year = ok ? ti.tm_year + 1900 : 2026, month = ok ? ti.tm_mon + 1 : 1;
  char title[32]; snprintf(title, sizeof(title), "%02d/%04d", month, year);
  txt(F18B, 0x0000, TC_DATUM, title, 240, 62);
  const char* days = "S   M   T   W   T   F   S";
  txt(F12B, 0x0000, TC_DATUM, days, 240, 92);
  int first = 0; // compact monthly grid; live calendar integration is separate from the clock.
  int maxDay = daysInMonth(year, month);
  for (int d = 1; d <= maxDay; d++) {
    int pos = first + d - 1, x = 35 + (pos % 7) * 67, y = 112 + (pos / 7) * 20;
    txt(F12, d == (ok ? ti.tm_mday : 1) ? UI_BLUE : 0x0000, MC_DATUM, String(d), x, y);
  }
  if (settings.calendarEnabled && classroomAssignmentCount > 0) {
    for (int i = 0; i < min(classroomAssignmentCount, 3); i++) {
      String line = assignmentTitles[i] + "  " + assignmentDues[i];
      if (line.length() > 46) line = line.substring(0, 46);
      txt(F12, 0x0000, TL_DATUM, line, 250, 228 + i * 24);
    }
  } else {
    txt(F12, 0x0000, TC_DATUM, settings.calendarEnabled ? "Google Classroom not authorized" : "Calendar disabled in Settings", 240, 285);
  }
}

void showHome() {
  screen = S_HOME;
  tft.fillScreen(0x0000);
  tft.drawFastVLine(240, 0, 320, 0x7BEF);
  tft.drawFastHLine(0, 225, 240, 0x7BEF);
  drawTimeBlock(); drawStatusBlock(); drawRightPanel();
}

// ---- GitHub update (fetches newest .bin from .source/compiled in the repo) ----
void updateProgress(int pct) {
  static int last = -1;
  if (pct != last) { last = pct; drawUpdatePage(pct, "Downloading and installing..."); }
}

void runUpdate() {
  if (WiFi.status() != WL_CONNECTED) return;
  drawUpdatePage(0, "Checking for updates...");
  BL::Result r = BL::checkAndInstall(updateProgress);
  if (r == BL::INSTALLED) {
    drawUpdatePage(100, "Update installed. Restarting...");
    delay(1200); ESP.restart();
  } else if (r == BL::UP_TO_DATE) {
    drawUpdatePage(100, "Your ClockOS is up to date.");
    delay(900);
  } else {
    drawUpdatePage(0, "Could not check right now. Try again later.");
    delay(900);
  }
}

// ====================================================================
void setup() {
  Serial.begin(115200);
  pinMode(LED_R, OUTPUT); pinMode(LED_G, OUTPUT); pinMode(LED_B, OUTPUT);
  digitalWrite(LED_R, HIGH); digitalWrite(LED_G, HIGH); digitalWrite(LED_B, HIGH);
  analogReadResolution(12); analogSetAttenuation(ADC_11db);

  tft.init(); tft.setRotation(TFT_ROTATION);
  UI_BLUE = C(0x62, 0xAB, 0xFF); UI_BACK = C(0xC3, 0xC8, 0xDE); UI_TEXT = C(0x41, 0x48, 0x56);
  UI_KEY = C(0xE8, 0xEC, 0xF5);  UI_LIP = C(0x8D, 0x96, 0xB3);
  UI_RED = C(255, 60, 60); UI_YEL = C(255, 230, 0); UI_GREEN = C(0xAD, 0xFF, 0x2F);

  spr.setColorDepth(16); spr.createSprite(100, 100);

  SPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);   // SD on the default (VSPI) bus; TFT uses HSPI
  sdPresent = SD.begin(SD_PIN_CS);
  sdOk = sdPresent && SD.exists("/.source") && SD.exists(PREFERENCES_DIR);
  if (!sdPresent) Serial.println("SD not found");
  else if (!sdOk) Serial.println("SD present but not formatted for ClockOS");

  ensureCalibration();
  loadSettings();
  loadAlarm();
  loadClassroomCache();
  lastActivity = millis();

  if (!settings.setupComplete) {
    showWelcome();
  } else if (settings.offline || !settings.wifiEnabled) {
    showClockEditor();
  } else if (sdOk && loadCreds() && trySavedNetworks(15000)) {
    configTzTime(TIME_TZ, NTP_SERVER);
    fetchWeather();
    runUpdate();
    showHome();
  } else {
    showHome();
  }
}

void loop() {
  int x, y;
  static uint32_t lastTick = 0;
  static int lastMin = -1;

  refreshSdState();

  if (screenSleeping) {
    if (readTouch(x, y)) wakeDisplay();
    delay(20);
    return;
  }
  if (screen == S_HOME && millis() - lastActivity >= 60000UL) {
    sleepDisplay();
    return;
  }

  if (screen == S_WELCOME) {
    if (readTouch(x, y) && y >= 190) {
      if (x < 240) { settings.wifiEnabled = true; showScan(); }
      else { settings.wifiEnabled = false; settings.offline = true; showClassroomSetup(); }
    }

  } else if (screen == S_SCAN) {
    if (readTouch(x, y)) {
      if (x < 85 && y < 40) { showHome(); return; }
      if (y > 270) { showScan(); return; }
      int i = (y - 42) / 46;
      if (i >= 0 && i < nNets && y >= 42) {
        selSsid = ssids[i]; password = ""; errMsg = ""; layer = 0; showPass = false;
        showKeyboard();
      }
    }

  } else if (screen == S_KEYS) {
    if (readTouch(x, y)) {
      for (int i = 0; i < nk; i++) {
        Key& k = keys[i];
        if (x < k.x || x > k.x + k.w || y < k.y || y > k.y + k.h) continue;
        char lb[8]; keyLabel(k, lb);
        drawAnyKey(k, lb, true); delay(60); drawAnyKey(k, lb, false);
        switch (k.t) {
          case 0: case 4: if (password.length() < 63) password += k.c; drawPasswordField(); break;
          case 1: layer = (layer == 1) ? 0 : 1; showKeyboard(); break;
          case 2: if (password.length()) password.remove(password.length() - 1); drawPasswordField(); break;
          case 3: layer = (layer == 2) ? 0 : 2; showKeyboard(); break;
          case 6: showPass = !showPass; showKeyboard(); break;
          case 7:
            if (editingCity) { editingCity = false; showWeatherPage(); }
            else showScan();
            break;
          case 5:
            if (editingCity) {
              manualCity = password; manualCity.trim();
              if (geocodeManualCity() || settings.manualWeather) {
                settings.manualWeather = true; saveSettings(); editingCity = false; fetchWeather(); showWeatherPage();
              } else {
                errMsg = "connect Wi-Fi to find that city"; showKeyboard();
              }
            } else if (tryConnect(15000)) {
              saveCreds();
              configTzTime(TIME_TZ, NTP_SERVER);
              fetchWeather();
              loadAlarm();
              runUpdate();
              showClassroomSetup();
            } else {
              errMsg = "could not connect - check password";
              showKeyboard();
            }
            break;
        }
        break;
      }
    }

  } else if (screen == S_HOME) {
    if (alarmRinging) {
      if (readTouch(x, y)) dismissAlarm();
      return;
    }
    if (readTouch(x, y)) {
      if (x >= 240 && y < 168) { showWeatherPage(); return; }
      if (x >= 240 && y >= 168) { showCalendarPage(); return; }
      if (x < 240 && y >= 270 && x < 95) { showWifiPage(); return; }
      if (x < 240 && y >= 270 && x >= 95 && x < 155) { showProfilePage(); return; }
      if (x < 240 && y >= 270 && x >= 150) { showSettingsPage(); return; }
      if (x < 240 && y >= 190) { settings.use24Hour = !settings.use24Hour; saveSettings(); drawTimeBlock(); return; }
      if (x < 240 && y < 190) { showAlarmEditor(); return; }
    }
    if (alarmDueNow()) { showAlarmRinging(); return; }
    if (millis() - lastTick > 1000) {
      lastTick = millis();
      struct tm ti;
      if (getLocalTime(&ti, 0) && ti.tm_min != lastMin) { lastMin = ti.tm_min; drawTimeBlock(); }
      static uint32_t lastStatus = 0;
      if (millis() - lastStatus > 30000) { lastStatus = millis(); drawStatusBlock(); }
      if (millis() - lastWeatherFetch > WEATHER_REFRESH_MS) {
        fetchWeather(); drawRightPanel();
      }
      loadClassroomCache(); drawRightPanel();
      static uint32_t lastUpdate = millis();
      if (millis() - lastUpdate > UPDATE_CHECK_MS) { lastUpdate = millis(); runUpdate(); showHome(); }
    }
  } else if (screen == S_CLOCK) {
    if (!readTouch(x, y)) return;
    if (y >= 82 && y < 116) {
      if (x < 100) clockDraft.hour = (clockDraft.hour + 23) % 24;
      else if (x < 200) clockDraft.hour = (clockDraft.hour + 1) % 24;
      else if (x >= 285 && x < 380) clockDraft.minute = (clockDraft.minute + 59) % 60;
      else if (x >= 380) clockDraft.minute = (clockDraft.minute + 1) % 60;
      drawClockEditor();
    } else if (y >= 148 && y < 182) {
      if (x < 82) adjustClockDate(0, -1); else if (x < 160) adjustClockDate(0, 1);
      else if (x < 245) adjustClockDate(1, -1); else if (x < 330) adjustClockDate(1, 1);
      else if (x < 405) adjustClockDate(2, -1); else adjustClockDate(2, 1);
      drawClockEditor();
    } else if (y >= 244 && y < 280) {
      if (x >= 140 && x < 240) { applyClockDraft(); showHome(); }
      else if (x >= 240) showHome();
    }
  } else if (screen == S_SETTINGS) {
    if (!readTouch(x, y)) return;
    if (swipeBackDetected) { swipeBackDetected = false; showHome(); return; }
    if (firstSetupClassroom) {
      if (y >= 135 && y < 205 && x < 240) { settings.classroomEnabled = true; finishFirstSetup(); return; }
      if (y >= 135 && y < 205 && x >= 240) { settings.classroomEnabled = false; finishFirstSetup(); return; }
      return;
    }
    if (firstSetupSync) {
      if (y >= 125 && y < 205 && x < 160) { settings.syncEnabled = false; settings.syncMain = true; }
      else if (y >= 125 && y < 205 && x < 320) { settings.syncEnabled = true; settings.syncMain = true; }
      else if (y >= 125 && y < 205) { settings.syncEnabled = true; settings.syncMain = false; }
      else return;
      firstSetupSync = false; settings.setupComplete = true; saveSettings(); showHome(); return;
    }
    if (x < 100 && y < 45) { leaveSettingsPage(); return; }
    if (x >= 400 && y < 50) { showSdCardPage(); return; }
    if (y >= 50 && y < 84 && x < 240) settings.offline = !settings.offline;
    else if (y >= 50 && y < 84 && x >= 240) settings.use24Hour = !settings.use24Hour;
    else if (y >= 92 && y < 126 && x < 240) settings.showBatteryPercent = !settings.showBatteryPercent;
    else if (y >= 92 && y < 126 && x >= 240) settings.classroomEnabled = !settings.classroomEnabled;
    else if (y >= 134 && y < 168 && x < 240) settings.calendarEnabled = !settings.calendarEnabled;
    else if (y >= 134 && y < 168 && x >= 240) settings.manualWeather = !settings.manualWeather;
    else if (y >= 184 && y < 218 && x < 240) { showSdCardPage(); return; }
    else if (y >= 184 && y < 218 && x >= 240) { saveSettings(); showWifiPage(); return; }
    else if (y >= 226 && y < 260 && x < 240) { settings.classroomEnabled = !settings.classroomEnabled; saveSettings(); showSettingsPage(); return; }
    else if (y >= 226 && y < 260 && x >= 240) { saveSettings(); showWeatherPage(); return; }
    else if (y >= 266 && y < 301 && x >= 100 && x <= 380) {
      if (factoryResetArmed) factoryResetClock();
      factoryResetArmed = true; showSettingsPage(); return;
    }
    saveSettings(); showSettingsPage();
  } else if (screen == S_SDCARD) {
    if (!readTouch(x, y)) return;
    if (swipeBackDetected) { swipeBackDetected = false; showSettingsPage(); return; }
    if (x < 100 && y < 45) { showSettingsPage(); return; }
    if (y >= 174 && y < 214 && x < 240) { prepareSdCard(); showSdCardPage(); return; }
    if (y >= 174 && y < 214 && x >= 240) { showSdFilesPage(); return; }
  } else if (screen == S_SDFILES) {
    if (readTouch(x, y)) {
      if (swipeBackDetected) { swipeBackDetected = false; showSdCardPage(); return; }
      if (x < 100 && y < 45) showSdCardPage();
    }
  } else if (screen == S_PROFILE) {
    if (readTouch(x, y)) {
      if (swipeBackDetected) { swipeBackDetected = false; showHome(); return; }
      if (x < 100 && y < 45) showHome();
    }
  } else if (screen == S_WIFI) {
    if (!readTouch(x, y)) return;
    if (swipeBackDetected) { swipeBackDetected = false; showHome(); return; }
    if (x < 100 && y < 45) { showHome(); return; }
    if (y >= 120 && y < 155 && x < 230) { showScan(); return; }
    if (y >= 120 && y < 155 && x >= 230) { settings.offline = !settings.offline; saveSettings(); showWifiPage(); return; }
    if (y >= 170 && y < 205) { showScan(); return; }
  } else if (screen == S_WEATHER) {
    if (!readTouch(x, y)) return;
    if (swipeBackDetected) { swipeBackDetected = false; showHome(); return; }
    if (x < 100 && y < 45) { showHome(); return; }
    if (y >= 100 && y < 140 && x < 240) { settings.manualWeather = false; saveSettings(); fetchWeather(); showWeatherPage(); return; }
    if (y >= 100 && y < 140 && x >= 240) { settings.manualWeather = true; saveSettings(); fetchWeather(); showWeatherPage(); return; }
    if (y >= 145 && y < 180) { showCityKeyboard(); return; }
    if (y >= 230 && y < 280) { fetchWeather(); showWeatherPage(); return; }
  } else if (screen == S_CALENDAR) {
    if (readTouch(x, y)) {
      if (swipeBackDetected) { swipeBackDetected = false; showHome(); return; }
      if (x < 100 && y < 45) showHome();
    }
  } else if (screen == S_ALARM) {
    if (!readTouch(x, y)) return;
    if (y >= 78 && y < 112) {
      if (x < 100) alarmDraft.hour = (alarmDraft.hour + 23) % 24;
      else if (x < 200) alarmDraft.hour = (alarmDraft.hour + 1) % 24;
      else if (x >= 285 && x < 380) alarmDraft.minute = (alarmDraft.minute + 59) % 60;
      else if (x >= 380) alarmDraft.minute = (alarmDraft.minute + 1) % 60;
      drawAlarmEditor();
    } else if (y >= 143 && y < 177) {
      if (x < 82) adjustAlarmDate(0, -1); else if (x < 160) adjustAlarmDate(0, 1);
      else if (x < 245) adjustAlarmDate(1, -1); else if (x < 330) adjustAlarmDate(1, 1);
      else if (x < 405) adjustAlarmDate(2, -1); else adjustAlarmDate(2, 1);
      drawAlarmEditor();
    } else if (y >= 205 && y < 240) {
      int i = x / 67; if (i >= 0 && i < 7) alarmDraft.repeatMask ^= (1 << i);
      drawAlarmEditor();
    } else if (y >= 266) {
      if (x < 130) alarmDraft.enabled = !alarmDraft.enabled;
      else if (x >= 240 && x < 355) { clockAlarm = alarmDraft; saveAlarm(); showHome(); }
      else if (x >= 355) showHome();
      else drawAlarmEditor();
    }
  }
}
