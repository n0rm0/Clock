# ClockOSv2.7 build record

**Release identity:** `ClockOSv2.7`
**Release type:** minor bug-fix release (`v2.6` → `v2.7`)
**Previous release:** `ClockOSv2.6`

## Published payloads

| Artifact | Repository path | Validation |
| --- | --- | --- |
| Raw Arduino source | `.source/uncompiled/updates/ClockOSv2.7/ClockOSv2.7.ino` | Same-name sketch folder with supporting headers |
| Application binary | `.source/compiled/updates/ClockOSv2.7/ClockOSv2.7.bin` | Arduino CLI build, ESP32 core 2.0.17 |
| Release manifest | `.source/releases/current.json` | Installer/release identity metadata |

## Included fixes

- Makes on-device **Settings → SD Card → Prepare ClockOS storage** create the exact same root and `.source/` directory tree as Windows Setup.
- Uses the shared directory-list specification in firmware and installer, and checks every required directory before showing **Ready**.
- Preparation remains non-destructive: both paths add missing folders without repartitioning, formatting, or erasing unrelated files. Windows Setup may populate source, firmware, theme, and icon payloads; the on-device offline prepare action creates the matching folder structure and saves the device's settings.

## Build and validation

- **Board/FQBN:** `esp32:esp32:esp32:PartitionScheme=min_spiffs`
- **ESP32 core:** `2.0.17`
- **Libraries:** TFT_eSPI 2.5.43, PNGdec 1.1.6, ArduinoJson 7.4.3
- **Flash usage:** 1,128,317 / 1,966,080 bytes (57%)
- **Dynamic memory:** 96,352 / 327,680 bytes (29%)
- **Binary size:** 1,134,896 bytes
- **SHA-256:** `9f84a6e448926b7db97dafd683c0a376ffd2e8aa2ea939f33ad49370d5d0eeb3`
- **Automated tests:** firmware and installer directory lists match; Windows structure creation produces the same folders in a temporary directory and preserves an unrelated user file.
- **Hardware status:** physical SD-card preparation and status-icon validation remain required.
