// ====================================================================
//  clock.ino - Hosyond 4" ESP32-32E (480x320 ST7796S, XPT2046 touch)
//  Libraries: TFT_eSPI (display + its built-in touch), PNGdec, ArduinoJson
//  Build flags for TFT_eSPI come from the installer (see config.h).
//  Flow: Welcome -> pick WiFi -> password keyboard -> connect -> Home
//  Saved on the SD card: /.source/data/wifi.txt, touch.txt (calibration)
//  Icons: wifi loader + battery redrawn from Uiverse.io designs
//         (Erasmus001 wifi-loader, Yaya12085 battery), keys = patrick_2593,
//         weather = Meteocons PNGs from /.source/icons
//  Home weather + calendar are still PLACEHOLDERS (not live yet).
// ====================================================================
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>
#include <TFT_eSPI.h>
#include <PNGdec.h>
#include "config.h"
#include "bootloader.h"   // GitHub updater

// Arduino's automatic prototype generation sees keyLabel() before the full
// declaration below. Keep the type visible to that generated prototype.
struct Key;
struct AlarmState;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

// ---------------- colors ----------------
#define C(r,g,b) tft.color565(r,g,b)
uint16_t UI_BLUE, UI_BACK, UI_TEXT, UI_KEY, UI_LIP, UI_RED, UI_YEL, UI_GREEN;

enum Screen { S_WELCOME, S_SCAN, S_KEYS, S_HOME, S_ALARM };
Screen screen = S_WELCOME;

String selSsid, password, errMsg;
String ssids[5]; int rssis[5]; int nNets = 0;
bool showPass = false; uint8_t layer = 0;
bool sdOk = false;

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
  for (int i = 0; i < 5; i++) calData[i] = (uint16_t)f.readStringUntil('\n').toInt();
  f.close();
  return calData[0] || calData[1] || calData[2] || calData[3];
}

void saveCal() {
  if (!sdOk) return;
  SD.mkdir("/.source"); SD.mkdir(DATA_DIR); SD.remove(TOUCH_FILE);
  File f = SD.open(TOUCH_FILE, FILE_WRITE);
  if (!f) return;
  for (int i = 0; i < 5; i++) f.println(calData[i]);
  f.close();
}

void ensureCalibration() {
  // Use the official TFT_eSPI calibration measured for rotation 3. Do not
  // launch the calibration screen or replace these values from the SD card.
  tft.setTouch(calData);
}

bool readTouch(int &x, int &y) {
  static uint32_t last = 0;
  uint16_t tx, ty;
  // TFT_eSPI documents Z=350 as the default pressure threshold. Passing it
  // explicitly prevents the disconnected/idle XPT2046 readings from acting
  // like touches (the test sketch previously exposed this as RAW_X=0).
  if (tft.getTouch(&tx, &ty, 350) && millis() - last > 220) {
    last = millis(); x = tx; y = ty; return true;
  }
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
AlarmState alarm = {false, 7, 0, 2026, 1, 1, 0};
AlarmState alarmDraft = {false, 7, 0, 2026, 1, 1, 0};
bool alarmRinging = false;

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

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;
  lastWeatherFetch = millis();
  WiFiClientSecure client;
  client.setInsecure(); // ESP32 has no bundled CA store; data is non-sensitive.
  HTTPClient http;
  double lat = WEATHER_LAT, lon = WEATHER_LON;
  String city = "Local";

  if (http.begin(client, "https://ipapi.co/json/")) {
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
  SD.mkdir("/.source"); SD.mkdir(DATA_DIR);
  SD.remove(WIFI_FILE);
  File f = SD.open(WIFI_FILE, FILE_WRITE);
  if (!f) return;
  f.println(selSsid); f.println(password); f.close();
}

bool loadCreds() {
  File f = SD.open(WIFI_FILE);
  if (!f) return false;
  selSsid = f.readStringUntil('\n'); selSsid.trim();
  password = f.readStringUntil('\n'); password.trim();
  f.close();
  return selSsid.length() > 0;
}

void saveAlarm() {
  if (!sdOk) return;
  SD.mkdir("/.source"); SD.mkdir(DATA_DIR); SD.remove(ALARM_FILE);
  File f = SD.open(ALARM_FILE, FILE_WRITE);
  if (!f) return;
  f.println(alarm.enabled ? 1 : 0); f.println(alarm.hour); f.println(alarm.minute);
  f.println(alarm.year); f.println(alarm.month); f.println(alarm.day); f.println(alarm.repeatMask);
  f.close();
}

bool loadAlarm() {
  if (!sdOk) return false;
  File f = SD.open(ALARM_FILE);
  if (!f) return false;
  alarm.enabled = f.readStringUntil('\n').toInt() != 0;
  alarm.hour = constrain(f.readStringUntil('\n').toInt(), 0, 23);
  alarm.minute = constrain(f.readStringUntil('\n').toInt(), 0, 59);
  alarm.year = constrain(f.readStringUntil('\n').toInt(), 2024, 2099);
  alarm.month = constrain(f.readStringUntil('\n').toInt(), 1, 12);
  alarm.day = constrain(f.readStringUntil('\n').toInt(), 1, 31);
  alarm.repeatMask = f.readStringUntil('\n').toInt() & 0x7F;
  f.close();
  return true;
}

String alarmTimeText(const AlarmState& a) {
  char b[12]; int h = a.hour % 12; if (!h) h = 12;
  snprintf(b, sizeof(b), "%d:%02d %s", h, a.minute, a.hour < 12 ? "AM" : "PM");
  return String(b);
}

String nextAlarmText() {
  if (!alarm.enabled) return "No alarm";
  return alarmTimeText(alarm) + (alarm.repeatMask ? "  Repeating" : "  One-time");
}

bool alarmDueNow() {
  if (!alarm.enabled) return false;
  struct tm ti;
  if (!getLocalTime(&ti, 0) || ti.tm_hour != alarm.hour || ti.tm_min != alarm.minute) return false;
  if (alarm.repeatMask) return (alarm.repeatMask & (1 << ti.tm_wday)) != 0;
  return ti.tm_year + 1900 == alarm.year && ti.tm_mon + 1 == alarm.month && ti.tm_mday == alarm.day;
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
  txt(F24B, 0x0000, TC_DATUM, "Hi, Welcome", 240, 90);
  txt(F12B, UI_TEXT, TC_DATUM, "tap to continue setup", 240, 160);
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
  txt(F12B, 0x0000, TC_DATUM, "Choose your WiFi", 240, 8);
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
  if (showPass) shown = password; else for (size_t i = 0; i < password.length(); i++) shown += '*';
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

  String t = "Password: " + selSsid; if (t.length() > 30) t = t.substring(0, 30);
  txt(F12B, 0x0000, TC_DATUM, t, 255, 12);
  if (errMsg.length()) txt(F12B, UI_RED, TC_DATUM, errMsg, 240, 92);
  drawPasswordField();
  char lb[8];
  for (int i = 0; i < nk; i++) { keyLabel(keys[i], lb); drawAnyKey(keys[i], lb, false); }
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
  if (ok) { int h = ti.tm_hour % 12; if (!h) h = 12; snprintf(buf, sizeof(buf), "%d:%02d", h, ti.tm_min); }
  else strcpy(buf, "--:--");
  drawBigTime(buf, 240, 36);
  if (ok) {                                           // only the current one shows
    if (ti.tm_hour < 12) txt(F24B, COL_WHITE, MC_DATUM, "AM", 70, 175);
    else                 txt(F24B, COL_WHITE, MC_DATUM, "PM", 170, 175);
  }
}

void drawStatusBlock() {
  tft.fillRect(0, 226, 240, 94, 0x0000);
  drawAlarmIcon(16, 246, alarm.enabled ? UI_YEL : COL_DIM);
  txt(F12, COL_DIM, ML_DATUM, "Next alarm", 34, 233);
  String next = nextAlarmText();
  if (next.length() > 28) next = next.substring(0, 28);
  txt(F12B, COL_WHITE, ML_DATUM, next, 34, 250);
  int lvl = (WiFi.status() == WL_CONNECTED) ? rssiLevel(WiFi.RSSI()) : 0;
  drawWifiSignal(28, 307, lvl, 0x0000, COL_WHITE, 0x39E7);
  int pct = readBatteryPct();
  drawBattery(170, 298, pct, 0x0000, COL_WHITE);
  char b[8]; snprintf(b, sizeof(b), "%d%%", pct);
  txt(F12, COL_DIM, ML_DATUM, b, 218, 301);
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
  txt(F12, COL_DIM, TL_DATUM, DEMO_EVENT, 256, 228);
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

void alarmButton(int x, int y, int w, const String& label, bool selected = false) {
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
  alarmDraft = alarm;
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

void showAlarmRinging() {
  alarmRinging = true;
  digitalWrite(LED_G, LOW);
  tft.fillScreen(UI_RED);
  drawAlarmIcon(240, 72, 0xFFFF);
  txt(F24B, 0xFFFF, TC_DATUM, "ALARM", 240, 132);
  txt(F12B, 0xFFFF, TC_DATUM, alarmTimeText(alarm), 240, 184);
  alarmButton(150, 245, 180, "Dismiss", false);
}

void dismissAlarm() {
  alarmRinging = false;
  digitalWrite(LED_G, HIGH);
  if (!alarm.repeatMask) { alarm.enabled = false; saveAlarm(); }
  showHome();
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
  animateLoader(240, 130);
  static int last = -1;
  if (pct != last) {
    last = pct;
    tft.fillRect(180, 186, 120, 26, UI_BLUE);
    char b[8]; snprintf(b, sizeof(b), "%d%%", pct);
    txt(F12B, UI_TEXT, TC_DATUM, b, 240, 188);
  }
}

void runUpdate() {
  if (WiFi.status() != WL_CONNECTED) return;
  loaderScreen("checking for updates");
  animateLoader(240, 130);
  BL::Result r = BL::checkAndInstall(updateProgress);
  if (r == BL::INSTALLED) { loaderScreen("update installed"); delay(800); ESP.restart(); }
  // UP_TO_DATE or FAILED: keep running the current software
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
  sdOk = SD.begin(SD_PIN_CS);
  if (!sdOk) Serial.println("SD not found");

  ensureCalibration();

  if (sdOk && loadCreds() && tryConnect(15000)) {
    configTzTime(TIME_TZ, NTP_SERVER);
    fetchWeather();
    loadAlarm();
    runUpdate();
    showHome();
  } else {
    showWelcome();
  }
}

void loop() {
  int x, y;
  static uint32_t lastTick = 0;
  static int lastMin = -1;

  if (screen == S_WELCOME) {
    if (readTouch(x, y)) showScan();

  } else if (screen == S_SCAN) {
    if (readTouch(x, y)) {
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
          case 7: showScan(); break;
          case 5:
            if (tryConnect(15000)) {
              saveCreds();
              configTzTime(TIME_TZ, NTP_SERVER);
              fetchWeather();
              loadAlarm();
              runUpdate();
              showHome();
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
      // Tapping the clock/time panel opens the alarm editor.
      if (x < 240 && y < 225) { showAlarmEditor(); return; }
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
      static uint32_t lastUpdate = millis();
      if (millis() - lastUpdate > UPDATE_CHECK_MS) { lastUpdate = millis(); runUpdate(); showHome(); }
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
      else if (x >= 240 && x < 355) { alarm = alarmDraft; saveAlarm(); showHome(); }
      else if (x >= 355) showHome();
      else drawAlarmEditor();
    }
  }
}
