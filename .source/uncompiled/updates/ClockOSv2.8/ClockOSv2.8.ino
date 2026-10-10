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
void drawBackHeader(const String& title, bool showGlobal = true);
void settingsRow(int x, int y, const String& label, const String& value = "", bool accent = false);
void settingsGroupRow(int y, const String& label, const String& value = "", bool accent = false);
void showSettingsPage();
void showHome();
void showFactoryResetPage();
void drawAppleButton(int x, int y, int w, int h, const String& label, bool primary = false);
void drawAlarmEditor();

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

// ---------------- colors ----------------
#define C(r,g,b) tft.color565(r,g,b)
uint16_t UI_BLUE, UI_BACK, UI_TEXT, UI_KEY, UI_LIP, UI_RED, UI_YEL, UI_GREEN;
uint16_t UI_ACTION, UI_ON_ACTION, UI_MUTED, UI_SEPARATOR, UI_SELECTION, UI_DISABLED;
String appearanceTheme = "midnight";

void applyAppearanceTheme() {
  // Semantic palette values are RGB565-ready inputs; keep action contrast
  // separate from a theme's decorative accent color.
  if (appearanceTheme == "midnight") {
    UI_BLUE = C(94, 155, 255); UI_BACK = C(9, 11, 18); UI_TEXT = C(245, 247, 255);
    UI_KEY = C(23, 27, 39); UI_LIP = C(0, 0, 0); UI_ACTION = C(0, 102, 214); UI_ON_ACTION = C(255, 255, 255);
    UI_MUTED = C(154, 164, 184); UI_SEPARATOR = C(56, 58, 62); UI_SELECTION = C(24, 58, 99); UI_DISABLED = C(99, 99, 102);
  } else if (appearanceTheme == "ocean") {
    UI_BLUE = C(0, 102, 204); UI_BACK = C(234, 247, 255); UI_TEXT = C(8, 36, 61);
    UI_KEY = C(255, 255, 255); UI_LIP = C(130, 190, 220); UI_ACTION = C(0, 102, 204); UI_ON_ACTION = C(255, 255, 255);
    UI_MUTED = C(84, 115, 140); UI_SEPARATOR = C(202, 223, 237); UI_SELECTION = C(215, 236, 251); UI_DISABLED = C(142, 147, 153);
  } else if (appearanceTheme == "sunrise") {
    UI_BLUE = C(255, 107, 53); UI_BACK = C(255, 246, 238); UI_TEXT = C(50, 27, 22);
    UI_KEY = C(255, 255, 255); UI_LIP = C(220, 160, 130); UI_ACTION = C(200, 77, 34); UI_ON_ACTION = C(255, 255, 255);
    UI_MUTED = C(122, 84, 74); UI_SEPARATOR = C(235, 213, 204); UI_SELECTION = C(255, 226, 213); UI_DISABLED = C(142, 147, 153);
  } else if (appearanceTheme == "graphite") {
    UI_BLUE = C(215, 220, 229); UI_BACK = C(32, 34, 37); UI_TEXT = C(255, 255, 255);
    UI_KEY = C(48, 50, 56); UI_LIP = C(32, 34, 37); UI_ACTION = C(215, 220, 229); UI_ON_ACTION = C(32, 34, 37);
    UI_MUTED = C(162, 168, 181); UI_SEPARATOR = C(73, 75, 84); UI_SELECTION = C(71, 75, 84); UI_DISABLED = C(99, 99, 102);
  } else {
    UI_BLUE = C(0, 122, 255); UI_BACK = C(242, 244, 248); UI_TEXT = C(20, 21, 26);
    UI_KEY = C(255, 255, 255); UI_LIP = C(209, 214, 222); UI_ACTION = C(0, 102, 214); UI_ON_ACTION = C(255, 255, 255);
    UI_MUTED = C(107, 114, 128); UI_SEPARATOR = C(209, 209, 214); UI_SELECTION = C(220, 235, 255); UI_DISABLED = C(142, 147, 153);
  }
  UI_RED = C(255, 69, 58); UI_YEL = C(255, 214, 10); UI_GREEN = C(48, 209, 88);
}

enum Screen { S_WELCOME, S_SCAN, S_KEYS, S_HOME, S_ALARM, S_CLOCK, S_SETTINGS, S_WIFI, S_WEATHER, S_CALENDAR, S_UPDATE, S_SDCARD, S_SDFILES, S_PROFILE, S_APPEARANCE, S_DEV_PIN, S_DEV_MODE, S_FACTORY_RESET };
Screen screen = S_WELCOME;

// ClockOS v2.7 uses a same-name Arduino sketch folder and a matching update
// payload so the Arduino IDE, installer, and OTA feed share one identity.
static const char* CLOCKOS_NAME = "ClockOS";
static const char* CLOCKOS_VERSION = "v2.8";
static const char* CLOCKOS_FIRMWARE = "ClockOSv2.8";
static const char* SYNC_PROTOCOL_VERSION = "2.8";
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
Screen scanReturnScreen = S_HOME;
String scanStatus = "";
bool showPass = false; uint8_t layer = 0;
bool sdOk = false;       // valid ClockOS-formatted SD card
bool sdPresent = false;  // SD hardware/card responds, but may be unformatted
String sdCardMessage = "";
bool screenSleeping = false;
Screen screenBeforeSleep = S_HOME;
bool factoryResetArmed = false;
uint8_t settingsCategory = 0; // 0 General, 1 Wi-Fi, 2 Appearance, 3 Calendar, 4 Storage, 5 System
bool settingsAtRoot = true;
uint32_t lastActivity = 0;
bool swipeBackDetected = false;
uint8_t versionTapCount = 0;
uint32_t versionTapWindow = 0;
String developerPinInput = "";
bool developerModeUnlocked = false;

// ====================================================================
//  text helper (GFX free fonts)
// ====================================================================
void txt(const GFXfont* f, uint16_t col, uint8_t datum, const String& s, int x, int y) {
  tft.setFreeFont(f);
  tft.setTextColor(col);
  tft.setTextDatum(datum);
  tft.drawString(s, x, y);
}

String fitText(const GFXfont* f, String value, int maxWidth) {
  tft.setFreeFont(f);
  if (tft.textWidth(value) <= maxWidth) return value;
  while (value.length() && tft.textWidth(value + "...") > maxWidth) value.remove(value.length() - 1);
  return value + "...";
}
// Compact system typography: 9 pt body, 9 pt labels, 12 pt section headings,
// and 18 pt emphasis. The seven-segment clock remains purposefully large.
#define F12B (&FreeSansBold9pt7b)
#define F18B (&FreeSansBold12pt7b)
#define F24B (&FreeSansBold18pt7b)
#define F12  (&FreeSans9pt7b)

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
const uint8_t MAX_ALARMS = 4;
AlarmState alarms[MAX_ALARMS];
uint8_t alarmCount = 0;
AlarmState alarmDraft = {false, 7, 0, 2026, 1, 1, 0};
AlarmState clockDraft = {true, 12, 0, 2026, 1, 1, 0};
int editingAlarmIndex = -1;
int ringingAlarmIndex = -1;
bool alarmListMode = true;
bool alarmRinging = false;
uint32_t lastAlarmMinuteKey = 0;

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
  } else {
    // IP geolocation can resolve to a distant ISP gateway (e.g. Phoenixville
    // or Phoenix). Keep the documented default anchored to Philadelphia.
    lat = WEATHER_LAT; lon = WEATHER_LON; city = "Philadelphia";
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
  // Draw a built-in, high-contrast glyph first so weather stays visible even
  // when the SD card has no downloaded PNGs or PNG decoding fails.
  const uint16_t cloud = C(226, 232, 244);
  const uint16_t sun = C(255, 196, 64);
  bool rain = (code >= 51 && code <= 82) || (code >= 95);
  bool snow = code >= 71 && code <= 77;
  bool fog = code == 45 || code == 48;
  bool cloudy = code >= 2 || code == 1 || code == -1;
  if (!cloudy && code == 0) {
    tft.fillCircle(x + 48, y + 42, 24, sun);
    for (int a = 0; a < 8; a++) {
      float ang = a * 0.785398f;
      tft.drawLine(x + 48 + cosf(ang) * 31, y + 42 + sinf(ang) * 31,
                   x + 48 + cosf(ang) * 39, y + 42 + sinf(ang) * 39, sun);
    }
    return;
  }
  if (code <= 3 && code >= 1) tft.fillCircle(x + 69, y + 30, 17, sun);
  tft.fillCircle(x + 39, y + 47, 18, cloud);
  tft.fillCircle(x + 58, y + 37, 23, cloud);
  tft.fillCircle(x + 78, y + 48, 17, cloud);
  tft.fillRoundRect(x + 25, y + 45, 68, 23, 11, cloud);
  if (rain && !snow) {
    for (int i = 0; i < 3; i++) tft.drawLine(x + 38 + i * 21, y + 73, x + 32 + i * 21, y + 86, C(82, 178, 255));
    if (code >= 95) tft.drawLine(x + 60, y + 70, x + 49, y + 88, sun);
  } else if (snow) {
    for (int i = 0; i < 3; i++) {
      int sx = x + 39 + i * 21, sy = y + 79;
      tft.drawLine(sx - 4, sy, sx + 4, sy, COL_WHITE);
      tft.drawLine(sx, sy - 4, sx, sy + 4, COL_WHITE);
    }
  } else if (fog) {
    tft.drawFastHLine(x + 27, y + 75, 64, C(176, 191, 210));
    tft.drawFastHLine(x + 34, y + 83, 50, C(176, 191, 210));
  }
}

void drawWeatherIcon(int x, int y) {
  char path[96];
  snprintf(path, sizeof(path), "%s/%s.png", ICON_DIR, weather.icon.c_str());
  drawWeatherFallback(x, y, weather.code);
  // Overlay the installed illustration when available; the built-in glyph
  // underneath remains a readable fallback for transparent/corrupt PNGs.
  drawPng(path, x, y);
}

// ====================================================================
//  uiverse key styles
// ====================================================================
// Flat, soft-radius controls inspired by Apple's system surfaces.
void drawKey(int x, int y, int w, int h, const char* label, bool pressed = false, uint16_t face = 0) {
  if (!face) face = UI_KEY;
  uint16_t surface = pressed ? C(62, 68, 82) : face;
  tft.fillRoundRect(x, y, w, h, min(h / 2, 10), surface);
  txt(F12B, UI_TEXT, MC_DATUM, label, x + w / 2, y + h / 2);
}

// iOS-style dark keyboard caps: quiet graphite keys with a restrained blue press state.
void drawKbKey(int x, int y, int w, int h, const char* label, bool pressed = false) {
  uint16_t face = pressed ? C(10, 132, 255) : C(55, 59, 69);
  tft.fillRoundRect(x, y + 1, w, h, 6, C(12, 14, 19));
  tft.fillRoundRect(x, y, w, h - 1, 6, face);
  txt(F12B, 0xFFFF, MC_DATUM, label, x + w / 2, y + h / 2);
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

void drawWifiSignal(int cx, int by, int level, uint16_t bg, uint16_t lit, uint16_t dim, bool connecting = false) {
  const int R[3] = {6, 11, 16};
  tft.fillRect(cx - 20, by - 22, 40, 28, bg);
  if (connecting) {
    int frame = (millis() / 180UL) % 4;
    for (int i = 0; i < 3; i++) {
      bool active = i <= frame % 3;
      arcLovy(tft, cx, by, R[i], 3, 225.0f + frame * 18.0f, 90.0f, active ? lit : dim, bg);
    }
    tft.fillCircle(cx, by - 1, 3, lit);
    return;
  }
  for (int i = 0; i < 3; i++)
    arcLovy(tft, cx, by, R[i], 3, 225.0f, 90.0f, (level > i) ? lit : dim, bg);
  tft.fillCircle(cx, by - 1, 3, level > 0 ? lit : dim);
}

void drawBattery(int x, int y, int pct, uint16_t bg, uint16_t line) {
  tft.fillRect(x - 2, y - 2, 38, 18, bg);
  tft.fillRoundRect(x, y, 30, 13, 3, line);
  tft.fillRoundRect(x + 2, y + 2, 26, 9, 2, bg);
  tft.fillRoundRect(x + 29, y + 4, 3, 5, 1, line);
  uint16_t col = pct <= 20 ? UI_RED : pct <= 45 ? UI_YEL : UI_GREEN;
  int w = max(1, pct * 22 / 100);
  tft.fillRect(x + 4, y + 3, w, 7, col);
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
  DynamicJsonDocument doc(2048);
  doc["schema"] = 2;
  JsonArray values = doc.createNestedArray("alarms");
  for (int i = 0; i < alarmCount; i++) {
    JsonObject item = values.createNestedObject();
    item["enabled"] = alarms[i].enabled; item["hour"] = alarms[i].hour; item["minute"] = alarms[i].minute;
    item["year"] = alarms[i].year; item["month"] = alarms[i].month; item["day"] = alarms[i].day;
    item["repeatMask"] = alarms[i].repeatMask;
  }
  serializeJson(doc, f); f.close();
}

bool loadAlarm() {
  alarmCount = 0;
  if (!sdOk) return false;
  File f = SD.open(ALARM_FILE);
  if (!f) return false;
  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return false; }
  f.close();
  JsonArray values = doc["alarms"].as<JsonArray>();
  if (!values.isNull()) {
    for (JsonObject item : values) {
      if (alarmCount >= MAX_ALARMS) break;
      AlarmState& a = alarms[alarmCount++];
      a.enabled = item["enabled"] | false; a.hour = constrain((int)(item["hour"] | 7), 0, 23);
      a.minute = constrain((int)(item["minute"] | 0), 0, 59); a.year = constrain((int)(item["year"] | 2026), 2024, 2099);
      a.month = constrain((int)(item["month"] | 1), 1, 12); a.day = constrain((int)(item["day"] | 1), 1, 31);
      a.repeatMask = (item["repeatMask"] | 0) & 0x7F;
    }
  } else if (doc.containsKey("hour")) {
    // Migrate the original single-alarm payload without discarding it.
    AlarmState& a = alarms[alarmCount++];
    a.enabled = doc["enabled"] | false; a.hour = constrain((int)(doc["hour"] | 7), 0, 23);
    a.minute = constrain((int)(doc["minute"] | 0), 0, 59); a.year = constrain((int)(doc["year"] | 2026), 2024, 2099);
    a.month = constrain((int)(doc["month"] | 1), 1, 12); a.day = constrain((int)(doc["day"] | 1), 1, 31);
    a.repeatMask = (doc["repeatMask"] | 0) & 0x7F;
  }
  return alarmCount > 0;
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
  doc["appearanceTheme"] = appearanceTheme;
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
  String savedTheme = (const char*)(doc["appearanceTheme"] | "midnight");
  if (savedTheme.length()) appearanceTheme = savedTheme;
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
  char b[12];
  if (settings.use24Hour) snprintf(b, sizeof(b), "%02u:%02u", a.hour, a.minute);
  else { int h = a.hour % 12; if (!h) h = 12; snprintf(b, sizeof(b), "%d:%02u %s", h, a.minute, a.hour < 12 ? "AM" : "PM"); }
  return String(b);
}

String alarmRepeatText(const AlarmState& a) {
  if (!a.repeatMask) { char b[20]; snprintf(b, sizeof(b), "%02u/%02u/%04u", a.month, a.day, a.year); return String(b); }
  const char* names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  String text;
  for (int i = 0; i < 7; i++) if (a.repeatMask & (1 << i)) { if (text.length()) text += " "; text += names[i]; }
  return text;
}

bool alarmDueNow() {
  struct tm ti;
  if (!getLocalTime(&ti, 0)) return false;
  uint32_t minuteKey = ((uint32_t)(ti.tm_year + 1900) * 1000000UL) + ((uint32_t)ti.tm_yday * 1440UL) + ti.tm_hour * 60UL + ti.tm_min;
  if (minuteKey == lastAlarmMinuteKey) return false;
  for (int i = 0; i < alarmCount; i++) {
    AlarmState& a = alarms[i];
    if (!a.enabled || ti.tm_hour != a.hour || ti.tm_min != a.minute) continue;
    bool due = a.repeatMask ? ((a.repeatMask & (1 << ti.tm_wday)) != 0)
                            : (ti.tm_year + 1900 == a.year && ti.tm_mon + 1 == a.month && ti.tm_mday == a.day);
    if (due) { ringingAlarmIndex = i; lastAlarmMinuteKey = minuteKey; return true; }
  }
  return false;
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
  tft.fillScreen(UI_BACK);
  txt(F12B, UI_TEXT, TC_DATUM, msg, 240, 235);
}

bool tryConnect(uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(selSsid.c_str(), password.c_str());
  loaderScreen("connecting");
  drawWifiSignal(240, 152, 0, UI_BACK, UI_TEXT, C(70, 75, 88), true);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
    // Animate only the Wi-Fi glyph while connecting; no full-page redraw.
    drawWifiSignal(240, 152, 0, UI_BACK, UI_TEXT, C(70, 75, 88), true);
    delay(80);
  }
  return WiFi.status() == WL_CONNECTED;
}

// ====================================================================
//  screens
// ====================================================================
void showWelcome() {
  screen = S_WELCOME;
  tft.fillScreen(UI_BACK);
  tft.fillRoundRect(30, 26, 420, 172, 24, UI_KEY);
  // A quiet nightstand/StandBy-style clock mark: simple white dial and blue hands.
  tft.drawCircle(240, 64, 19, C(205, 210, 220));
  tft.drawLine(240, 64, 240, 51, C(10, 132, 255));
  tft.drawLine(240, 64, 250, 69, C(10, 132, 255));
  tft.fillCircle(240, 64, 2, C(10, 132, 255));
  txt(F24B, UI_TEXT, TC_DATUM, "ClockOS", 240, 108);
  txt(F12B, C(190, 194, 202), TC_DATUM, "Time, weather, and your day at a glance", 240, 142);
  txt(F12, C(160, 164, 172), TC_DATUM, "Choose how to get started", 240, 172);
  drawAppleButton(36, 222, 196, 44, "Set up Wi-Fi", true);
  drawAppleButton(248, 222, 196, 44, "Set up offline");
  txt(F12, C(142, 146, 154), TC_DATUM, "You can change these choices later in Settings.", 240, 294);
}

void showClassroomSetup() {
  screen = S_SETTINGS;
  firstSetupClassroom = true;
  tft.fillScreen(UI_BACK);
  txt(F18B, UI_TEXT, TC_DATUM, "Google Classroom", 240, 42);
  txt(F12, UI_TEXT, TC_DATUM, "Show assignments and calendar reminders?", 240, 92);
  alarmButton(28, 144, 190, "Enable", settings.classroomEnabled);
  alarmButton(262, 144, 190, "Not now", !settings.classroomEnabled);
  txt(F12, UI_MUTED, TC_DATUM, "You can change this in Settings.", 240, 230);
}

void showSyncSetup() {
  screen = S_SETTINGS;
  firstSetupClassroom = false; firstSetupSync = true;
  tft.fillScreen(UI_BACK);
  txt(F18B, UI_TEXT, TC_DATUM, "Sync displays", 240, 42);
  txt(F12, UI_TEXT, TC_DATUM, "Share time, alarms, and settings?", 240, 92);
  alarmButton(20, 132, 140, "Off", !settings.syncEnabled);
  alarmButton(170, 132, 140, "Main device", settings.syncEnabled && settings.syncMain);
  alarmButton(320, 132, 140, "Side device", settings.syncEnabled && !settings.syncMain);
  txt(F12, UI_MUTED, TC_DATUM, "Main sends time and alarms; Side receives them.", 240, 220);
  txt(F12, syncPeerVersion.length() && !syncVersionCompatible(syncPeerVersion) ? UI_RED : UI_TEXT,
      TC_DATUM, syncCompatibilityText(), 240, 264);
}

void finishFirstSetup() {
  firstSetupClassroom = false;
  showSyncSetup();
}

void scanNetworks() {
  nNets = 0; scanStatus = "";
  tft.fillScreen(UI_BACK);
  txt(F12B, UI_TEXT, TC_DATUM, "Scanning for Wi-Fi networks...", 240, 160);
  WiFi.mode(WIFI_STA);
  WiFi.scanDelete();
  int n = WiFi.scanNetworks(false, true);
  if (n < 0) {
    scanStatus = "Scan failed. Tap Rescan to try again.";
    return;
  }
  for (int i = 0; i < n && nNets < 5; i++) {
    String name = WiFi.SSID(i);
    if (!name.length()) continue; // hidden SSIDs cannot be selected from a scan result
    bool dup = false;
    for (int j = 0; j < nNets; j++) if (ssids[j] == name) dup = true;
    if (dup) continue;
    ssids[nNets] = name; rssis[nNets] = WiFi.RSSI(i); nNets++;
  }
  WiFi.scanDelete();
  if (!nNets) scanStatus = "No visible networks found. Check router range, then rescan.";
}

void showScan() {
  screen = S_SCAN;
  scanNetworks();
  tft.fillScreen(UI_BACK);
  drawKey(6, 4, 72, 30, "Back", false, UI_KEY);
  txt(F12B, UI_TEXT, TC_DATUM, "Choose your Wi-Fi", 270, 8);
  if (nNets) {
    for (int i = 0; i < nNets; i++) {
      String name = ssids[i]; if (name.length() > 26) name = name.substring(0, 26);
      drawKey(10, 42 + i * 46, 460, 38, name.c_str());
    }
    txt(F12, UI_TEXT, TC_DATUM, String(nNets) + (nNets == 1 ? " network found" : " networks found"), 240, 268);
  } else {
    txt(F12B, UI_TEXT, TC_DATUM, scanStatus, 240, 126);
  }
  drawKey(10, 280, 460, 30, "Rescan", false, UI_KEY);
}

void openWifiScanner(uint8_t returnScreen) {
  scanReturnScreen = (Screen)returnScreen;
  showScan();
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
  tft.fillRoundRect(60, 44, 330, 38, 8, UI_KEY);
  String shown;
  if (editingCity || showPass) shown = password; else for (size_t i = 0; i < password.length(); i++) shown += '*';
  if (shown.length() > 20) shown = shown.substring(shown.length() - 20);
  txt(F12B, UI_TEXT, ML_DATUM, shown, 70, 63);
}

void showKeyboard() {
  screen = S_KEYS;
  tft.fillScreen(UI_BACK);

  // Calm dark keyboard tray with uniform rounded caps.
  tft.fillRoundRect(0, 108, 480, 212, 16, UI_KEY);

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
  txt(F12B, UI_TEXT, TC_DATUM, t, 255, 12);
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
  } else strcpy(buf, "--:--");
  drawBigTime(buf, 240, 22);
  if (ok && !settings.use24Hour) {
    txt(F18B, COL_WHITE, MC_DATUM, ti.tm_hour < 12 ? "AM" : "PM", 120, 138);
  }
  if (ok) {
    char date[16]; snprintf(date, sizeof(date), "%02d/%02d/%04d", ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900);
    txt(F12B, COL_DIM, MC_DATUM, date, 120, 180);
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

bool sdDirectoryExists(const char* path) {
  File f = SD.open(path);
  bool ok = f && f.isDirectory();
  if (f) f.close();
  return ok;
}

// Keep this relative-path set in sync with CLOCKOS_SD_DIRECTORIES in install.py.
// Both setup paths create folders only; neither repartitions nor formats/wipes the card.
const char* const CLOCKOS_SD_DIRECTORIES[] = {
  "/.source",
  "/.source/compiled",
  "/.source/compiled/updates",
  "/.source/compiled/bootloader",
  "/.source/compiled/bootloader/fallback",
  "/.source/uncompiled",
  "/.source/uncompiled/updates",
  "/.source/uncompiled/bootloader",
  "/.source/uncompiled/bootloader/fallback",
  "/.source/data",
  "/.source/data/preferences",
  "/.source/data/secrets",
  ICON_DIR,
  "/.source/themes",
  THEMES_DIR,
  DATA_DIR,
  PREFERENCES_DIR,
  "/data/secrets"
};
const size_t CLOCKOS_SD_DIRECTORY_COUNT = sizeof(CLOCKOS_SD_DIRECTORIES) / sizeof(CLOCKOS_SD_DIRECTORIES[0]);

bool ensureClockOsSdLayout() {
  if (!sdPresent) return false;
  // A freshly FAT-formatted card is valid. Create the same layout as Windows setup.
  bool ok = true;
  for (size_t i = 0; i < CLOCKOS_SD_DIRECTORY_COUNT; i++) {
    const char* dir = CLOCKOS_SD_DIRECTORIES[i];
    if (!SD.exists(dir) && !SD.mkdir(dir) && !SD.exists(dir)) ok = false;
  }
  return ok;
}

bool clockOsSdReady() {
  if (!sdPresent) return false;
  for (size_t i = 0; i < CLOCKOS_SD_DIRECTORY_COUNT; i++) {
    if (!sdDirectoryExists(CLOCKOS_SD_DIRECTORIES[i])) return false;
  }
  return true;
}

void drawStatusBlock() {
  tft.fillRect(0, 226, 240, 94, 0x0000);
  int lvl = (WiFi.status() == WL_CONNECTED) ? rssiLevel(WiFi.RSSI()) : 0;
  bool connecting = WiFi.status() == WL_IDLE_STATUS;
  drawWifiSignal(26, 294, lvl, 0x0000, COL_WHITE, 0x39E7, connecting);
  drawSdCardIcon(70, 294, sdPresent, false);
  int pct = readBatteryPct();
  drawBattery(102, 286, pct, 0x0000, COL_WHITE);
  if (settings.showBatteryPercent) {
    char b[8]; snprintf(b, sizeof(b), "%d%%", pct);
    txt(F12, COL_DIM, ML_DATUM, b, 142, 292);
  }
  tft.fillRoundRect(171, 276, 58, 34, 17, C(42, 45, 54));
  txt(F12B, COL_WHITE, MC_DATUM, "Settings", 200, 293);
}

void refreshSdState() {
  static uint32_t lastCheck = 0;
  static bool previousOk = false, previousPresent = false;
  if (millis() - lastCheck < 3000) return;
  lastCheck = millis();
  bool present = SD.begin(SD_PIN_CS);
  sdPresent = present;
  if (present) ensureClockOsSdLayout();
  bool valid = present && clockOsSdReady();
  bool changed = present != previousPresent || valid != previousOk;
  sdOk = valid;
  previousPresent = present; previousOk = valid;
  if (changed && !screenSleeping) {
    if (screen == S_HOME) drawStatusBlock();
    else if (screen == S_SETTINGS) showSettingsPage();
    else if (screen == S_WIFI) showWifiPage();
    else if (screen == S_WEATHER) showWeatherPage();
    else if (screen == S_SDCARD) showSdCardPage();
  }
}

void drawRightPanel() {
  tft.fillRect(241, 0, 239, 320, 0x0000);
  // Compact weather module: icon, temperature, then place; never a long condition label.
  drawWeatherIcon(312, 2);
  String temperature = weather.valid ? (String((int)roundf(weather.temperatureF)) + "°") : "--°";
  txt(F18B, COL_WHITE, TC_DATUM, temperature, 360, 112);
  txt(F12, COL_DIM, TC_DATUM, weather.valid ? weather.city : "Philadelphia", 360, 137);
  tft.drawFastHLine(255, 160, 210, 0x7BEF);
  txt(F12B, COL_WHITE, TL_DATUM, "Calendar", 258, 176);
  if (settings.calendarEnabled && settings.notifications && classroomAssignmentCount > 0) {
    String title = fitText(F12, nextAssignment, 196);
    txt(F12, COL_WHITE, TL_DATUM, title, 258, 214);
    txt(F12, COL_DIM, TL_DATUM, fitText(F12, nextAssignmentDue, 196), 258, 238);
  } else {
    txt(F12, COL_DIM, TL_DATUM, "No upcoming assignments", 258, 214);
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
  tft.fillRoundRect(x, y, w, 30, 9, selected ? UI_BLUE : UI_KEY);
  tft.drawRoundRect(x, y, w, 30, 9, selected ? UI_BLUE : C(82, 88, 104));
  txt(F12B, UI_TEXT, MC_DATUM, label, x + w / 2, y + 15);
}

void drawAlarmList() {
  screen = S_ALARM; alarmListMode = true;
  drawBackHeader("Alarms");
  txt(F12, C(92, 96, 106), TC_DATUM, "Tap an alarm to edit. Use the switch to enable it.", 240, 52);
  for (int i = 0; i < alarmCount; i++) {
    int y = 68 + i * 46;
    tft.fillRoundRect(16, y, 448, 40, 12, UI_KEY);
    txt(F18B, UI_TEXT, ML_DATUM, alarmTimeText(alarms[i]), 32, y + 20);
    txt(F12, UI_MUTED, ML_DATUM, fitText(F12, alarmRepeatText(alarms[i]), 210), 184, y + 20);
    tft.fillRoundRect(396, y + 8, 50, 24, 12, alarms[i].enabled ? C(52, 199, 89) : C(192, 196, 204));
    tft.fillCircle(alarms[i].enabled ? 434 : 408, y + 20, 9, 0xFFFF);
  }
  if (alarmCount < MAX_ALARMS) drawAppleButton(16, 264, 220, 36, "+ Add alarm", true);
  drawAppleButton(244, 264, 220, 36, "Done");
}

void beginAlarmEdit(int index) {
  editingAlarmIndex = index;
  if (index >= 0 && index < alarmCount) alarmDraft = alarms[index];
  else {
    struct tm ti;
    alarmDraft = {true, 7, 0, 2026, 1, 1, 0};
    if (getLocalTime(&ti, 0)) {
      alarmDraft.year = ti.tm_year + 1900; alarmDraft.month = ti.tm_mon + 1; alarmDraft.day = ti.tm_mday;
      alarmDraft.hour = (ti.tm_hour + 1) % 24; alarmDraft.minute = 0;
    }
  }
  alarmListMode = false; drawAlarmEditor();
}

void drawAlarmEditor() {
  screen = S_ALARM; alarmListMode = false;
  drawBackHeader(editingAlarmIndex >= 0 ? "Edit Alarm" : "New Alarm");
  txt(F24B, UI_TEXT, TC_DATUM, alarmTimeText(alarmDraft), 240, 58);
  txt(F12, UI_MUTED, TC_DATUM, settings.use24Hour ? "24-hour time" : "12-hour time", 240, 80);
  alarmButton(18, 92, 78, "Hour -"); alarmButton(106, 92, 78, "Hour +");
  alarmButton(296, 92, 78, "Min -"); alarmButton(384, 92, 78, "Min +");
  char date[20]; snprintf(date, sizeof(date), "%02u/%02u/%04u", alarmDraft.month, alarmDraft.day, alarmDraft.year);
  txt(F18B, UI_TEXT, TC_DATUM, date, 240, 136);
  alarmButton(8, 151, 72, "Day -"); alarmButton(86, 151, 72, "Day +");
  alarmButton(164, 151, 78, "Month -"); alarmButton(248, 151, 78, "Month +");
  alarmButton(332, 151, 68, "Year -"); alarmButton(406, 151, 68, "Year +");
  txt(F12, C(185, 190, 202), ML_DATUM, "Repeat", 12, 206);
  const char* names[] = {"S","M","T","W","T","F","S"};
  for (int i = 0; i < 7; i++) alarmButton(68 + i * 58, 188, 50, names[i], alarmDraft.repeatMask & (1 << i));
  drawAppleButton(12, 244, 145, 38, alarmDraft.enabled ? "Enabled" : "Disabled", alarmDraft.enabled);
  drawAppleButton(167, 244, 145, 38, "Save", true);
  drawAppleButton(322, 244, 145, 38, "Cancel");
}

void showAlarmEditor() {
  drawAlarmList();
}

void drawClockEditor() {
  tft.fillScreen(UI_BACK);
  txt(F12B, UI_TEXT, TC_DATUM, "Set date and time", 240, 18);
  txt(F24B, UI_TEXT, TC_DATUM, alarmTimeText(clockDraft), 240, 55);
  alarmButton(18, 82, 78, "Hour -"); alarmButton(106, 82, 78, "Hour +");
  alarmButton(296, 82, 78, "Min -"); alarmButton(384, 82, 78, "Min +");
  char date[20]; snprintf(date, sizeof(date), "%02u/%02u/%04u", clockDraft.day, clockDraft.month, clockDraft.year);
  txt(F18B, UI_TEXT, TC_DATUM, date, 240, 128);
  alarmButton(8, 148, 72, "Day -"); alarmButton(86, 148, 72, "Day +");
  alarmButton(164, 148, 78, "Month -"); alarmButton(248, 148, 78, "Month +");
  alarmButton(332, 148, 68, "Year -"); alarmButton(406, 148, 68, "Year +");
  alarmButton(145, 244, 90, "Save", true); alarmButton(250, 244, 100, "Cancel");
  txt(F12, UI_MUTED, TC_DATUM, "Offline mode asks for this after every restart", 240, 292);
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
  AlarmState active = (ringingAlarmIndex >= 0 && ringingAlarmIndex < alarmCount) ? alarms[ringingAlarmIndex] : alarmDraft;
  tft.fillScreen(UI_BACK);
  tft.fillRoundRect(44, 34, 392, 224, 24, UI_KEY);
  drawAlarmIcon(240, 78, UI_RED);
  txt(F24B, UI_TEXT, TC_DATUM, "Alarm", 240, 132);
  txt(F12B, C(174, 178, 186), TC_DATUM, alarmTimeText(active), 240, 174);
  drawAppleButton(150, 218, 180, 38, "Dismiss", true);
}

void dismissAlarm() {
  alarmRinging = false;
  digitalWrite(LED_G, HIGH);
  if (ringingAlarmIndex >= 0 && ringingAlarmIndex < alarmCount && !alarms[ringingAlarmIndex].repeatMask) {
    alarms[ringingAlarmIndex].enabled = false; saveAlarm();
  }
  ringingAlarmIndex = -1;
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
  tft.fillRoundRect(34, 236, 412, 60, 14, UI_KEY);
  drawSdCardIcon(64, 266, false, sdPresent);
  txt(F12B, UI_TEXT, ML_DATUM, sdPresent ? "SD card is not ClockOS-ready" : "SD card not inserted", 94, 254);
  txt(F12, UI_MUTED, ML_DATUM, "Changes will not be saved", 94, 276);
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
  // Same directory list as Windows setup; preserve unrelated files and contents.
  bool ok = ensureClockOsSdLayout();
  if (!ok) { sdOk = false; sdCardMessage = "Could not prepare this SD card."; return false; }
  sdOk = clockOsSdReady();
  if (!sdOk) { sdCardMessage = "SD card layout is incomplete; try Prepare again."; return false; }
  saveSettings(); saveAlarm(); saveCreds(); saveCal();
  sdOk = clockOsSdReady();
  if (!sdOk) { sdCardMessage = "Could not verify all ClockOS folders."; return false; }
  sdCardMessage = "ClockOS storage is ready. Current settings saved.";
  return true;
}

void showSdCardPage() {
  screen = S_SDCARD; drawBackHeader("Storage");
  settingsGroupRow(54, "ClockOS storage", sdOk ? "Ready" : (sdPresent ? "Needs setup" : "No card"), sdOk);
  String capacity = "Insert an SD card to prepare ClockOS storage";
  if (sdPresent) {
    uint64_t total = SD.cardSize() / (1024ULL * 1024ULL);
    uint64_t used = SD.usedBytes() / (1024ULL * 1024ULL);
    capacity = String(used) + " MB used of " + String(total) + " MB";
  }
  settingsGroupRow(100, "Capacity", capacity);
  settingsGroupRow(146, "Prepare ClockOS storage", "Creates required folders", true);
  settingsGroupRow(192, "Browse files", "Read-only viewer");
  txt(F12, UI_MUTED, TC_DATUM, "Preparation keeps unrelated files and works without Wi-Fi.", 240, 254);
  if (sdCardMessage.length()) txt(F12, sdOk ? UI_GREEN : UI_RED, TC_DATUM, fitText(F12, sdCardMessage, 420), 240, 282);
}

void showSdFilesPage() {
  screen = S_SDFILES; drawBackHeader("SD Card Files");
  if (!sdPresent) {
    txt(F12B, UI_RED, TC_DATUM, "No SD card detected", 240, 96);
    return;
  }
  txt(F12, UI_MUTED, TC_DATUM, "Read-only viewer", 240, 58);
  File root = SD.open("/");
  int row = 0;
  while (root && row < 9) {
    File entry = root.openNextFile();
    if (!entry) break;
    String name = String(entry.name());
    if (name.length() > 30) name = name.substring(name.length() - 30);
    String line = entry.isDirectory() ? "[DIR] " + name : name + "  " + String((unsigned long)entry.size()) + " B";
    txt(F12, UI_TEXT, ML_DATUM, fitText(F12, line, 420), 24, 88 + row * 22);
    entry.close(); row++;
  }
  if (root) root.close();
  if (!row) txt(F12, UI_MUTED, TC_DATUM, "No readable files", 240, 110);
}

void showProfilePage() {
  screen = S_PROFILE; drawBackHeader("Profile");
  tft.fillRoundRect(34, 50, 412, 190, 22, UI_KEY);
  drawProfileIcon(105, 116, C(120, 125, 138));
  txt(F18B, UI_TEXT, ML_DATUM, profileName, 145, 104);
  txt(F12, UI_MUTED, ML_DATUM, settings.classroomEnabled ? "Google Classroom sync enabled" : "Classroom sync disabled", 145, 132);
  alarmButton(82, 176, 316, "Sign in with Clock Setup", true);
  txt(F12, UI_MUTED, TC_DATUM, "Use the Windows setup app to authorize Google.", 240, 270);
}

void showAppearancePage() {
  screen = S_APPEARANCE; drawBackHeader("Appearance");
  txt(F12, UI_MUTED, TC_DATUM, "Choose a ClockOS theme", 240, 58);
  const char* names[] = {"Crystal", "Midnight", "Ocean", "Sunrise", "Graphite"};
  const char* ids[] = {"crystal", "midnight", "ocean", "sunrise", "graphite"};
  for (int i = 0; i < 5; i++) {
    bool selected = appearanceTheme == ids[i];
    settingsRow(14 + (i % 2) * 242, 82 + (i / 2) * 44, names[i], selected ? "Selected" : ">", selected);
  }
  txt(F12, UI_MUTED, TC_DATUM, "More themes can be added to /themes/appearance.", 240, 266);
}

void showDeveloperPinPage() {
  screen = S_DEV_PIN; developerPinInput = ""; drawBackHeader("Developer Mode");
  txt(F18B, UI_TEXT, TC_DATUM, "Enter developer PIN", 240, 62);
  txt(F18B, C(0, 122, 255), TC_DATUM, "****", 240, 92);
  const char* keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "Clear", "0", "Enter"};
  for (int i = 0; i < 12; i++) {
    int col = i % 3, row = i / 3;
    alarmButton(45 + col * 145, 112 + row * 42, 125, keys[i], i == 11);
  }
}

void showDeveloperModePage() {
  screen = S_DEV_MODE; drawBackHeader("Developer Mode");
  tft.fillRoundRect(28, 48, 424, 48, 14, UI_KEY);
  txt(F12B, UI_GREEN, TC_DATUM, "Developer mode enabled", 240, 72);
  settingsRow(14, 110, "USB debugging", "Enabled", true);
  settingsRow(256, 110, "ClockOS version", CLOCKOS_VERSION, false);
  alarmButton(14, 166, 210, "Revert software");
  alarmButton(256, 166, 210, "Recovery tools");
  txt(F12, UI_MUTED, TC_DATUM, "Developer features can affect stability.", 240, 238);
  txt(F12, UI_MUTED, TC_DATUM, "USB Serial logging is enabled while this page is open.", 240, 260);
  Serial.setDebugOutput(true);
}

void drawBackHeader(const String& title, bool showGlobal) {
  tft.fillScreen(UI_BACK);
  alarmButton(8, 8, 78, "Back");
  txt(F18B, UI_TEXT, TC_DATUM, title, 240, 24);
  if (showGlobal) {
    drawProfileIcon(405, 24, C(190, 195, 207));
    drawSdCardIcon(438, 24, sdOk, sdPresent && !sdOk);
  }
}

void settingsRow(int x, int y, const String& label, const String& value, bool accent) {
  uint16_t blue = C(0, 122, 255);
  tft.fillRoundRect(x, y, 210, 34, 10, UI_KEY);
  txt(F12, UI_TEXT, ML_DATUM, fitText(F12, label, 125), x + 12, y + 17);
  if (value.length()) txt(F12, accent ? blue : UI_MUTED, MR_DATUM, fitText(F12, value, 64), x + 198, y + 17);
}

void settingsGroupRow(int y, const String& label, const String& value, bool accent) {
  uint16_t blue = C(0, 122, 255);
  tft.fillRoundRect(16, y, 448, 38, 11, UI_KEY);
  txt(F12B, UI_TEXT, ML_DATUM, fitText(F12B, label, 245), 30, y + 19);
  if (value.length()) txt(F12, accent ? blue : UI_MUTED, MR_DATUM, fitText(F12, value, 160), 447, y + 19);
}

void drawSettingsToggle(int x, int y, bool enabled) {
  uint16_t track = enabled ? C(48, 209, 88) : C(105, 107, 114);
  tft.fillRoundRect(x, y, 46, 28, 14, enabled ? track : UI_DISABLED);
  tft.fillCircle(enabled ? x + 32 : x + 14, y + 14, 11, 0xFFFF);
}

void drawSettingsCard(int y, int rows, int rowHeight) {
  int h = rows * rowHeight;
  tft.fillRoundRect(12, y, 456, h, 13, UI_KEY);
  for (int i = 1; i < rows; i++) tft.drawFastHLine(64, y + i * rowHeight, 390, UI_SEPARATOR);
}

void drawSettingsOption(int y, const String& title, const String& detail, bool enabled, bool toggle, bool disclosure = true) {
  txt(F12B, UI_TEXT, ML_DATUM, fitText(F12B, title, toggle ? 280 : 340), 26, y + 17);
  if (detail.length()) txt(F12, UI_MUTED, MR_DATUM, fitText(F12, detail, 118), 418, y + 17);
  if (toggle) drawSettingsToggle(408, y + 9, enabled);
  else if (disclosure) {
    tft.drawLine(445, y + 17, 451, y + 23, UI_BLUE);
    tft.drawLine(451, y + 23, 445, y + 29, UI_BLUE);
  }
}

void drawSettingsCategoryIcon(int x, int y, int index) {
  uint16_t colors[] = {C(142, 145, 150), C(10, 132, 255), C(175, 82, 222),
                       C(255, 59, 48), C(48, 209, 88), C(255, 149, 0)};
  uint16_t white = 0xFFFF;
  tft.fillRoundRect(x, y, 26, 26, 6, colors[index]);
  if (index == 0) { tft.drawCircle(x + 13, y + 13, 7, white); tft.drawCircle(x + 13, y + 13, 2, white); }
  else if (index == 1) {
    tft.drawArc(x + 13, y + 20, 11, 7, 205, 335, white, colors[index], true);
    tft.drawArc(x + 13, y + 20, 6, 3, 215, 325, white, colors[index], true); tft.fillCircle(x + 13, y + 20, 2, white);
  } else if (index == 2) { tft.drawCircle(x + 10, y + 13, 6, white); tft.drawCircle(x + 16, y + 13, 6, white); }
  else if (index == 3) { tft.drawRoundRect(x + 5, y + 6, 16, 15, 2, white); tft.drawFastHLine(x + 6, y + 11, 14, white); tft.drawFastVLine(x + 9, y + 4, 5, white); tft.drawFastVLine(x + 17, y + 4, 5, white); }
  else if (index == 4) { tft.drawRoundRect(x + 6, y + 5, 14, 17, 2, white); tft.drawFastHLine(x + 9, y + 10, 8, white); tft.drawFastHLine(x + 9, y + 14, 8, white); }
  else { tft.drawCircle(x + 13, y + 13, 9, white); tft.drawFastVLine(x + 13, y + 12, 6, white); tft.fillCircle(x + 13, y + 8, 1, white); }
}

void showSettingsPage() {
  screen = S_SETTINGS;
  tft.fillScreen(UI_BACK);
  const char* labels[] = {"General", "Wi-Fi", "Appearance", "Calendar", "Storage", "System"};
  const char* ids[] = {"crystal", "midnight", "ocean", "sunrise", "graphite"};
  if (settingsAtRoot) {
    txt(F18B, UI_TEXT, TC_DATUM, "Settings", 240, 24);
    drawSettingsCard(52, 6, 42);
    for (int i = 0; i < 6; i++) {
      int y = 52 + i * 42;
      drawSettingsCategoryIcon(26, y + 8, i);
      txt(F12B, UI_TEXT, ML_DATUM, labels[i], 64, y + 21);
      if (i == 0) txt(F12, UI_MUTED, MR_DATUM, settings.use24Hour ? "24-hour" : "12-hour", 420, y + 21);
      else if (i == 1) txt(F12, UI_MUTED, MR_DATUM, WiFi.status() == WL_CONNECTED ? fitText(F12, WiFi.SSID(), 118) : "Not Connected", 420, y + 21);
      else if (i == 2) txt(F12, UI_MUTED, MR_DATUM, fitText(F12, appearanceTheme, 118), 420, y + 21);
      else if (i == 3) txt(F12, UI_MUTED, MR_DATUM, settings.calendarEnabled ? "On" : "Off", 420, y + 21);
      else if (i == 4) txt(F12, sdOk ? UI_GREEN : UI_MUTED, MR_DATUM, sdOk ? "Ready" : (sdPresent ? "Needs Setup" : "No Card"), 420, y + 21);
      else txt(F12, UI_MUTED, MR_DATUM, CLOCKOS_VERSION, 420, y + 21);
      tft.drawLine(445, y + 15, 451, y + 21, UI_BLUE); tft.drawLine(451, y + 21, 445, y + 27, UI_BLUE);
    }
    drawAppleButton(12, 8, 72, 30, "Back", false);
    return;
  }
  drawAppleButton(12, 8, 82, 34, "‹ Back", false);
  txt(F18B, UI_TEXT, TC_DATUM, labels[settingsCategory], 240, 24);
  switch (settingsCategory) {
    case 0:
      drawSettingsCard(56, 3, 50);
      drawSettingsOption(56, "24-hour time", "", settings.use24Hour, true);
      drawSettingsOption(106, "Battery percentage", "", settings.showBatteryPercent, true);
      drawSettingsOption(156, "Appearance", "", false, false);
      break;
    case 1:
      drawSettingsCard(56, 3, 50);
      drawSettingsOption(56, "Network", WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "Not Connected", false, false, false);
      drawSettingsOption(106, "Choose Network", "", false, false);
      drawSettingsOption(156, "Saved Networks", savedNetworkCount ? String(savedNetworkCount) + " saved" : "None", false, false, false);
      break;
    case 2: {
      drawSettingsCard(56, 5, 46);
      const char* names[] = {"Crystal", "Midnight", "Ocean", "Sunrise", "Graphite"};
      for (int i = 0; i < 5; i++) {
        int y = 56 + i * 46;
        if (appearanceTheme == ids[i]) tft.fillRoundRect(12, y, 456, 46, 10, UI_SELECTION);
        txt(F12B, UI_TEXT, ML_DATUM, names[i], 28, y + 23);
        if (appearanceTheme == ids[i]) {
          txt(F12B, UI_BLUE, MR_DATUM, "✓", 444, y + 23);
        } else {
          tft.drawLine(450, y + 17, 456, y + 23, UI_BLUE); tft.drawLine(456, y + 23, 450, y + 29, UI_BLUE);
        }
      }
      break;
    }
    case 3:
      drawSettingsCard(56, 3, 50);
      drawSettingsOption(56, "Calendar", "", settings.calendarEnabled, true);
      drawSettingsOption(106, "Classroom", "", settings.classroomEnabled, true);
      drawSettingsOption(156, "Notifications", "", settings.notifications, true);
      break;
    case 4:
      drawSettingsCard(56, 3, 50);
      drawSettingsOption(56, "ClockOS Storage", sdOk ? "Ready" : (sdPresent ? "Needs Setup" : "No Card"), false, false, false);
      drawSettingsOption(106, "Prepare Storage", "", false, false);
      drawSettingsOption(156, "Browse Files", "", false, false);
      break;
    default:
      drawSettingsCard(56, 1, 50);
      drawSettingsOption(56, "ClockOS Version", CLOCKOS_VERSION, false, false, false);
      tft.fillRoundRect(12, 122, 456, 50, 13, UI_KEY);
      txt(F12B, UI_RED, ML_DATUM, "Factory Reset", 26, 147);
      tft.drawLine(445, 139, 451, 145, UI_RED); tft.drawLine(451, 145, 445, 151, UI_RED);
      txt(F12, UI_MUTED, TL_DATUM, "About and device information", 24, 197);
      break;
  }
}

void showFactoryResetPage() {
  screen = S_FACTORY_RESET; drawBackHeader("Factory Reset");
  tft.fillRoundRect(18, 58, 444, 104, 14, UI_KEY);
  txt(F18B, UI_RED, TC_DATUM, "Erase local ClockOS data", 240, 86);
  txt(F12, C(190, 194, 202), TC_DATUM, "Settings, saved Wi-Fi, alarms, Classroom token and cache", 240, 112);
  txt(F12, C(190, 194, 202), TC_DATUM, "will be removed. Firmware and unrelated SD files stay intact.", 240, 134);
  settingsGroupRow(184, "Reset readiness", sdOk ? "Local data available" : "No SD data to erase");
  drawAppleButton(76, 246, 328, 42, factoryResetArmed ? "Tap again to factory reset" : "Factory Reset", factoryResetArmed);
}

void drawAppleUpdateIcon(int cx, int cy, uint16_t blue) {
  tft.fillRoundRect(cx - 28, cy - 34, 56, 68, 12, blue);
  tft.fillRoundRect(cx - 18, cy - 24, 36, 48, 7, 0xFFFF);
  tft.fillRoundRect(cx - 10, cy - 6, 20, 12, 5, blue);
  tft.fillCircle(cx, cy + 17, 2, blue);
  tft.drawLine(cx + 8, cy - 30, cx + 18, cy - 40, blue);
  tft.drawLine(cx + 18, cy - 40, cx + 24, cy - 34, blue);
}

void drawAppleButton(int x, int y, int w, int h, const String& label, bool primary) {
  tft.fillRoundRect(x, y, w, h, h / 2, primary ? UI_ACTION : UI_KEY);
  txt(F12B, primary ? UI_ON_ACTION : UI_TEXT, MC_DATUM, label, x + w / 2, y + h / 2);
}

void drawUpdatePage(int pct, const String& status) {
  screen = S_UPDATE;
  uint16_t bg = C(17, 19, 24), card = C(32, 35, 43), blue = C(10, 132, 255);
  uint16_t primary = C(242, 244, 248), secondary = C(165, 171, 184);
  tft.fillScreen(bg);
  tft.fillRoundRect(54, 34, 372, 252, 18, card);
  drawAppleUpdateIcon(240, 73, blue);
  txt(F18B, primary, TC_DATUM, "Software Update", 240, 122);
  txt(F12, secondary, TC_DATUM, String(CLOCKOS_NAME) + " " + CLOCKOS_VERSION, 240, 145);
  String line = status;
  if (line.length() > 52) line = line.substring(0, 52);
  txt(F12, primary, TC_DATUM, line, 240, 171);
  tft.fillRoundRect(92, 192, 296, 10, 5, C(60, 64, 74));
  int fill = constrain(pct, 0, 100) * 288 / 100;
  if (fill > 0) tft.fillRoundRect(96, 194, fill, 6, 3, blue);
  char progress[8]; snprintf(progress, sizeof(progress), "%d%%", constrain(pct, 0, 100));
  txt(F12B, primary, TC_DATUM, progress, 240, 220);
  txt(F12, secondary, TC_DATUM, "ClockOS keeps your clock current", 240, 246);
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

void setDisplayBacklight(bool enabled) {
  // GPIO27 is the panel's active-high backlight enable; assert the output
  // explicitly rather than relying only on the controller's display-off mode.
  pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(TFT_BACKLIGHT_PIN, enabled ? HIGH : LOW);
  delayMicroseconds(100);
  digitalWrite(TFT_BACKLIGHT_PIN, enabled ? HIGH : LOW);
}

void sleepDisplay() {
  if (screenSleeping) return;
  screenBeforeSleep = screen;
  tft.fillScreen(0x0000);
  tft.writecommand(0x28); delay(20); // panel display off
  tft.writecommand(0x10); delay(20); // panel sleep-in
  setDisplayBacklight(false);        // cut LED backlight after panel commands
  screenSleeping = true;
}

void wakeDisplay() {
  setDisplayBacklight(false);
  tft.writecommand(0x11); delay(120); // panel sleep-out
  tft.writecommand(0x29); delay(20);  // panel display on while still dark
  screenSleeping = false; lastActivity = millis();
  setDisplayBacklight(true);
  // Home is redrawn from black after the panel is ready, avoiding a white wake flash.
  showHome();
}

void drawWifiPageStatus() {
  tft.fillRoundRect(16, 54, 448, 38, 11, UI_KEY);
  bool connected = WiFi.status() == WL_CONNECTED;
  String state = connected ? fitText(F12, WiFi.SSID(), 160) : (WiFi.status() == WL_IDLE_STATUS ? "Connecting…" : "Not connected");
  txt(F12B, UI_TEXT, ML_DATUM, "Network", 30, 73);
  txt(F12, connected ? C(72, 202, 114) : UI_MUTED, MR_DATUM, state, 447, 73);
  drawWifiSignal(364, 74, connected ? rssiLevel(WiFi.RSSI()) : 0, UI_KEY, C(10, 132, 255), C(85, 91, 104), !connected && WiFi.status() == WL_IDLE_STATUS);
}

void showWifiPage() {
  screen = S_WIFI; drawBackHeader("Wi-Fi");
  drawWifiPageStatus();
  settingsGroupRow(102, "Choose network", " >", true);
  settingsGroupRow(148, "Refresh networks", " >");
  settingsGroupRow(194, "Saved networks", savedNetworkCount ? String(savedNetworkCount) + " saved" : "None");
  txt(F12, UI_MUTED, TC_DATUM, "Wi-Fi can be configured here. Offline setup is available only during first-run setup.", 240, 266);
}

void showWeatherPage() {
  screen = S_WEATHER; drawBackHeader("Weather");
  settingsGroupRow(54, "Location mode", settings.manualWeather ? "Manual city" : "Automatic", settings.manualWeather);
  settingsGroupRow(100, "Automatic location", "Use Wi-Fi location", !settings.manualWeather);
  settingsGroupRow(146, "Manual US city", fitText(F12, manualCity, 160), settings.manualWeather);
  settingsGroupRow(192, "Refresh weather", weather.valid ? "Update now" : "Connect Wi-Fi", true);
  txt(F12, UI_MUTED, TC_DATUM, "Philadelphia is the default manual city. Automatic location remains available.", 240, 262);
}

void showCalendarPage() {
  screen = S_CALENDAR; drawBackHeader("Calendar", false);
  struct tm ti; bool ok = getLocalTime(&ti, 0);
  int year = ok ? ti.tm_year + 1900 : 2026, month = ok ? ti.tm_mon + 1 : 1;
  char title[32]; snprintf(title, sizeof(title), "%02d/%04d", month, year);
  txt(F18B, UI_TEXT, TC_DATUM, title, 240, 62);
  const char* days = "S    M    T    W    T    F    S";
  txt(F12B, UI_MUTED, TC_DATUM, days, 240, 91);
  int first = 0, maxDay = daysInMonth(year, month);
  for (int d = 1; d <= maxDay; d++) {
    int pos = first + d - 1, x = 37 + (pos % 7) * 67, y = 112 + (pos / 7) * 20;
    txt(F12, d == (ok ? ti.tm_mday : 1) ? C(10, 132, 255) : UI_TEXT, MC_DATUM, String(d), x, y);
  }
  tft.drawFastHLine(18, 224, 444, C(58, 62, 72));
  if (settings.calendarEnabled && classroomAssignmentCount > 0) {
    for (int i = 0; i < min(classroomAssignmentCount, 3); i++) {
      String line = fitText(F12, assignmentTitles[i] + "  " + assignmentDues[i], 410);
      txt(F12, UI_TEXT, TL_DATUM, line, 34, 246 + i * 22);
    }
  } else txt(F12, UI_MUTED, TC_DATUM, settings.calendarEnabled ? "No cached Classroom items" : "Calendar disabled in Settings", 240, 265);
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
  pinMode(TFT_BACKLIGHT_PIN, OUTPUT); digitalWrite(TFT_BACKLIGHT_PIN, HIGH);
  applyAppearanceTheme();

  spr.setColorDepth(16); spr.createSprite(100, 100);

  SPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);   // SD on the default (VSPI) bus; TFT uses HSPI
  sdPresent = SD.begin(SD_PIN_CS);
  if (sdPresent) ensureClockOsSdLayout();
  sdOk = clockOsSdReady();
  if (!sdPresent) Serial.println("SD not found");
  else if (!sdOk) Serial.println("SD present; ClockOS folders could not be created");

  ensureCalibration();
  loadSettings();
  applyAppearanceTheme();
  loadAlarm();
  loadClassroomCache();
  lastActivity = millis();

  // A previous firmware version could reboot for OTA after saving Wi-Fi but
  // before the setup wizard wrote setupComplete. Resume from those saved
  // credentials instead of forcing the user through setup again.
  bool savedWifiConnected = false;
  if (sdOk && !settings.offline && settings.wifiEnabled && loadCreds())
    savedWifiConnected = trySavedNetworks(15000);
  if (!settings.setupComplete && savedWifiConnected) {
    settings.setupComplete = true;
    saveSettings();
  }

  if (!settings.setupComplete) {
    showWelcome();
  } else if (settings.offline || !settings.wifiEnabled) {
    showClockEditor();
  } else if (savedWifiConnected) {
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
      if (x < 240) { settings.wifiEnabled = true; openWifiScanner(S_WELCOME); }
      else { settings.wifiEnabled = false; settings.offline = true; showClassroomSetup(); }
    }

  } else if (screen == S_SCAN) {
    if (readTouch(x, y)) {
      if (x < 85 && y < 40) {
        if (scanReturnScreen == S_SETTINGS) showSettingsPage();
        else if (scanReturnScreen == S_WIFI) showWifiPage();
        else if (scanReturnScreen == S_WELCOME) showWelcome();
        else showHome();
        return;
      }
      if (y >= 278) { showScan(); return;}
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
              // Don't install/reboot mid-wizard: setupComplete is saved only after sync setup.
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
      if (x < 52 && y >= 270) { showWifiPage(); return; }
      if (x >= 52 && x < 94 && y >= 270) { showSdCardPage(); return; }
      if (x >= 166 && y >= 270) { showSettingsPage(); return; }
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
      static uint32_t lastClassroomRefresh = 0;
      if (millis() - lastClassroomRefresh > 60000UL) { lastClassroomRefresh = millis(); loadClassroomCache(); drawRightPanel(); }
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
    if (swipeBackDetected) { swipeBackDetected = false; settingsAtRoot = true; showSettingsPage(); return; }
    if (y < 44 && x < 100) {
      if (settingsAtRoot) leaveSettingsPage(); else { settingsAtRoot = true; showSettingsPage(); }
      return;
    }
    if (settingsAtRoot) {
      if (x < 12 || x >= 468 || y < 52 || y >= 304) return;
      settingsCategory = constrain((y - 52) / 42, 0, 5);
      settingsAtRoot = false; showSettingsPage(); return;
    }
    if (x < 12 || x >= 468 || y < 56 || y >= 306) return;
    int row = (y - 56) / (settingsCategory == 2 ? 46 : 50);
    if (settingsCategory == 0) {
      if (row == 0) settings.use24Hour = !settings.use24Hour;
      else if (row == 1) settings.showBatteryPercent = !settings.showBatteryPercent;
      else if (row == 2) { settingsCategory = 2; showSettingsPage(); return; }
      else return;
      saveSettings(); showSettingsPage(); drawStatusBlock(); return;
    }
    if (settingsCategory == 1) {
      if (row == 1) { openWifiScanner(S_SETTINGS); return; }
      return;
    }
    if (settingsCategory == 2) {
      if (row < 0 || row > 4) return;
      const char* ids[] = {"crystal", "midnight", "ocean", "sunrise", "graphite"};
      appearanceTheme = ids[row]; applyAppearanceTheme(); saveSettings(); showSettingsPage(); return;
    }
    if (settingsCategory == 3) {
      if (row == 0) settings.calendarEnabled = !settings.calendarEnabled;
      else if (row == 1) settings.classroomEnabled = !settings.classroomEnabled;
      else if (row == 2) settings.notifications = !settings.notifications;
      else return;
      saveSettings(); showSettingsPage(); drawRightPanel(); return;
    }
    if (settingsCategory == 4) {
      if (row == 1) { showSdCardPage(); return; }
      if (row == 2) { showSdFilesPage(); return; }
      return;
    }
    if (settingsCategory == 5) {
      if (y >= 56 && y < 106) {
        uint32_t now = millis();
        if (now - versionTapWindow > 2200) versionTapCount = 0;
        versionTapWindow = now; versionTapCount++;
        if (versionTapCount >= 5) { versionTapCount = 0; showDeveloperPinPage(); }
      } else if (y >= 122 && y < 172) showFactoryResetPage();
    }
  } else if (screen == S_SDCARD) {
    if (!readTouch(x, y)) return;
    if (swipeBackDetected) { swipeBackDetected = false; settingsAtRoot = false; showSettingsPage(); return; }
    if (x < 100 && y < 45) { settingsAtRoot = false; showSettingsPage(); return;}
    if (y >= 146 && y < 184) { prepareSdCard(); showSdCardPage(); return; }
    if (y >= 192 && y < 230) { showSdFilesPage(); return; }
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
  } else if (screen == S_APPEARANCE) {
    if (!readTouch(x, y)) return;
    if (swipeBackDetected) { swipeBackDetected = false; showSettingsPage(); return; }
    if (x < 100 && y < 45) { showSettingsPage(); return; }
    const char* ids[] = {"crystal", "midnight", "ocean", "sunrise", "graphite"};
    for (int i = 0; i < 5; i++) {
      int bx = 14 + (i % 2) * 242, by = 82 + (i / 2) * 44;
      if (x >= bx && x < bx + 210 && y >= by && y < by + 34) {
        appearanceTheme = ids[i]; applyAppearanceTheme(); saveSettings(); showAppearancePage(); return;
      }
    }
  } else if (screen == S_DEV_PIN) {
    if (!readTouch(x, y)) return;
    if (swipeBackDetected || (x < 100 && y < 45)) { swipeBackDetected = false; showSettingsPage(); return; }
    int col = (x - 45) / 145, row = (y - 112) / 42;
    if (col < 0 || col > 2 || row < 0 || row > 3) return;
    int key = row * 3 + col;
    if (key < 9 && developerPinInput.length() < 4) developerPinInput += String(key + 1);
    else if (key == 9) developerPinInput = "";
    else if (key == 10 && developerPinInput.length() < 4) developerPinInput += "0";
    else if (key == 11) {
      if (developerPinInput == "0000") { developerModeUnlocked = true; showDeveloperModePage(); return; }
      developerPinInput = "";
    }
    showDeveloperPinPage();
  } else if (screen == S_DEV_MODE) {
    if (!readTouch(x, y)) return;
    if (swipeBackDetected || (x < 100 && y < 45)) { swipeBackDetected = false; showSettingsPage(); return; }
    if (y >= 166 && y < 215 && x < 240) {
      drawUpdatePage(0, "Rollback is available only with a signed recovery image.");
    } else if (y >= 166 && y < 215 && x >= 240) {
      drawUpdatePage(0, "Recovery tools ready. No changes were made.");
    }
  } else if (screen == S_WIFI) {
    static uint32_t lastWifiStatusFrame = 0;
    if (millis() - lastWifiStatusFrame > 180UL) {
      lastWifiStatusFrame = millis();
      drawWifiPageStatus();
    }
    if (!readTouch(x, y)) return;
    if (swipeBackDetected) { swipeBackDetected = false; showHome(); return; }
    if (x < 100 && y < 45) { showHome(); return; }
    if (y >= 102 && y < 140) { openWifiScanner(S_WIFI); return; }
    if (y >= 148 && y < 186) { openWifiScanner(S_WIFI); return; }
  } else if (screen == S_WEATHER) {
    if (!readTouch(x, y)) return;
    if (swipeBackDetected) { swipeBackDetected = false; showHome(); return; }
    if (x < 100 && y < 45) { showHome(); return; }
    if (y >= 100 && y < 138) { settings.manualWeather = false; saveSettings(); fetchWeather(); showWeatherPage(); return; }
    if (y >= 146 && y < 184) { settings.manualWeather = true; saveSettings(); showCityKeyboard(); return; }
    if (y >= 192 && y < 230) { fetchWeather(); showWeatherPage(); return; }
  } else if (screen == S_CALENDAR) {
    if (readTouch(x, y)) {
      if (swipeBackDetected) { swipeBackDetected = false; showHome(); return; }
      if (x < 100 && y < 45) showHome();
    }
  } else if (screen == S_ALARM) {
    if (!readTouch(x, y)) return;
    if (alarmListMode) {
      if (swipeBackDetected || (x < 100 && y < 45)) { swipeBackDetected = false; showHome(); return; }
      for (int i = 0; i < alarmCount; i++) {
        int y0 = 68 + i * 46;
        if (y >= y0 && y < y0 + 40) {
          if (x >= 388) { alarms[i].enabled = !alarms[i].enabled; saveAlarm(); drawAlarmList(); }
          else beginAlarmEdit(i);
          return;
        }
      }
      if (y >= 264 && x < 240 && alarmCount < MAX_ALARMS) { beginAlarmEdit(-1); return; }
      if (y >= 264 && x >= 240) { showHome(); return; }
    } else {
      if (swipeBackDetected || (x < 100 && y < 45)) { swipeBackDetected = false; drawAlarmList(); return; }
      if (y >= 92 && y < 126) {
        if (x < 100) alarmDraft.hour = (alarmDraft.hour + 23) % 24;
        else if (x < 200) alarmDraft.hour = (alarmDraft.hour + 1) % 24;
        else if (x >= 285 && x < 380) alarmDraft.minute = (alarmDraft.minute + 59) % 60;
        else if (x >= 380) alarmDraft.minute = (alarmDraft.minute + 1) % 60;
        drawAlarmEditor(); return;
      }
      if (y >= 151 && y < 185) {
        if (x < 82) adjustAlarmDate(0, -1); else if (x < 160) adjustAlarmDate(0, 1);
        else if (x < 245) adjustAlarmDate(1, -1); else if (x < 330) adjustAlarmDate(1, 1);
        else if (x < 405) adjustAlarmDate(2, -1); else adjustAlarmDate(2, 1);
        drawAlarmEditor(); return;
      }
      if (y >= 188 && y < 226) {
        int i = (x - 68) / 58; if (i >= 0 && i < 7) alarmDraft.repeatMask ^= (1 << i);
        drawAlarmEditor(); return;
      }
      if (y >= 244 && y < 286) {
        if (x < 157) { alarmDraft.enabled = !alarmDraft.enabled; drawAlarmEditor(); return; }
        if (x < 312) {
          if (editingAlarmIndex >= 0) alarms[editingAlarmIndex] = alarmDraft;
          else if (alarmCount < MAX_ALARMS) alarms[alarmCount++] = alarmDraft;
          saveAlarm(); drawAlarmList(); return;
        }
        drawAlarmList(); return;
      }
    }
  } else if (screen == S_FACTORY_RESET) {
    if (!readTouch(x, y)) return;
    if (swipeBackDetected || (x < 100 && y < 45)) { swipeBackDetected = false; factoryResetArmed = false; showSettingsPage(); return; }
    if (y >= 246 && y < 290) {
      if (factoryResetArmed) factoryResetClock();
      factoryResetArmed = true; showFactoryResetPage(); return;
    }
  }
}
