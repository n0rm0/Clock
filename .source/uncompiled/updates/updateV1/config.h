#pragma once
// ====================================================================
//  config.h - Hosyond 4" ESP32-32E display (480x320, ST7796S, XPT2046 touch)
//  Display + touch pins are NOT set here: the installer passes them to
//  TFT_eSPI as build flags (see TFT_FLAGS in install.py). They match the
//  LCD wiki: CS15 DC2 SCLK14 MOSI13 MISO12 BL27 RST=EN, touch CS33 IRQ36.
//  If you build in Arduino IDE by hand, put the same values in TFT_eSPI's User_Setup.h
//  and use the HSPI port (#define USE_HSPI_PORT).
// ====================================================================
#define TFT_W 480
#define TFT_H 320
#define TFT_ROTATION 3          // landscape, rotated 180 degrees from the previous orientation

// Official TFT_eSPI calibration output for rotation 3.
#define TOUCH_CAL_X0 365
#define TOUCH_CAL_X1 3431
#define TOUCH_CAL_Y0 321
#define TOUCH_CAL_Y1 3368
#define TOUCH_CAL_ROTATION 7

// ---------- SD card (own SPI bus, default SPI) ----------
#define SD_PIN_CS   5
#define SD_PIN_SCK  18
#define SD_PIN_MISO 19
#define SD_PIN_MOSI 23

// ---------- Onboard RGB LED (active LOW) ----------
#define LED_R 22
#define LED_G 16
#define LED_B 17

// ---------- Misc board pins ----------
#define BAT_ADC_PIN 34
#define I2C_SDA 32
#define I2C_SCL 25

// ---------- Time ----------
#define TIME_TZ "EST5EDT,M3.2.0,M11.1.0"   // US Eastern
#define NTP_SERVER "pool.ntp.org"

// ---------- Saved on the SD card by the setup screens ----------
#define DATA_DIR      "/.source/data"
#define WIFI_FILE     "/.source/data/wifi.txt"
#define TOUCH_FILE    "/.source/data/touch.txt"      // touch calibration (5 numbers)
#define HISTORY_FILE  "/.source/data/history.txt"    // every update installed
#define ALARM_FILE    "/.source/data/alarm.txt"      // enabled,time,date,repeat mask

// ---------- Weather (placeholder values on screen for now) ----------
#define WEATHER_LAT  39.95      // Philadelphia
#define WEATHER_LON -75.16
#define WEATHER_UNITS_F true    // true = Fahrenheit
#define WEATHER_REFRESH_MS (10UL * 60UL * 1000UL)

// ---------- SD paths ----------
#define ICON_DIR       "/.source/icons"
#define UPDATES_DIR    "/.source/compiled/updates"          // EVERY downloaded .bin is kept here (own file each)
#define INSTALLED_FILE "/.source/data/installed.txt"

// ---------- GitHub updates ----------
// The device fetches the NEWEST .bin (by commit date) from .source/compiled/updates/ in this repo.
#define GH_OWNER        "n0rm0"
#define GH_REPO         "Clock"
#define GH_UPDATES_DIR  ".source/compiled/updates"
#define GH_TOKEN        ""                         // only needed if the repo is private
#define UPDATE_CHECK_MS (60UL * 60UL * 1000UL)     // re-check every hour while running

// ---------- Colors (RGB565) ----------
#define COL_BG      0x0000
#define COL_WHITE   0xFFFF
#define COL_RED     0xF800
#define COL_BLUE    0x041F
#define COL_DIM     0x7BEF
