# ClockOSv3.9 correction release

**Release identity:** `ClockOSv3.9`

**Release type:** small corrective release (`v3.8` → `v3.9`)

**Previous stable release:** `ClockOSv3.8`

## User-requested corrections

Settings is reorganized as an iPadOS-style grouped hierarchy, with labeled categories, clearer back navigation, inset rows and dividers, live values, larger switches and chevrons, and plain-language descriptions on the detail pages. **General → Turn display off** is saved as a preference; eligible Home, Settings, and detail screens can sleep after inactivity, and wake redraws the prior screen before enabling the backlight. **General → Weather & location** opens the Weather controls and remembers its Settings return destination.

The Home Wi-Fi, SD, battery, and Settings indicators have aligned labels and consistent system-style glyphs; the profile silhouette and SD indicator have also been revised. SD status distinguishes absent, inserted-needs-setup, and ready states, refreshes at a bounded interval, and prepares ClockOS folders without formatting the card or deleting unrelated files.

Eight transparent 96 × 96 Meteocons PNGs and the upstream MIT license are checked in. Windows Setup downloads and validates those exact files into `/.source/icons/` on the SD card, and firmware loads the named PNGs from that path. **Settings → Weather → Weather PNG** reports the file loaded or the missing/fallback reason. Visual inspection caught black-filled symbols from the first SVG rasterization; the final exports preserve the gradients, cloud fills, colored weather detail, and transparency. Host tests now reject similarly dark or broken outputs.

## Build and validation

| Item | Result |
| --- | --- |
| Board/FQBN | `esp32:esp32:esp32:PartitionScheme=min_spiffs` |
| Arduino CLI / ESP32 core | `1.5.2-rc.1` / `2.0.17` |
| Libraries | TFT_eSPI `2.5.43`, PNGdec `1.1.6`, ArduinoJson `7.4.3` |
| Flash usage | 1,136,961 / 1,966,080 bytes (57%) |
| Dynamic memory | 96,432 / 327,680 bytes (29%) |
| Binary size | 1,143,536 bytes |
| SHA-256 | `b25ecfcb89a7b9ea9426496ecb0de6083ee478a6bb4b5fd071288740c986a024` |

ESP32 compilation succeeded and all 12 host regression tests pass, covering source behavior, the installer, PNG dimensions/transparency/visibility, release-manifest/hash integrity, and SD-layout parity. The compiler still reports ArduinoJson deprecation and third-party library warnings; there are no compile errors.

**Hardware status remains open:** the sandbox has no attached board, so physical display appearance, touch navigation, SD insertion/removal, PNG rendering, OTA, and backlight-off/wake have not been confirmed on the user's unit. The [LCDWiki board specification](https://www.lcdwiki.com/4.0inch_ESP32-32E_Display) confirms GPIO27 is active-high (LOW is the expected off level), but only a device test can confirm the user's unit behaves that way.
