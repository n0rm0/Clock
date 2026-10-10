# ClockOSv2.6 build record

**Release identity:** `ClockOSv2.6`
**Release type:** minor bug-fix release (`v2.5` → `v2.6`)
**Previous release:** `ClockOSv2.5`

## Published payloads

| Artifact | Repository path | Validation |
| --- | --- | --- |
| Raw Arduino source | `.source/uncompiled/updates/ClockOSv2.6/ClockOSv2.6.ino` | Same-name sketch folder with supporting headers |
| Application binary | `.source/compiled/updates/ClockOSv2.6/ClockOSv2.6.bin` | Arduino CLI build, ESP32 core 2.0.17 |
| Release manifest | `.source/releases/current.json` | Installer/release identity metadata |

## Included fixes

- Restored Wi-Fi scanner access from Settings and preserved a sensible Back destination for onboarding, Settings, and the Wi-Fi page. Scan failures and empty results are now visible, stale scan results are cleared, and the screen reports how many networks were found.
- Defers automatic OTA during the first-run Wi-Fi step until onboarding saves `setupComplete`; if the old firmware interrupted setup after saving Wi-Fi, V2.6 resumes from those credentials instead of reopening the wizard.
- Unifies setup, Settings, Wi-Fi scanner/keyboard, and alarms with a dark Apple/iPad system aesthetic, compact labels, colored category glyphs, grouped rows, and iOS-style switches; the welcome page is StandBy-inspired.
- Explicitly drives the active-high GPIO27 backlight low for sleep and restores it on wake.
- Adds a same-name `ClockOSV1/ClockOSV1.ino` and compiled alias while retaining the `updateV1` compatibility paths.
- OTA, installer source, default-flash-target, and Auto binary selection use semantic version ordering across supported ClockOS/updateV application releases; v2.10 correctly outranks v2.9 and bootloaders are excluded.

## Build and validation

- **Board/FQBN:** `esp32:esp32:esp32:PartitionScheme=min_spiffs`
- **ESP32 core:** `2.0.17`
- **Libraries:** TFT_eSPI 2.5.43, PNGdec 1.1.6, ArduinoJson 7.4.3
- **Flash usage:** 1,128,489 / 1,966,080 bytes (57%)
- **Dynamic memory:** 96,352 / 327,680 bytes (29%)
- **Binary size:** 1,135,072 bytes
- **SHA-256:** `9dbe6272c1aaae99ed809a68c215b80910d061f615cebcdf01e86b197c35d60c`

Arduino CLI build passed. Physical-device Wi-Fi scan, update, setup-resume, Apple-style layout, and GPIO27 backlight sleep/wake tests remain required.
