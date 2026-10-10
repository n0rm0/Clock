# ClockOSv2.8 build record

**Release identity:** `ClockOSv2.8`
**Release type:** minor UI and installer fix (`v2.7` → `v2.8`)
**Previous release:** `ClockOSv2.7`

## Published payloads

| Artifact | Repository path | Validation |
| --- | --- | --- |
| Raw Arduino source | `.source/uncompiled/updates/ClockOSv2.8/ClockOSv2.8.ino` | Same-name sketch folder with support headers |
| Application binary | `.source/compiled/updates/ClockOSv2.8/ClockOSv2.8.bin` | Arduino CLI compile, ESP32 core 2.0.17 |
| Release manifest | `.source/releases/ClockOSv2.8.json` and `current.json` | Byte count and SHA-256 verified |

## Included changes

- Replaced the compact left-rail/right-pane Settings layout with a six-row, full-width grouped category list, clear detail pages, consistent 42–50 px touch rows, larger switches, category glyphs, concise status values, and local Back navigation.
- Retained the compiled display rotation, touch calibration values and pressure threshold. Physical panel validation remains outstanding.
- Enlarged the Windows setup window, made it resizable, and moved status/progress/Start into a protected bottom footer. Failed jobs restore the primary action as **TRY AGAIN**.
- Added an visible, explicit **Erase all existing files** option (auto-selected when the user enables SD installation in accordance with the requested clean-card behavior) for Windows SD setup. Unselected normal preparation is non-destructive. Erase remains restricted to verified removable drives, requires two confirmations (second defaults to No), never formats/repartitions, preserves only Windows metadata roots, reports incomplete deletions, and does not restore old JSON/settings/secrets.

## Build and validation

- **Board/FQBN:** `esp32:esp32:esp32:PartitionScheme=min_spiffs`
- **Arduino CLI:** 1.5.2-rc.1 (binary SHA-256/pinned distribution not recorded; this is a locally verified build, not a provenance-grade reproducible release)
- **ESP32 core:** 2.0.17
- **Libraries:** TFT_eSPI 2.5.43, PNGdec 1.1.6, ArduinoJson 7.4.3
- **Flash usage:** 1,129,157 / 1,966,080 bytes (57%)
- **Dynamic memory:** 96,352 / 327,680 bytes (29%)
- **Binary size:** 1,135,728
- **SHA-256:** `682afcb79c9d5bb88a8dba585c811f0ead8b854838bbee6f4ef4c6339834d5ec`
- **Hardware status:** physical Hosyond rendering, corner/center touch, SD behavior, OTA update and GPIO27 checks remain open.
- **Windows status:** native Windows 10/11 execution and high-DPI layout validation remain open.
