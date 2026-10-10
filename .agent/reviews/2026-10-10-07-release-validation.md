# Release and non-hardware validation review

**Date:** 2026-10-10
**Review ID:** 07 — release validation
**Repository:** `/home/ubuntu/Clock`
**Reviewed commit:** `781759577efc713135b03ec30dd4b6b767a32ffa` (`main`)
**Current application:** `ClockOSv2.8`
**Scope:** versioned source/compiled paths, release metadata, automated tests, Arduino CLI evidence, and truthful readiness/TODO reporting for a firmware UI overhaul. This is not a UI redesign or a hardware certification.

## 1. Readiness verdict and review boundaries

> **The repository has a consistent v2.8 application artifact and seven passing non-hardware regression tests. It does not establish that an additional UI overhaul has been compiled, that the published image is reproducibly derived from the exact current source, or that physical display/touch/SD/OTA behavior and native Windows execution have been validated.**

**Observed in this review:**

- The current and versioned v2.8 manifests are byte-identical. Their application path, byte count, and SHA-256 match the actual binary.
- All **seven existing `unittest` tests passed** in this Linux sandbox, in 0.161 seconds.
- A separate read-only audit confirmed current **v2.8**, not only historical v2.7, has the same 18-directory SD layout specification as the installer.
- Installer source/binary selection, exercised with a mocked local repository tree, selects v2.8. A small synthetic check confirms v2.10 sorts above v2.9 and the default target excludes `bootloaderV99` when clock application choices exist.
- Python syntax checks succeeded for `install.py` and both test files, without importing Tkinter or creating repository bytecode.
- The published image contains `v2.8` and the six Settings strings checked by the existing release test.
- **Arduino CLI, esptool, and Tkinter are not available in this sandbox. No Arduino compile, esptool image-validation command, Windows GUI execution, device flashing, or physical test was performed by this review.**

**Recommended disposition:** retain v2.8 as a **locally build-recorded, artifact-integrity-checked release candidate**, not as hardware-validated or provenance-grade reproducible firmware. Treat the next overhaul as a new, separately validated artifact. Do not carry the seven-test pass or the v2.8 binary forward as evidence for changed source.

The five supplied inputs were read: `.agent/todo.md`, `.agent/agent.md`, `README.md`, `ClockOSv2.8.ino`, and `config.h`. Additional inspection was limited to release policy/manifests/build records, the current `bootloader.h`, relevant installer functions, the two test files, source/binary slot inventories, and legacy release packaging notes. Other agents' UI reports were not used as implementation or validation evidence.

**Change boundary:** no application source, manifest, TODO, binary, test, index, branch, or commit was modified. The only repository file written by this review is this report. Scratch audit code and logs are under `/home/ubuntu/jobs/d63908cbc547_a6/`.

## 2. Evidence actually obtained

| Evidence | Result | What it establishes | What it does not establish |
| --- | --- | --- | --- |
| `python3 -B -m unittest discover -s tests -p 'test_*.py' -v` | 7 tests, all passed | Existing host-side regression assertions pass against this checkout | Firmware execution, visual correctness, touch accuracy, Windows behavior |
| v2.8 manifest comparison | `current.json` equals `ClockOSv2.8.json`, including exact bytes | Current pointer and versioned metadata agree | Release-channel isolation or artifact authenticity |
| v2.8 binary size/hash | 1,135,728 bytes; SHA-256 matches both manifests | Local file integrity relative to metadata | Source-to-binary provenance, compiler success in this session, physical boot |
| Versioned v2.5–v2.8 manifest audit | All four recorded sizes/hashes and same-name source paths match | Archived application metadata is internally consistent | Historical native/hardware tests or automatic rollback |
| Current SD list audit | 18 entries, identical order after resolving `config.h` path macros | v2.8 and installer declare the same directory specification | ESP32 `SD.mkdir`/directory behavior on an actual card |
| Installer mocked local-tree selection | Current source and binary selected; v2.10 outranks v2.9 | Relevant Python selection functions work for those fixtures | Live GitHub response, interrupted download, upload, or all malformed-feed cases |
| Published image markers | `v2.8`, `Settings`, `General`, `Appearance`, `Storage`, `Factory Reset`, `Choose Network` present | Expected strings occur somewhere in the image | Correct pages, hit targets, glyphs, drawing order, or changed UI code actually executing |
| ESP image header inspection | Magic `0xe9`, five segments | Header is consistent with an ESP application image | Valid image checksum/hash; this is **not** esptool validation |
| Read-only Git inspection | Tracked and staged diffs empty; branch `main` | No tracked implementation changes were introduced by this review | A clean untracked workspace: other review reports were already present |

Evidence files:

- `/home/ubuntu/jobs/d63908cbc547_a6/evidence/unittest.log`
- `/home/ubuntu/jobs/d63908cbc547_a6/evidence/release-static.log`
- `/home/ubuntu/jobs/d63908cbc547_a6/audit_release.py` — scratch-only, read-only local audit; no network, GUI, or flashing.

The audit script was corrected during its construction to report, rather than incorrectly assume, identical legacy alias images and retention of an unused identity string. These are reviewer-script corrections, **not repository test fixes**. The final audit succeeds while recording both observations below.

**Image-identity limitation:** the complete string `ClockOSv2.8` does not occur in the published image, although `v2.8` and the tested Settings markers do. `CLOCKOS_FIRMWARE` is declared but not referenced by the inspected application. Removal of an unused string during optimization is consistent with this result; it is not evidence of a wrong or corrupt image. Use exact source constants, build provenance and an intentionally retained application descriptor if a full binary identity is required, not an assumption that every source string survives linking.

### Current source fingerprint

These hashes identify the reviewed inputs; the existing manifest does not bind them to its binary.

| File | SHA-256 |
| --- | --- |
| `ClockOSv2.8/ClockOSv2.8.ino` | `973bb32ff18092cd08a56edd75215c7ba577c1d9e9bd1a10d6d689961b3f5957` |
| `ClockOSv2.8/config.h` | `dd4e36171b88d5848b8f737f4220e0a0adb71922ff50777a4856dd640157b4d5` |
| `ClockOSv2.8/bootloader.h` | `dc38a02376fd6a54975acb84513891cb5a2278d87422107298b2723b26235d65` |
| `.source/install/install.py` | `150972b396e788e30d2b81c0ab3240ac7a6dbc3ff534ad72106e0c92ee76ce6c` |

## 3. Versioned paths, release identity, and publication consequences

### 3.1 Current v2.8 application identity is consistent

**Observation:** the following existing constants and metadata agree:

- `CLOCKOS_NAME = "ClockOS"`
- `CLOCKOS_VERSION = "v2.8"`
- `CLOCKOS_FIRMWARE = "ClockOSv2.8"`
- `SYNC_PROTOCOL_VERSION = "2.8"`
- Installer `DEFAULT_RELEASE` has matching product/version/identity/sketch/source/binary fields. `load_release_manifest()` fills `PRODUCT_NAME`, `PRODUCT_VERSION`, `FIRMWARE_IDENTITY`, and `FLASH_SKETCH` from release metadata for labeling/defaults.
- `.source/uncompiled/updates/ClockOSv2.8/ClockOSv2.8.ino` exists with `config.h` and `bootloader.h` in the same folder.
- `.source/compiled/updates/ClockOSv2.8/ClockOSv2.8.bin` is the application payload referenced by both manifests.

All seven sketches under `.source/uncompiled/` have a same-name folder/entry sketch. All six tracked compiled files are `.bin` application payloads. The `updateV1/ClockOSV1.bin` basename exception is explicitly documented as a legacy compatibility slot; it must not be “cleaned up” by deleting or renaming the old path.

**Recommendation:** make the next release gate validate the entire identity tuple, not merely whether `"v2.8"` appears in source. Require one canonical entry `.ino`, supporting headers, valid manifest paths, consistent installer fallback labels, and the application-only filename. Preserve existing legacy paths and historical payloads.

### 3.2 The archive does not literally mirror an empty compiled fallback slot

**Observation:** `.source/uncompiled/bootloader/fallback/bootloaderV1/` exists with three source files. `.source/compiled/bootloader/fallback/bootloaderV1/` **does not exist in this checkout**, although README tree diagrams show it. The firmware/installer SD directory specification creates the generic compiled bootloader/fallback directories, not that version-specific slot. Git does not preserve empty directories by itself.

**Recommendation:** document the fallback slot as source-only/unbuilt rather than imply a downloadable fallback `.bin` exists. Do not add dummy firmware, a fabricated binary, or a non-binary placeholder inside `compiled/`. If maintaining an empty slot is a packaging requirement, create it in the installer/package-generation step and test that separately; it is not proof of a fallback build.

### 3.3 Legacy aliases are source-identical but not byte-identical binaries

**Observation:** the `updateV1.ino` and `ClockOSV1.ino` source files are byte-identical, as are their support headers. Their binary files have the same size, **1,137,568 bytes**, but different SHA-256 values:

- Legacy slot: `31248105393498055a07e6edce2912f88792d1d57c4b4b5231c001e72b357bbb`
- Canonical slot: `a0b1b7d7b0eaff5974f7b16e0d167d8c449fc9d1ae768fb0943e00e46b929cbb`

A byte comparison found only 64 differing bytes, at the embedded digest region near offset 176 and at the final image digest. This is compatible with build/ELF metadata differences; it is **not proof of corrupt firmware or different behavior**. Neither identical runtime behavior nor provenance can be certified by that comparison alone.

**Recommendation:** preserve both existing artifacts. Describe them as compatibility-path builds unless a recorded build comparison establishes stronger equivalence. Avoid a blanket byte-equality assertion for independently built sketch aliases. For future aliases intended to be exact copies, generate both from one verified application artifact and record both path/hash entries at publication time.

### 3.4 “Current release candidate” is already an automatic update candidate

**Observation:**

- Installer `latest_source_path()`, `latest_binary()`, and `default_flash_target()` choose the maximum version from available application paths, independent of the current manifest target.
- `version_key()` uses integer tuples, correctly ordering v2.10 above v2.9.
- `BL::versionNumber()` packs major/minor/patch components; `BL::newestCandidate()` chooses the maximum encoded version, then lexicographic path on ties.
- `BL::findNewest()` reads the recursive GitHub `main` tree. `BL::installCandidate()` downloads the chosen file from `main`.
- There is no stable/beta channel field consulted by the firmware. Moving `current.json` to an older release does **not** stop a higher-version `.bin` in the update tree from being selected.

Thus keeping ClockOSV1 on disk does not make Auto mode use it, and calling v2.8 a release candidate does not isolate it from OTA once published on `main`.

**Recommendation:** preserve the current maximum-version selection behavior. Keep all draft build outputs in scratch/CI artifacts or on an unpublished development branch. Treat introduction of a higher-version application `.bin` on `main` as the deployment event. Publish source, verified application binary, metadata, release record, and tests together only after the intended gates pass. A future channel mechanism would be a separate behavior change, not part of this review's preservation-oriented recommendation.

### 3.5 Manifests are a record, not enforced download-integrity/compatibility policy

**Observation:** both v2.8 manifests use `schemaVersion: 1`, `binaryBytes`, and `sha256`; those values are accurate locally. However:

- `load_release_manifest()` validates labels/paths but does not consume `binaryBytes` or `sha256` for the download.
- `download_latest_binary()` saves bytes returned by `http_get()` without manifest size/hash verification.
- `BL::Candidate` contains `path`, `version`, and `found`; it has no board, partition, channel, size, digest, or signature fields.
- `BL::isCandidate()` accepts any lowercase `.bin` beneath `GH_UPDATES_DIR`; it is broader than the installer's supported application-folder filter.
- `BL::installCandidate()` uses `Update.begin()`, `Update.writeStream()`, `Update.end(true)`, `Update.isFinished()`, and failure `Update.abort()`. This is application update validation/failure handling, not manifest authentication or a implemented signed-recovery mechanism.
- `BL::checkAndInstall()` compares against `BL::installedPath()` on the SD card, not `CLOCKOS_VERSION`. With missing installed-path metadata, it does not prove a candidate is newer than the currently executing application.

**Recommendation:** make publication checks reject unexpected binary names, nested helper binaries, malformed versions, or non-application artifacts. Publish **only the application image**, never generated `.ino.bootloader.bin`, `.ino.partitions.bin`, or merged/full-flash images under `updates/`. This protects the existing selector behavior without changing it. Document local hash verification separately from runtime integrity/authentication. If runtime compatibility, digest checking, same-version reinstall prevention, or signed recovery is required, give it a separate implementation/test scope and do not close it merely because manifest hashes match.

### 3.6 Stable terminology and increment policy need an explicit decision

**Observation:** README calls **ClockOSV1** the stable legacy release, while v2.8 `previousStableRelease` is `ClockOSv2.7`. The v2.7 build record still leaves physical SD/status checks open. The field name therefore conveys a stronger status than the supplied validation establishes.

The release policy specifies small fixes `+0.1` and major feature releases `+1.0`, with the example v2.7 → v3.7. At the present baseline, that means **v2.9 for a small fix** or **v3.8 for a major release under the stated example**, not an automatically assumed v3.0. The semantic selection code treats components as integers, so the later v2.9 → v2.10 transition must be made explicit rather than performed with floating-point arithmetic.

**Recommendation:** classify the overhaul before changing any identity. Clarify whether `previousStableRelease` means last built release or hardware-accepted stable release; either correct the value/status or explicitly redefine the field in documentation. Preserve existing behavior and `SYNC_PROTOCOL_VERSION`/`syncVersionCompatible()` policy unless the release deliberately changes compatibility. Do not silently reset the minor component or invent stable acceptance.

## 4. Arduino CLI build evidence: what exists and what is missing

### Existing recorded build

`.source/releases/ClockOSv2.8.md` records:

| Property | Recorded value |
| --- | --- |
| Board/FQBN | `esp32:esp32:esp32:PartitionScheme=min_spiffs` |
| Arduino CLI | `1.5.2-rc.1` |
| ESP32 core | `2.0.17` |
| TFT_eSPI | `2.5.43` |
| PNGdec | `1.1.6` |
| ArduinoJson | `7.4.3` |
| Program usage | 1,129,157 / 1,966,080 bytes, 57% |
| Dynamic memory | 96,352 / 327,680 bytes, 29% |
| Application image | 1,135,728 bytes |
| Application SHA-256 | `682afcb79c9d5bb88a8dba585c811f0ead8b854838bbee6f4ef4c6339834d5ec` |

**Observation:** the record openly states that the CLI distribution/binary hash is not recorded and the release is not provenance-grade reproducible. No tracked full compiler log, toolchain lock, CI workflow, source hash binding, ELF, generated partition artifact, or retained esptool output was found. Actual image size/hash match the record, but the historical compiler and esptool invocations were not repeated here.

`CORE` is pinned in the installer, while `LIBS = ["TFT_eSPI", "PNGdec", "ArduinoJson"]` is unversioned. `find_cli()` may use an installed Arduino IDE CLI or a moving `arduino-cli_latest_Windows_64bit.zip`. Therefore current Beta/Manual behavior does **not** guarantee the versions in the v2.8 build record.

**Recommendation:** separate the documented release build from the installer’s convenience build. A release build must pin the CLI distribution and verify its supplied checksum, ESP32 core, all three library versions, relevant Python/build dependencies, FQBN, and the exact `TFT_FLAGS`. Save command, environment, dependency inventory, complete compiler output and exit status, source hashes/commit, ELF, partition CSV/binary, application image, image-info output, size, and hash outside the application OTA feed. A moving CLI/library install is not acceptable evidence of reproducing this binary.

### Partition and application-only flashing boundary

`FQBN` selects `min_spiffs`, documented in `install.py` as two OTA application slots. `flash_binary()` writes the application-only payload at `0x10000`. This assumes a compatible existing bootloader/partition layout; it does not install or certify that layout on a device.

The present image is below the recorded 1,966,080-byte app-slot limit by **830,352 bytes**. Program-usage bytes and `.bin` bytes differ; record both and validate the actual image against both OTA slot sizes. Static RAM use also excludes runtime allocations such as the `TFT_eSprite`, JSON documents, strings, HTTP/TLS buffers, and SD/PNG operations.

**Recommendation:** inspect the installed core's `min_spiffs.csv` and generated partition payload in the non-hardware build gate. Confirm both `ota_0` and `ota_1` exist and fit the image and that the application offset matches `0x10000`. This closes a **build configuration** check only. Actual partition compatibility, initial flash, boot, interrupted update behavior, and recovery still require the target device.

## 5. Automated tests: accurate coverage and required additions

### Existing suite

`tests/test_installer_erase_and_release.py` contains five tests:

1. `test_explicit_erase_removes_mixed_files_and_recreates_layout` executes extracted `wipe_card_contents()` and `structure()` against a disposable directory.
2. `test_windows_metadata_is_the_only_wipe_exemption` checks the two Windows metadata root-name exemptions.
3. `test_delete_failure_is_reported_and_fails_closed` injects `PermissionError` into deletion and checks failure reporting.
4. `test_erase_is_explicit_and_update_only_is_forced_safe` checks **source strings** for erase-option/confirmation/update-only safeguards; it does not click dialogs or execute `go()`/`work()`.
5. `test_current_release_manifest_matches_firmware_image_and_ui` checks **only `current.json`**, hard-codes v2.8 source/binary names, checks image size/hash, and searches six UI strings in the binary.

`tests/test_sd_layout_parity.py` contains two tests:

- `test_firmware_and_windows_installer_share_sd_layout` resolves a firmware directory array and compares it to installer `CLOCKOS_SD_DIRECTORIES`.
- `test_windows_setup_creates_layout_without_erasing_existing_files` executes installer `structure()` on a temporary directory and preserves an unrelated file.

**Important observation:** `FIRMWARE_PATH` and `CONFIG_PATH` in the parity test are hard-coded to **ClockOSv2.7**, not v2.8. A v2.8 layout regression could pass this suite. The separate audit verified current v2.8 parity today; it does not remove that ongoing test gap.

### Recommended test changes for the authorized implementation phase

| Priority | Addition | Exact existing surfaces | Acceptance condition |
| --- | --- | --- | --- |
| P0 | Test the release being shipped | `current.json`, `FIRMWARE_PATH`, `CONFIG_PATH`, `CLOCKOS_SD_DIRECTORIES`, `ensureClockOsSdLayout()`, `clockOsSdReady()` | Resolve current source/config through validated manifest metadata; keep an explicit expected release identity at the promotion gate; retain legacy coverage separately |
| P0 | Full manifest/source/image consistency | `DEFAULT_RELEASE`, `CLOCKOS_NAME`, `CLOCKOS_VERSION`, `CLOCKOS_FIRMWARE`, `SYNC_PROTOCOL_VERSION` | Current/versioned metadata agree; paths are valid; exact constants and same-name sketch agree; binary size/hash match; required support files exist |
| P0 | Publishable application-tree invariant | Compiled update slots, installer `latest_binary()`, `BL::isCandidate()` | No helper/merged/bootloader image can enter the automatic feed; documented legacy exception remains accepted |
| P1 | Mocked source/binary selection | `is_application_sketch()`, `version_key()`, `latest_source_path()`, `latest_binary()`, `default_flash_target()` | v2.10 > v2.9, canonical/legacy ties, bootloader exclusion, same-name entry enforcement, empty tree, and manifest-not-pinning cases are exercised |
| P1 | Manifest fallback and hostile input fixtures | `_safe_release_text()`, `_safe_release_path()`, `load_release_manifest()` | Missing, malformed, partial and unsafe paths/labels do not create a mixed fallback identity; reset `_RELEASE_LOADED` between tests |
| P1 | OTA logic under host mocks | `BL::versionNumber()`, `BL::newestCandidate()`, `BL::checkAndInstall()`, `BL::installCandidate()`, `runUpdate()` | Exercise actual C++ logic with fake HTTP/SD/Update/time; distinguish bad response, `Update.begin` failure, short stream, `Update.end` failure, success and installed-path cases; no simulated reboot on failure |
| P1 | Orchestration safety, not text presence alone | `safe_to_wipe()`, `work()`, nested `go()`/`poll()`, `flash_binary()`, `build_sketches()`, `save_named_binaries()` | Mock OS/network/process/GUI boundaries; update-only never erases/flashes, rejected drives never erase, failed wipe stops copying, retry unlocks controls, app-only Auto command remains at `0x10000`, named output selected correctly |
| P1 | UI-overhaul non-hardware regression evidence | `showSettingsPage()`, `drawSettingsCard()`, `drawSettingsOption()`, `drawSettingsToggle()`, `showHome()`, `loop()`, `readTouch()` | A recording TFT/touch/time harness executes the relevant real drawing/state logic, checks 480 × 320 bounds and touch/state transitions; label it simulated, not physical rendering |

Do not satisfy these by duplicating production algorithms in tests. AST-extracted pure Python helpers are appropriate where already used; C++ tests should execute the existing functions under stubs or an explicitly factored, behavior-preserving interface. Source substring tests and binary strings remain useful smoke checks, but cannot certify UI geometry, free-font glyph availability, touch reachability, partial redraws, or no wake flash.

Keep all current erase semantics: normal preparation preserves unrelated files; destructive erase remains a separate option automatically selected when SD install is enabled, can be unchecked, requires two confirmations, rejects unsafe drives, retains only Windows-managed metadata, fails on incomplete deletion, and does not restore old JSON/secrets. These are release regression requirements, not redesign suggestions.

## 6. TODO truthfulness and sandbox closure matrix

The TODO introduction defines checked items as **completed and validated**. Several historical checks contradict its current open-status prose. A source implementation, a host test, a compiler result, native Windows execution, and physical-device validation must be separate checkboxes or separately named statuses.

### Items whose non-hardware evidence can be reaffirmed now

| TODO item/reference | Sandbox-supported disposition |
| --- | --- |
| Same-name sketches (line 23), source/fallback module presence (lines 25, 57, 127) | Source-layout/implementation presence verified. No fallback binary or hardware boot claimed |
| Versioned application layout / real compiled files (lines 49–51, 125) | Application slots exist and current image integrity is checked. Clarify absent compiled fallback slot and do not equate `.bin` presence with a fresh compile |
| Disposable-directory installer test (line 91) | Existing directory-level preparation/erase tests passed. Not a Windows/removable-drive execution claim |
| SD layout parity (line 254) | Existing test covers v2.7; independent current v2.8 declaration parity verified. Firmware filesystem operations are not physically validated |
| v2.8 size/hash and Settings markers (line 299) | Actual binary, both manifest hashes, size and tested markers verified. “Build a new” remains attributed to the existing build record, not a compile performed here |
| Seven regressions (line 300 and line 304 test clause) | Seven tests passed afresh; explicitly document source-string and historical-v2.7 coverage limits |
| Firmware constants for rotation/calibration (lines 165, 235 implementation clause) | `TFT_ROTATION 3`, `TOUCH_CAL_* = {343,3436,266,3381,1}`, and `ensureCalibration()` calling `tft.setTouch(calData)` confirmed in source; physical mapping and compile clauses are separate |
| Release policy and current identity (line 241) | Policy exists and v2.8 identity paths/default labels agree. Next release synchronization must be revalidated after changes |

These are predominantly already checked items. **None of the seven explicitly unchecked items can be fully closed by this review.**

**Sandbox-capable after additional work:** a pinned Arduino compile and offline esptool validation (once tools are provisioned), complete release/path/hash checks, Python/helper regressions, mocked OTA/selection/orchestration tests, simulated UI bounds/transitions, and an authoritative board-documentation comparison with cited model/revision sources can all be completed without physical hardware. The documentation-comparison subtask of lines 39–41 can therefore be closed from a sandbox after that research is actually performed; the physical board/panel clause cannot. These prospective closures are not evidence obtained in this review and do not close any combined hardware/native-Windows checkbox.

### Items still open, or requiring truthful reopening/splitting

| TODO item/reference | Why it cannot be closed here | Required evidence |
| --- | --- | --- |
| Physical firmware validation (line 45, currently checked) | No Hosyond hardware attached; current build record explicitly leaves hardware checks open | Dated board/firmware-specific display, touch, SD, weather/Wi-Fi and peripheral test record; reopen or split the hardware clause |
| Board documentation/config confirmation (lines 39–41); “Hardware configuration unverified” (line 131, checked despite wording) | Configuration comments identify a pin-map source but are not an authoritative-documentation comparison or a physical check | Cited board documentation with exact model/revision and configuration mapping; physical verification separately. Mark unverified work open |
| OTA hardware test (line 133, unchecked) | `Update.abort()` source review and app-slot build configuration do not test interrupted writes, reboot, installed partition layout, or recovery | Target-device normal/failure/power-loss tests. Preserve as open |
| “Installer behavior untested” (line 135, checked) | The text itself says Windows tests are needed; pure Linux helper tests do not validate Windows mode selection, bootstrap, upload or GUI | Split implementation/helper regressions from native Windows execution; reopen the latter |
| Historical Windows execution claims (lines 141–149, 225, 229) | No native Windows environment, serial ESP32, or dated execution logs supplied here | Preserve only as clearly attributed historical evidence if such records exist; otherwise reopen the execution claims, not the implemented functionality |
| Corrected orientation integration/validation (line 233, unchecked) | The supplied source retains rotation 3/calibration constants, but availability/integration of a separate corrected source is not established; physical confirmation unavailable | Corrected-source reference/diff plus compile record; final orientation/touch test on board |
| Physical calibration (line 237, unchecked) | Constant equality cannot establish the physical corners/center | Five-point test with board and firmware identity recorded |
| Windows 10/11 icon and console behavior (line 246, unchecked) | No Windows GUI or Explorer launch; `.bat` and `.vbs` behavior is platform-specific | Run both entry paths on both Windows versions; README already distinguishes unavoidable `.bat` console host from hidden `.vbs` launch |
| Physical SD folder creation/Ready (line 255, unchecked) | Host list parity is not `SD.begin`/directory/Ready behavior on FAT media | Physical card insertion/preparation/preservation/failure test |
| Physical Settings layout/touch (line 301, unchecked) | No display, touch controller, panel metrics, or actual free-font rendering | Hosyond layout/readability/tap/swipe matrix for the actual new image |
| Windows DPI and retry (line 302, unchecked) | Source `poll()` restores controls, but native event loop, DPI scaling and dialog placement are untested | Windows 10/11 DPI matrix and injected failing-job → TRY AGAIN → success sequence |
| Build claims (lines 43, 129, 211, 243, 299) | Historical records exist but neither Arduino CLI nor a pinned toolchain is installed here | Fresh compiler exit-success log against exact changed source; preserve historical wording as “recorded build” until repeated |
| UI absolutes (notably lines 267–275: “never clips”, visible animation, no flashing/white wake flash) | Neither seven tests nor image markers measure those outcomes | Simulated drawing/transition tests where possible plus separate physical rendering/animation/backlight tests |

The seven unchecked entries are at TODO lines **133, 233, 237, 246, 255, 301 and 302**. None is fully sandbox-closable. The orientation item's integration/build subtask could be completed later if the corrected source and toolchain are supplied; its hardware clause still cannot be closed here.

### Additional release-note truthfulness corrections

**Observed inconsistencies to correct in a later documentation change, without altering implemented behavior:**

- `.agent/agent.md` line 11 still says compiled binaries/weather icons are pending, despite published images and installer-managed icon downloads. Its intended indoor-temperature/humidity description conflicts with the TODO's no-added-indoor-sensor direction. Mark historical intent as historical rather than current acceptance criteria.
- The v2.8 source banner still says `clock.ino`, old `/.source/data/wifi.txt`/`touch.txt`, and “ClockOS v2.7”. Actual current preferences are JSON under `/data/preferences/`.
- `config.h` still describes weather as placeholders and remote updates as newest “by commit date”. `fetchWeather()` retrieves Open-Meteo conditions and both current selectors order version components, not commit timestamps.
- `UPDATES_DIR` says every downloaded `.bin` is retained on SD. The inspected `BL::installCandidate()` streams directly into `Update`; it records an installed **path**, not a downloaded SD binary. Do not claim SD binary retention from this updater.
- README and TODO lines 197–199 claim the cached assignment display refreshes every second. Current `loop()` ticks approximately every second, but calls `loadClassroomCache()`/`drawRightPanel()` for that refresh after **60,000 ms**, only while on Home. Installer Google sync is separate. Correct the description to actual behavior; do not change the cadence in this release-only task.
- README broadly says sleep occurs after one minute without touch; current `loop()` invokes `sleepDisplay()` for inactivity only when `screen == S_HOME`. Document the Home-only condition instead of implying every page sleeps.
- The current status sentence's historical “esptool.py confirms” should be linked to retained command output when available. This review confirms local SHA-256 but has not rerun esptool.
- `previousStableRelease`, preserved ClockOSV1 packaging, and Auto's highest-version behavior need consistent wording. Legacy launchers download current setup code; their V1 branding is not a firmware-channel pin.

## 7. Exact recommended non-hardware validation sequence

**The commands below are a recommended validation procedure. Only the host tests, scratch audit and source/metadata checks described in Sections 1–2 have already run. Toolchain provisioning, Arduino compilation, and esptool validation below have not run.**

Run all build/test output in scratch. Do not run `install.py` as an end-to-end shortcut: it can move Downloads files, modify workspaces, authorize accounts, erase drives and flash devices. Do not use `upload`, `write_flash`, `erase_flash`, physical-port discovery as acceptance evidence, or any command requiring a device.

### Step 1 — Freeze inputs and create an evidence location

```bash
set -euo pipefail
REPO=/home/ubuntu/Clock
WORK=/home/ubuntu/jobs/d63908cbc547_a6/validation
mkdir -p "$WORK/logs" "$WORK/tmp" "$WORK/arduino/data" \
  "$WORK/arduino/downloads" "$WORK/arduino/user" \
  "$WORK/build/ClockOSv2.8" "$WORK/export/ClockOSv2.8"
export PYTHONDONTWRITEBYTECODE=1
export TMPDIR="$WORK/tmp"
cd "$REPO"
GIT_OPTIONAL_LOCKS=0 git rev-parse HEAD | tee "$WORK/logs/commit.txt"
GIT_OPTIONAL_LOCKS=0 git status --short | tee "$WORK/logs/status-before.txt"
sha256sum .source/uncompiled/updates/ClockOSv2.8/* \
  .source/install/install.py tests/*.py > "$WORK/logs/source-hashes.txt"
```

**Gate:** identify the expected commit and source hashes. A report-only/untracked review file is not firmware change evidence. If reviewing the eventual new release, deliberately substitute its agreed identity/path in every step; do not keep v2.8 names or use its binary.

### Step 2 — Run the existing suite and independent current-artifact audit

```bash
python3 -B -m unittest discover -s tests -p 'test_*.py' -v \
  2>&1 | tee "$WORK/logs/unittest.txt"
python3 -B /home/ubuntu/jobs/d63908cbc547_a6/audit_release.py \
  2>&1 | tee "$WORK/logs/artifact-audit.txt"
```

**Gate:** tests pass, both manifests agree, path/name/constants/supports agree, actual image size/hash matches, current SD list matches the installer, and no helper images enter the application tree. The scratch audit is v2.8-specific; parameterize its production replacement for the next release. Add the Section 5 regressions in the authorized implementation phase and rerun the same test discovery command before promotion. A passing current suite alone is insufficient.

### Step 3 — Provision a pinned Linux toolchain in scratch, then record it

**Prerequisite, currently blocked:** place a reviewed Linux Arduino CLI `1.5.2-rc.1` binary at the path below and verify its distribution checksum against a separately supplied trusted checksum. The repository contains no such checksum. Do not invent one or silently replace it with a moving latest CLI. If the exact distribution cannot be obtained, record an approved toolchain change and produce a new build record; do not claim byte-for-byte reproduction of v2.8.

```bash
CLI="$WORK/toolchain/bin/arduino-cli"
test -x "$CLI"
"$CLI" version | tee "$WORK/logs/arduino-cli-version.txt"
sha256sum "$CLI" | tee "$WORK/logs/arduino-cli-binary-sha256.txt"

cat > "$WORK/arduino-cli.yaml" <<EOF
board_manager:
  additional_urls:
    - https://espressif.github.io/arduino-esp32/package_esp32_index.json
directories:
  data: $WORK/arduino/data
  downloads: $WORK/arduino/downloads
  user: $WORK/arduino/user
EOF

"$CLI" --config-file "$WORK/arduino-cli.yaml" core update-index \
  2>&1 | tee "$WORK/logs/core-index.txt"
"$CLI" --config-file "$WORK/arduino-cli.yaml" core install esp32:esp32@2.0.17 \
  2>&1 | tee "$WORK/logs/core-install.txt"
"$CLI" --config-file "$WORK/arduino-cli.yaml" lib install \
  TFT_eSPI@2.5.43 PNGdec@1.1.6 ArduinoJson@7.4.3 \
  2>&1 | tee "$WORK/logs/library-install.txt"
"$CLI" --config-file "$WORK/arduino-cli.yaml" core list \
  | tee "$WORK/logs/core-list.txt"
"$CLI" --config-file "$WORK/arduino-cli.yaml" lib list \
  | tee "$WORK/logs/library-list.txt"
"$CLI" --config-file "$WORK/arduino-cli.yaml" config dump \
  > "$WORK/logs/arduino-config.txt"
```

**Gate:** inspect version/inventory output for the recorded versions and retain download/checksum evidence, compiler/tool versions, OS and Python details. No unresolved dependency substitution is allowed. Network access for package acquisition is not hardware validation.

### Step 4 — Extract the installer’s actual TFT flags and perform a clean compile

```bash
TFT_FLAGS="$(python3 -B - <<'PY'
import ast
from pathlib import Path
p = Path('/home/ubuntu/Clock/.source/install/install.py')
tree = ast.parse(p.read_text(encoding='utf-8'))
nodes = [n for n in tree.body if isinstance(n, ast.Assign) and any(
    isinstance(t, ast.Name) and t.id == 'TFT_FLAGS' for t in n.targets)]
assert len(nodes) == 1
ns = {}
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(p), 'exec'), ns)
print(ns['TFT_FLAGS'])
PY
)"
printf '%s\n' "$TFT_FLAGS" > "$WORK/logs/tft-flags.txt"
FQBN='esp32:esp32:esp32:PartitionScheme=min_spiffs'

"$CLI" --config-file "$WORK/arduino-cli.yaml" compile \
  --fqbn "$FQBN" \
  --build-property "compiler.cpp.extra_flags=$TFT_FLAGS" \
  --build-path "$WORK/build/ClockOSv2.8" \
  --output-dir "$WORK/export/ClockOSv2.8" \
  --warnings all --verbose \
  "$REPO/.source/uncompiled/updates/ClockOSv2.8" \
  2>&1 | tee "$WORK/logs/compile.txt"
```

Use a fresh build/output directory for each independent build. `set -o pipefail` ensures `tee` does not mask a compiler failure. Capture full warnings; ArduinoJson compatibility/deprecation warnings should be documented rather than confused with a clean warning-free build. This command leaves source, tests and existing compiled slots unchanged.

**Gate:** compiler exit success, correct selected board/core/libraries/flags, new named `.ino.bin` or `.bin` present, program and static RAM usage recorded. Reject absent/stale output. `config.h` by itself does not configure TFT_eSPI's separately compiled display library; `TFT_FLAGS`, including `USER_SETUP_LOADED`, `ST7796_DRIVER`, HSPI and pin/font definitions, are required.

### Step 5 — Verify actual image and generated OTA partition configuration

```bash
IMAGE="$WORK/export/ClockOSv2.8/ClockOSv2.8.ino.bin"
if [ ! -f "$IMAGE" ]; then
  IMAGE="$WORK/export/ClockOSv2.8/ClockOSv2.8.bin"
fi
test -s "$IMAGE"
stat -c '%n %s bytes' "$IMAGE" | tee "$WORK/logs/image-size.txt"
sha256sum "$IMAGE" | tee "$WORK/logs/image-sha256.txt"

PARTITIONS="$WORK/arduino/data/packages/esp32/hardware/esp32/2.0.17/tools/partitions/min_spiffs.csv"
test -f "$PARTITIONS"
cp "$PARTITIONS" "$WORK/logs/min_spiffs.csv"
find "$WORK/export/ClockOSv2.8" "$WORK/build/ClockOSv2.8" \
  -type f \( -name '*.elf' -o -name '*partitions.bin' \) \
  -print | tee "$WORK/logs/generated-build-artifacts.txt"
```

Parse the saved CSV, verify `ota_0` and `ota_1`, check both application capacities against the actual image byte count, confirm application start `0x10000`, and retain the generated partition binary and ELF. Do not copy them into `compiled/updates/`.

For offline image parsing, provision esptool in a **scratch virtual environment** and retain its version/install output. One explicit validation-tool choice is esptool 4.5.1; using it does not assert it was the historical build tool:

```bash
python3 -m venv "$WORK/image-tools"
"$WORK/image-tools/bin/python" -m pip install esptool==4.5.1 \
  2>&1 | tee "$WORK/logs/esptool-install.txt"
"$WORK/image-tools/bin/python" -m esptool version \
  2>&1 | tee "$WORK/logs/esptool-version.txt"
"$WORK/image-tools/bin/python" -m esptool --chip esp32 image_info "$IMAGE" \
  2>&1 | tee "$WORK/logs/image-info-new.txt"
"$WORK/image-tools/bin/python" -m esptool --chip esp32 image_info \
  "$REPO/.source/compiled/updates/ClockOSv2.8/ClockOSv2.8.bin" \
  2>&1 | tee "$WORK/logs/image-info-published.txt"
```

**Gate:** successful image parsing with reported checksum/validation hash validity, appropriate ESP32 application header and sizes. SHA-256 matching a manifest and esptool's embedded image-validation hash are **different checks**. Neither proves a signature, device boot, panel pin correctness or installed partition layout.

### Step 6 — Establish source/build provenance and compare, without overwriting

```bash
sha256sum "$IMAGE" \
  "$REPO/.source/compiled/updates/ClockOSv2.8/ClockOSv2.8.bin" \
  | tee "$WORK/logs/new-versus-published-sha256.txt"
if cmp -s "$IMAGE" \
  "$REPO/.source/compiled/updates/ClockOSv2.8/ClockOSv2.8.bin"; then
  printf 'byte-identical\n' | tee "$WORK/logs/rebuild-comparison.txt"
else
  printf 'different bytes: investigate build metadata/toolchain/source provenance\n' \
    | tee "$WORK/logs/rebuild-comparison.txt"
fi
GIT_OPTIONAL_LOCKS=0 git diff --stat | tee "$WORK/logs/tracked-diff-after.txt"
GIT_OPTIONAL_LOCKS=0 git diff --cached --stat | tee "$WORK/logs/staged-diff-after.txt"
GIT_OPTIONAL_LOCKS=0 git status --short | tee "$WORK/logs/status-after.txt"
```

**Gate:** a new compile proves those inputs compile. Byte identity strengthens reproducibility evidence; a mismatch is not automatically corruption, but must be investigated and recorded before claiming reproduction. Absolute paths, embedded ELF hashes/build timestamps and tool versions can affect output. Do not replace the existing image or regenerate manifest hashes merely to make a comparison pass.

For the eventual UI overhaul, comparison with v2.8 is expected to differ. Bind the **new** verified image to the **new** source/commit/build record, rerun the current-release/selection/host behavior tests against that version, and keep the original baseline intact.

### Step 7 — Promotion decision and truthful status

Non-hardware gates can authorize **“source reviewed; host tests pass; compiler exit success; image/manifest integrity checked; partition configuration inspected”**. They cannot authorize **“physically validated”, “no flash”, “touch calibrated on the panel”, “OTA rollback tested”, “Windows 10/11 tested”, or “stable hardware release”**.

Keep the device/Windows checkboxes open and publish a precise acceptance matrix. Promotion onto `main` exposes the maximum version to existing Auto/OTA consumers; do not mistake a manifest-only edit, a candidate label, or an older preserved binary for deployment isolation. Make the publication decision explicit.

## 8. Ordered implementation checklist

1. **Freeze the v2.8 baseline and preserve behavior.** Retain `updateV1`, canonical ClockOSV1, historical v2.x slots, the current rotation/calibration/pressure threshold, weather/persistence/alarms/Classroom/setup flow, SD erase safeguards, Home sleep behavior, and maximum-version update selection. Do not alter them solely to make tests or labels pass.
2. **Agree the next release identity and release classification.** Apply the repository's existing increment policy consistently; define stable versus previous-built terminology and explain that Auto/OTA selects the highest published version rather than the manifest target.
3. **Repair acceptance documentation/TODO semantics in the implementation PR.** Separate implementation, host-test, build, native Windows and hardware statuses; reopen or attribute unsupported checked execution claims; correct outdated comments/refresh/sleep/retention descriptions. Leave all seven unchecked hardware/native-dependent entries open.
4. **Make release tests current-version-aware.** Remove the v2.7-only parity blind spot and v2.8-only release-test assumptions; validate exact release constants, all identity/path fields, current/versioned manifest equality, support files, application-only image paths and hashes. Keep documented legacy exceptions.
5. **Add isolated selection and safety regressions.** Exercise Python selectors/fallback and actual OTA logic under mocks; test malformed/empty feeds, version/tie handling, no automatic bootloader selection, short/failed writes, installed-path conditions, no failure reboot, update-only safety, failed wipe stopping installation, output naming and retry restoration.
6. **Add non-hardware UI preservation evidence for the overhaul.** Use recording TFT/touch/time stubs to exercise existing drawing/state functions and check bounds/transitions without duplicating production logic; retain image-marker smoke checks but never use them as visual acceptance.
7. **Provision and record a pinned scratch release toolchain.** Verify the CLI distribution checksum; pin core 2.0.17 and the recorded library versions, preserve `FQBN`/`TFT_FLAGS`, capture dependency/compiler inventory and full logs. If unavailable, report the build gate as blocked rather than claim success.
8. **Compile the exact new same-name sketch outside the repository feed.** Resolve errors from actual output; record program/RAM/image bytes, source/commit hashes, ELF and generated partitions; verify both OTA slots fit. Never publish a helper, bootloader, partition or merged image as the application update.
9. **Validate image format and release provenance.** Run offline esptool image-info, verify actual SHA-256/size, investigate unexpected rebuild differences, and generate both new manifests from the one approved image. Do not overwrite historical artifacts to manufacture matching evidence.
10. **Run the full host validation sequence and inspect the resulting diff.** Confirm all existing and added tests pass against the new version and all outputs remain isolated until approved. Record exactly which steps ran and which remain blocked.
11. **Obtain separate target-platform evidence before closing platform TODOs.** Hosyond orientation/corners/center, Settings rendering/touch, physical SD Ready/preservation, Wi-Fi/weather/peripherals, GPIO27 sleep/wake, OTA/partition/failure/recovery, and Windows 10/11 icon/DPI/launcher/retry/flashing require the respective physical/native environments. No sandbox result substitutes for them.
12. **Make an explicit release/promotion decision.** Publish the synchronized source, verified application image, manifests, build record and validation matrix together; remember a higher-version `.bin` on `main` is immediately eligible for current Auto/OTA logic. Keep residual hardware/Windows limitations visible and do not close their TODOs until their own evidence exists.
