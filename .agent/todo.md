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
- [ ] Implement the 480 × 320 landscape home screen.
- [ ] Add the central seven-segment time display and abbreviated weekday.
- [ ] Add the date, indoor temperature, and humidity bottom row.
- [ ] Add the upper-right weather icon and outdoor temperature.
- [ ] Add Wi-Fi configuration without committing credentials, API keys, or private location data.
- [ ] Add SD-card weather-icon loading from `.source/icons/`.
- [ ] Confirm the Hosyond 4-inch ESP32-32E/ST7796S pin map against authoritative board documentation.
- [ ] Confirm the display library, board package, selected board, and required library versions.
- [ ] Build the firmware and resolve all compiler errors from actual compiler output.
- [ ] Validate the display, SD card, Wi-Fi/weather data, and sensors on physical hardware.

## SD-card and update layout

- [x] Create matching versioned trees under `.source/uncompiled/` and `.source/compiled/`.
- [x] Keep the compiled update and V1 fallback slots empty of firmware until builds pass; compiled may contain only `.bin` files.
- [x] Configure the clock to check `.source/compiled/updates/` for newer updates.
- [ ] Add `.source/data/` and `.source/icons/` as installer-generated SD-card folders.
- [x] Add the bootloader/update implementation and matching fallback source slot.
- [x] Implement newest-compatible-update selection from `.source/compiled/updates/`.
- [x] Preserve the current OTA slot by aborting failed writes before reboot.
- [x] Keep the current firmware running when an update fails; hardware rollback testing remains open.
- [ ] Verify OTA partition requirements and document that this is application-level update logic, not a replacement for the ESP32 ROM bootloader.

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
- [ ] Report missing Python, inaccessible drives, failed downloads, and failed copies clearly.
- [ ] Avoid deleting or overwriting existing SD-card files without an explicit documented opt-in.
- [ ] Test the tool using a disposable directory or removable test drive.

## Documentation and delivery

- [x] Update `README.md` with the current source/compiled layout and known build limitation.
- [ ] Keep commits focused and describe the validation performed.
- [ ] Update this checklist only when work is genuinely complete.

## Code and bug check — 2026-10-07

### Checked

- [x] Searched the repository for `.ino`, `.h`, `.cpp`, `.bat`, and `.py` files.
- [x] Inspected the imported Arduino, Python, batch, and documentation files.
- [x] Checked `.source/` and confirmed it has separate `uncompiled/` and `compiled/` areas.
- [x] Performed a static source review and Python syntax check.
- [x] Confirmed no `.bin` files were supplied and no fake binaries were created.

### Bugs found

- **Resolved blocker:** `updateV1.ino` now has the required `bootloader.h` module.
- **Confirmed blocker:** no compiled `.bin` files were included, so the mirrored `.source/compiled/` slots cannot be flashed.
- **Confirmed limitation:** the source references weather icons, but no icon assets were included in the archive.

### Open bugs and blockers

- [x] **Resolved layout blocker:** `.source/` now has matching versioned `uncompiled/` and `compiled/` trees.
- [x] **Resolved implementation blocker:** the OTA bootloader module and same-name fallback sketch are present.
- [ ] **No successful build:** Arduino compilation has not yet been run with the ESP32 board package and required libraries.
- [ ] **Hardware configuration unverified:** display and peripheral pin mappings, display inversion/color order, and library settings still require authoritative documentation and hardware testing.
- [ ] **Update behavior untested:** OTA partition compatibility, fallback installation, version comparison, and rollback behavior have not been implemented or exercised.
- [ ] **Installer behavior untested:** Windows mode selection, Python bootstrap, serial-port upload, file-preservation behavior, download failures, and icon installation still need Windows testing.

> **Current status:** Structure, bootloader source, and installer paths are implemented. A real Arduino build, Windows installer test, icon download test, and hardware validation remain open.

## Deferred installer follow-up

- [ ] Run the `.bat` on Windows with Python/Tkinter and verify the setup window is the only visible launcher UI.
- [ ] Test Auto mode with a real `.source/compiled/updates/*.bin` and confirm it flashes the selected ESP32 serial port without compiling.
- [ ] Test Beta mode with multiple versioned raw update folders and confirm it selects and compiles the newest `.ino`.
- [ ] Test Manual mode with a selected folder containing an application or bootloader `.ino` and its headers.
- [ ] Confirm the port picker excludes SD-card volumes and labels the ESP32 as `ESP32 Dev Module` when detected.
- [ ] Confirm compile-only output is a named `.bin` matching the compiled sketch.
