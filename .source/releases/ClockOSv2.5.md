# ClockOSv2.5 build record

**Release identity:** `ClockOSv2.5`
**Release type:** major
**Previous stable release:** `ClockOSV1` (`updateV1` compatibility slot)

## Published payloads

| Artifact | Repository path | Validation |
| --- | --- | --- |
| Raw Arduino source | `.source/uncompiled/updates/ClockOSv2.5/ClockOSv2.5.ino` | Same-name sketch folder with `config.h` and `bootloader.h` |
| Application binary | `.source/compiled/updates/ClockOSv2.5/ClockOSv2.5.bin` | Arduino CLI build, ESP32 core 2.0.17 |
| Release manifest | `.source/releases/current.json` | Installer and release identity metadata |

## Build command and result

The application was compiled for the Hosyond ESP32-32E configuration with:

- **Board/FQBN:** `esp32:esp32:esp32:PartitionScheme=min_spiffs`
- **ESP32 core:** `2.0.17`
- **Libraries:** TFT_eSPI 2.5.43, PNGdec 1.1.6, ArduinoJson 7.4.3
- **Flash usage:** 1,132,685 bytes / 1,966,080 bytes (57%)
- **Dynamic memory:** 96,336 bytes / 327,680 bytes (29%)
- **Binary size:** 1,139,264 bytes
- **SHA-256:** `e101a7fb21c6a509f87f6ab378db2545dff88ec2ee12a0b5c65e7e1ca7eca97f`

The initial build completed successfully; the follow-up settings/sidebar, automatic fresh-SD layout creation, vector-weather fallback, Philadelphia weather layout, and compact typography were rebuilt successfully for the corrective PR update. Existing ArduinoJson and third-party library deprecation warnings remain non-blocking and should be addressed in a dedicated compatibility pass rather than mixed into this release.

## Release safeguards

ClockOSV1 source and binary remain in their existing legacy locations. ClockOSv2.5 is published as a separate same-name Arduino sketch and application binary, so both the installer and OTA updater can select the highest compatible application version without automatically selecting a fallback bootloader.

## Still required before field rollout

A physical Hosyond display must validate orientation, touch calibration, SD detection, sleep/wake behavior, status icon geometry, Wi-Fi animation, alarm UI, and OTA/rollback behavior. The installer also needs Windows 10 and Windows 11 runtime validation for the title-bar icon and console-free launch path.
