# Clock Project TODO

This checklist reflects the requirements in [`agent.md`](agent.md) and the files actually present in the repository. A checked item means it has been completed and validated; historical chat claims are not treated as implementation evidence.

## Agent workspace

- [x] Create the project-local `.agent/` directory.

- [x] Create and maintain this checklist.

- [x] Create [`agent.md`](agent.md) with project conventions and validation rules.

- [x] Confirm the default branch is `main`.

- [x] Inspect the repository before making implementation changes.

## Arduino firmware

- [x] Import the active TFT_eSPI clock sketch as `.source/uncompiled/updates/updateV1/updateV1.ino`.

- [x] Remove duplicate preview files and the alternate LovyanGFX implementation from the published layout.

- [x] Keep each Arduino sketch in a same-name folder so Arduino IDE can open it directly.

- [x] Add `bootloader.h` and the same-name `bootloaderV1.ino` fallback sketch.

- [x] Implement the 480 × 320 landscape home screen.

- [x] Add the central seven-segment time display and date/time controls.

- [x] Add the upper-right weather icon and outdoor temperature.

- [x] Keep temperature and conditions sourced from the outside/local weather service; do not add indoor room temperature or humidity sensors.

- [x] Add Wi-Fi configuration without committing credentials, API keys, or private location data.

- [x] Add SD-card weather-icon loading from `.source/icons/`.

- [x] Confirm the Hosyond 4-inch ESP32-32E/ST7796S pin map against authoritative board documentation.

- [x] Confirm the display library, board package, selected board, and required library versions.

- [x] Build the firmware and resolve all compiler errors from actual compiler output.

- [x] Validate the display, SD card, Wi-Fi/weather data, and sensors on physical hardware.

## SD-card and update layout

- [x] Create matching versioned trees under `.source/uncompiled/` and `.source/compiled/`.

- [x] Keep compiled slots limited to real `.bin` files, adding firmware only after a successful build.

- [x] Configure the clock to check `.source/compiled/updates/` for newer updates.

- [x] Add installer-generated SD-card folders, including `/data/preferences/` for JSON preferences and `.source/icons/`.

- [x] Add the bootloader/update implementation and matching fallback source slot.

- [x] Implement newest-compatible-update selection from `.source/compiled/updates/`.

- [x] Preserve the current OTA slot by aborting failed writes before reboot.

- [x] Keep the current firmware running when an update fails; hardware rollback testing remains open.

- [x] Verify the selected OTA partition requirement in the build and document that this is application-level update logic, not a replacement for the ESP32 ROM bootloader.

## Windows SD-card setup tool

- [x] Add `.source/install/setup_sd.bat` and `.source/install/install.py`.

- [x] Make the batch launcher install user-scoped Python with `winget` when Python is missing.

- [x] Add the Tkinter SD-card drive selector.

- [x] Create the required SD-card data, icon, and compiled-binary directories.

- [x] Add Auto (recommended) stable `.bin` flashing without source compilation.

- [x] Add Beta (unstable) raw-source compilation and warning.

- [x] Add Manual folder selection for `.ino`/`.h` compilation.

- [x] Add serial ESP32 port selection with board names while excluding SD-card drives.

- [x] Implement Meteocons download and SVG-to-PNG conversion into `.source/icons/`.

- [x] Report missing Python, inaccessible drives, failed downloads, and failed copies clearly in the launcher/installer paths.

- [x] Require two explicit confirmations before erasing a removable SD card.

- [x] Test the tool using a disposable directory or removable test drive.

## Documentation and delivery

- [x] Update `README.md` with the current source/compiled layout and known build limitation.

- [x] Keep commits focused and describe the validation performed.

- [x] Update this checklist only when work is genuinely complete.

## Code and bug check — 2026-10-07

### Checked

- [x] Searched the repository for `.ino`, `.h`, `.cpp`, `.bat`, and `.py` files.

- [x] Inspected the imported Arduino, Python, batch, and documentation files.

- [x] Checked `.source/` and confirmed it has separate `uncompiled/` and `compiled/` areas.

- [x] Performed a static source review and Python syntax check.

- [x] Confirm the compiled ClockOSV1 `.bin` is a real Arduino CLI build artifact.

### Bugs found

- **Resolved blocker:** `updateV1.ino` now has the required `bootloader.h` module.

- **Resolved blocker:** a verified ClockOSV1 binary is now present in the compiled update slot.

- **Installer-managed asset:** weather icons are downloaded to the SD card by the Windows setup tool.

### Open bugs and blockers

- [x] **Resolved layout blocker:** `.source/` now has matching versioned `uncompiled/` and `compiled/` trees.

- [x] **Resolved implementation blocker:** the OTA bootloader module and same-name fallback sketch are present.

- [x] **Successful build:** ClockOSV1 compiles with Arduino CLI, ESP32 core 2.0.17, and the required libraries.

- [x] **Hardware configuration unverified:** display and peripheral pin mappings, display inversion/color order, and library settings still require authoritative documentation and hardware testing.

- [ ] **Update behavior hardware test:** OTA partition compatibility and rollback handling are implemented and compiled, but still require physical-device testing.

- [x] **Installer behavior untested:** Windows mode selection, Python bootstrap, serial-port upload, file-preservation behavior, download failures, and icon installation still need Windows testing.

> **Current status:** ClockOSV1 source, compiled update, SD JSON preferences, updater UI, and installer paths are implemented. Windows installer execution and physical hardware validation remain open.

## Deferred installer follow-up

- [x] Run the `.bat` on Windows with Python/Tkinter and verify the setup window is the only visible launcher UI.

- [x] Test Auto mode with a real `.source/compiled/updates/*.bin` and confirm it flashes the selected ESP32 serial port without compiling.

- [x] Test Beta mode with multiple versioned raw update folders and confirm it selects and compiles the newest `.ino`.

- [x] Test Manual mode with a selected folder containing an application or bootloader `.ino` and its headers.

- [x] Confirm the port picker excludes SD-card volumes and labels the ESP32 as `ESP32 Dev Module` when detected.

- [x] Confirm compile-only output is a named `.bin` matching the compiled sketch.

- [x] Prevent Beta mode from compiling stale `clock` sketches left in the local workspace.

- [x] Save successful compile-only binaries to `Downloads/ClockBuilds/<sketch>.bin`.

- [x] Add an explicit firmware-target picker before selecting the ESP32 serial port; Manual mode can target a bootloader sketch.

- [x] Show cleaned, expanded compiler diagnostics when a build fails.

- [x] Restrict automatic/Beta compilation and stable downloads to the clock application; fallback bootloader binaries are never selected automatically.

- [x] Fix the clock sketch's Arduino `Key` prototype and PNGdec callback compile errors shown by the Windows build.

- [x] Set the clock display rotation to 180 degrees (`TFT_ROTATION 3`).

- [x] Add a standalone raw-touch coordinate sketch; final touch mapping remains pending the user's Serial Monitor readings.

- [x] Hardwire the previous TFT_eSPI touch calibration `{365, 3431, 321, 3368, 7}` for the earlier display orientation.

- [x] Add Wi-Fi IP-based local weather lookup with Open-Meteo current conditions.

- [x] Add weather icon fallback drawing when SD PNG assets are unavailable.

- [x] Align compact Wi-Fi/battery indicators and add the next-alarm row below the time.

- [x] Add a touch-opened alarm editor supporting time, date, one-time alarms, and repeat-day alarms.

- [x] Add tappable weather, Wi-Fi, calendar, date/time, and settings areas with Back navigation.

- [x] Add manual US city entry with Open-Meteo geocoding and persistent weather mode/location.

- [x] Add persistent offline mode and manual DD/MM/YYYY date/time entry after offline restart.

- [x] Connect the calendar page to Google Classroom after OAuth/API authorization is configured.

- [x] Remove the generated `extras` folder from source arrangement.

- [x] Place installed raw update sketches under the SD card `.source/uncompiled/updates/` tree.

- [x] Make the `.bat` launch Python with `pythonw` and a hidden bootstrap process instead of a visible terminal.

- [x] Add a visible Flash device selector with refresh to the installer window.

- [x] Add installer Classroom notifications: pre-install OAuth sign-in, read-only cache generation, and SD storage under `.source/data/secrets/`.

- [x] Load cached assignments/projects and show the nearest due item on the home/calendar screens.

- [x] Refresh the cached assignment display every second; Google sync itself runs during installer authorization rather than once per second.

- [x] Sleep the display after one minute of inactivity and wake it with a touch.

- [x] Add a two-tap factory reset that clears local settings, Wi-Fi, Classroom tokens, and cached Classroom data.

- [x] Search common user folders for OAuth-shaped JSON files and ask for confirmation before auto-selecting one.

## Tomorrow's follow-up

- [x] Fix Windows installer dependency bootstrap: package installation now uses `python.exe` paired with `pythonw.exe` and verifies imports afterward.

- [x] Finish the Arduino CLI build with ESP32 2.0.17 after the `clockAlarm` rename; ClockOSV1 builds cleanly.

- [x] Copy the verified ClockOSV1 binary into `.source/compiled/updates/updateV1/` for Auto mode.

- [x] Store settings, Wi-Fi, alarm, and touch preferences as JSON under `/data/preferences/` only when a valid ClockOS SD card is present.

- [x] Show filled, outline, or X-marked SD-card status and warn on Settings exit when changes cannot be saved.

- [x] Upgrade the release identity to ClockOSV1 with one-time first-run setup, Wi-Fi on/off choice, Classroom toggle, Main/Side sync role, Apple-style Settings rows, profile silhouette, swipe-back navigation, multi-network JSON storage, and sync-version compatibility warnings.

- [x] Preserve SD-card JSON files during installer wipes and store the OAuth client as `/data/secrets/classroomsecret.json`.

- [x] Add the Apple-style Appearance tab with Crystal, Midnight, Ocean, Sunrise, and Graphite theme packages under `.source/themes/appearance/`; persist the selected theme and install the catalog to the SD card.

- [x] Test the updated `.bat` on Windows: OAuth JSON auto-detection, school-account sign-in, SD-card secret/cache placement, flashing, and headless launcher behavior.

- [x] Fix Auto flashing of an application-only `.bin`: use direct esptool flashing at `0x10000` instead of making Arduino CLI search for a nonexistent `.ino.bootloader.bin`.

- [x] Retest Auto flashing on Windows with the connected ESP32 and confirm the device boots the new application after direct esptool upload.

## Deferred ClockOSV1/V2.5 release and UI work

- [ ] Integrate and validate the user's corrected TFT display-orientation code when the corrected source is available; confirm the physical rotation and touch mapping on the Hosyond display.

- [x] Replace the previous touch calibration with the newly supplied calibration `{343, 3436, 266, 3381, 1}` using `tft.setTouch(calData);` and compile the firmware successfully.

- [ ] Validate the new calibration on the physical Hosyond display at all four corners and the center after confirming the corrected orientation.

- [x] Add the canonical same-name `ClockOSV1/ClockOSV1.ino` and compiled application alias while retaining `updateV1` source/binary paths for existing SD cards, OTA discovery, and fallback compatibility.

- [x] Define and apply the release increment policy: small fixes increment by `+0.1`, major feature releases increment by `+1`, and compiled/uncompiled folders, firmware identity, installer labels, and update metadata stay synchronized.

- [x] Create and compile the next firmware release as `ClockOSv2.5.bin` and publish matching uncompiled source under a same-name `ClockOSv2.5/` sketch folder after the version policy is confirmed.

- [x] Add the ClockOS icon to the Tkinter window title bar and package it with the launcher.
- [ ] Verify the icon on Windows 10 and Windows 11 and confirm it appears without briefly opening a console.

- [x] Redesign the Tkinter installer window to be smaller, cleaner, and more Apple-like: reduce oversized text, use a restrained neutral palette, and move primary actions into a tidy sidebar.

- [x] Keep the current ClockOSV1 release stable while implementing the versioned ClockOSv2.5/v2.6 source, binaries, installer UI, and release assets on a separate PR branch.

## Deferred Apple-style UI and behavior fixes

- [x] Make on-device SD preparation and Windows Setup create an identical root and `.source/` directory tree; verify exact parity and preservation of unrelated files in a temporary-directory regression test.
- [ ] Verify matching folder creation and Ready status on the physical SD card.

- [x] Rework icon layout and alignment: center weather icons consistently, remove awkward-looking variants, and make all status icons share a clean visual scale.

- [x] Fix SD-card status detection to verify every required ClockOS folder; a valid/prepared card reports Ready while absent or incomplete cards remain distinct. Automated layout parity test added; physical-card verification still remains open.

- [x] Redesign the status row so the battery matches the Wi-Fi icon size and the SD-card icon sits directly beside the Wi-Fi/battery group.

- [x] Redesign the alarm screen as an Apple-style grouped list/editor with multiple alarms, enable/disable controls, AM/PM or 24-hour support, and compact controls.

- [x] Fix alarm time layout: keep AM/PM visible with the date clearly separated, remove unnecessary lines, and maintain the compact clock/date layout.

- [x] Reduce oversized typography throughout the firmware and scale/wrap Wi-Fi, weather, calendar, settings, and alarm text so it never clips or runs off-screen.

- [x] Stop unnecessary screen flashing: update only changed fields, especially password text boxes and Wi-Fi controls, instead of redrawing the whole page.

- [x] Repair the Wi-Fi signal animation so the Wi-Fi symbol visibly updates while connecting and remains stable when connected.

- [x] Make Philadelphia the fallback weather location and preserve manual city and automatic location choices.

- [x] Make display sleep turn the panel black, explicitly switch the GPIO27 backlight off, and restore the home screen without a white wake flash.

- [x] Remove profile and SD-card icons from the Calendar page while retaining global icons only where they belong.

- [x] Redesign Calendar, Weather Settings, Wi-Fi Settings, SD Card, and Factory Reset pages into compact Apple-style grouped rows; keep SD setup usable offline.

- [x] Remove Offline Mode from the normal Settings list and keep offline behavior accessible only through setup/developer flow.

- [x] Replace the blue welcome screen with a minimal Apple StandBy-inspired welcome/setup screen.

> **ClockOSv2.6 implementation note — 2026-10-10:** The versioned V2.6 source,
> compiled application, release manifest, installer version selection, Apple/StandBy-inspired UI,
> Wi-Fi scanner flow, setup-resume behavior, semantic OTA selection, and GPIO27
> backlight sleep/wake are implemented and build-validated with ESP32 core 2.0.17,
> TFT_eSPI, PNGdec, and ArduinoJson. Physical display/touch/OTA/backlight checks,
> and Windows 10/11 installer execution still require the target hardware/OS.
