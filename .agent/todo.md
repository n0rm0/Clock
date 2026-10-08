# Clock Project TODO

This checklist reflects the requirements in [`agent.md`](agent.md) and the files actually present in the repository. A checked item means it has been completed and validated; historical chat claims are not treated as implementation evidence.

## Agent workspace

- [x] Create the project-local `.agent/` directory.
- [x] Create and maintain this checklist.
- [x] Create [`agent.md`](agent.md) with project conventions and validation rules.
- [x] Confirm the default branch is `main`.
- [x] Inspect the repository before making implementation changes.

## Arduino firmware

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

- [ ] Create `.source/data/` and `.source/icons/`.
- [ ] Create versioned update folders at `.source/updates/updateV<version>/`.
- [ ] Store each update sketch as `updateV<version>.ino`.
- [ ] Create the fallback bootloader path at `.source/bootloader/fallback/bootloader/bootloaderV<version>/`.
- [ ] Store the fallback bootloader sketch as `bootloaderV<version>.ino`.
- [ ] Implement newest-compatible-update selection.
- [ ] Preserve a fallback image before an update installation.
- [ ] Return to the previous working firmware when both update and fallback installation fail.
- [ ] Verify OTA partition requirements and document that this is application-level update logic, not a replacement for the ESP32 ROM bootloader.

## Windows SD-card setup tool

- [ ] Add a `.bat` launcher backed by Python.
- [ ] Add a Tkinter drive selector.
- [ ] Create the required `.source` directory tree and versioned folders.
- [ ] Download or copy the weather icons into `.source/icons/`.
- [ ] Report missing Python, inaccessible drives, failed downloads, and failed copies clearly.
- [ ] Avoid deleting or overwriting existing SD-card files without an explicit documented opt-in.
- [ ] Test the tool using a disposable directory or removable test drive.

## Documentation and delivery

- [ ] Update `README.md` with setup, build, board, library, SD-card, and firmware-update instructions.
- [ ] Keep commits focused and describe the validation performed.
- [ ] Update this checklist only when work is genuinely complete.

## Code and bug check — 2026-10-07

### Checked

- [x] Searched the repository for `.ino`, `.h`, `.cpp`, `.bat`, and `.py` files.
- [x] Inspected all current project files: `README.md`, `.agent/agent.md`, and this checklist.
- [x] Confirmed there is no Arduino code available to compile or perform a source-level bug review on.
- [x] Confirmed the working tree was clean before this checklist update.

### Bugs found

No source-code bugs were found because no Arduino or setup-tool source exists in the repository yet.

### Open bugs and blockers

- [ ] **Missing implementation:** the firmware, support modules, bootloader, update sketches, icons, and setup tool described in the requirements are not committed.
- [ ] **No build target:** there is no Arduino project or board/library configuration, so compilation cannot currently be run.
- [ ] **Hardware configuration unverified:** display and peripheral pin mappings, display inversion/color order, and library settings still require authoritative documentation and hardware testing.
- [ ] **Update behavior untested:** OTA partition compatibility, fallback installation, version comparison, and rollback behavior have not been implemented or exercised.
- [ ] **Installer behavior untested:** drive selection, file-preservation behavior, download failures, and icon installation have not been implemented or tested.

> **Current status:** The checklist and bug audit are complete. Firmware and installer work remain open; no implementation should be marked done until source files exist and validation has been performed.
