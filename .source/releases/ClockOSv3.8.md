# ClockOSv3.8 build record

**Release identity:** `ClockOSv3.8`
**Release type:** major visual and interaction release (`v2.8` → `v3.8`)
**Previous stable release:** `ClockOSv2.8`

## Included changes

- Rebuilt the home screen as an original dark bedside display with large segmented time, a next-alarm line, date/time-format status, compact connectivity/battery controls, weather, and calendar cards.
- Rebuilt Settings as a full-width grouped list with clear category summaries, consistent 44–48 px touch rows, chevrons, selected-theme states, and larger switches.
- Applied the same hierarchy to Wi-Fi, weather, calendar, storage, alarms, update, reset, developer, and welcome screens.
- Prevented home-only painters from overwriting settings screens, made SD status redraws wizard-safe, corrected calendar month alignment, and protected the destructive reset confirmation with a release between taps.
- Hardened weather PNG decoding with valid open/seek callbacks, a fixed scanline callback return value, bounded dimensions, and a vector fallback when decoding fails.

## Build and validation

- **Board/FQBN:** `esp32:esp32:esp32:PartitionScheme=min_spiffs`
- **Arduino CLI:** `1.5.2-rc.1`
- **ESP32 core:** `2.0.17`
- **Libraries:** TFT_eSPI `2.5.43`, PNGdec `1.1.6`, ArduinoJson `7.4.3`
- **Flash usage:** 1,131,761 / 1,966,080 bytes (57%)
- **Dynamic memory:** 96,416 / 327,680 bytes (29%)
- **Binary size:** 1,138,336 bytes
- **SHA-256:** `d7ad53110a909c976642dff1814a1f414b557a8abcf8e403e2b8bf0d23de9a98`
- **Validation:** Arduino CLI compilation, firmware image inspection, manifest/hash integrity tests, SD layout parity tests, and repository whitespace checks passed.
- **Hardware status:** Physical Hosyond display/touch/SD/OTA/backlight validation remains open because no device is attached to this sandbox.
