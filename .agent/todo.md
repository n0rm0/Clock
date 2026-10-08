# Clock Project TODO

This checklist reflects the requirements in [`agent.md`](agent.md) and the files actually present in the repository. A checked item means it has been completed and validated; historical chat claims are not treated as implementation evidence.

## Agent workspace

- [x] Create the project-local `.agent/` directory.
- [x] Create and maintain this checklist.
- [x] Create [`agent.md`](agent.md) with project conventions and validation rules.
- [x] Confirm the default branch is `main`.
- [x] Inspect the repository before making implementation changes.

## Arduino firmware

- [x] Import the active TFT_eSPI clock sketch into `.source/uncompiled/updates/updateV1/`.
- [x] Remove duplicate preview files and the alternate LovyanGFX implementation from the published layout.
- [ ] Add one versioned primary Arduino entry sketch; do not keep duplicate clock sketches.
- [ ] Split supporting code into responsibility-named modules such as `display.h`, `weather.h`, and `bootloader.h`.
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
- [x] Keep the compiled update and fallback slots empty of firmware until builds pass.
- [x] Configure the clock to check `.source/compiled/updates/` for newer updates.
- [ ] Add `.source/data/` and `.source/icons/` as installer-generated SD-card folders.
- [ ] Add the bootloader/update implementation and place resulting binaries in the matching compiled slots.
- [ ] Implement newest-compatible-update selection.
- [ ] Preserve a fallback image before an update installation.
- [ ] Return to the previous working firmware when both update and fallback installation fail.
- [ ] Verify OTA partition requirements and document that this is application-level update logic, not a replacement for the ESP32 ROM bootloader.

## Windows SD-card setup tool

- [ ] Add a `.bat` launcher backed by Python.
- [ ] Add a Tkinter drive selector.
- [ ] Create the required SD-card data, icon, and compiled-binary directories.
- [ ] Download or copy the weather icons into `.source/icons/`.
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

- **Confirmed blocker:** `.source/uncompiled/updates/updateV1/clock.ino` includes `bootloader.h`, but `bootloader.h` was not present in the uploaded archive and is not in the repository.
- **Confirmed blocker:** no compiled `.bin` files were included, so the mirrored `.source/compiled/` slots cannot be flashed.
- **Confirmed limitation:** the source references weather icons, but no icon assets were included in the archive.

### Open bugs and blockers

- [x] **Resolved layout blocker:** `.source/` now has matching versioned `uncompiled/` and `compiled/` trees.
- [ ] **Missing implementation:** the bootloader module and weather icons are still absent.
- [ ] **No successful build:** the Arduino project cannot compile until `bootloader.h` and the required libraries are supplied.
- [ ] **Hardware configuration unverified:** display and peripheral pin mappings, display inversion/color order, and library settings still require authoritative documentation and hardware testing.
- [ ] **Update behavior untested:** OTA partition compatibility, fallback installation, version comparison, and rollback behavior have not been implemented or exercised.
- [ ] **Installer behavior untested:** drive selection, file-preservation behavior, download failures, and icon installation have not been implemented or tested.

> **Current status:** Archive cleanup and layout work are complete. Firmware and installer work remain open; no implementation should be marked done until the missing module/assets are supplied and a real build passes.
