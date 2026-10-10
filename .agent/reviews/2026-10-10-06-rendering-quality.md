# Rendering quality review — ClockOSv2.8

**Fix framebuffer ownership and PNG callback correctness before adding more visual polish.** The present firmware already uses compact FreeSans fonts, local password-field updates, bounded alarm arrays, and a dark StandBy-style home screen. Those foundations can be retained. The largest remaining technical risks are home-only painters writing over Settings, PNG decoding stopping after the first row, text slots that overlap other controls, and periodic erase-and-repaint operations that run even when nothing displayed has changed.

This is a **static source and version-matched dependency review**, not a new ESP32 build or a physical-device test. The supplied TODO, conventions, README, current `.ino`, and `config.h` were read. Additional inspection was limited to this version's `bootloader.h`, the release build record, relevant installer TFT flags/icon conversion, and upstream rendering dependencies. No application source, manifest, TODO, binary, test, or Git state was changed. No claim here establishes physical display, touch, backlight, SD, or OTA validation.

Unless qualified otherwise, source locations below refer to `.source/uncompiled/updates/ClockOSv2.8/ClockOSv2.8.ino`. The release record documents ESP32 core **2.0.17**, TFT_eSPI **2.5.43**, PNGdec **1.1.6**, ArduinoJson **7.4.3**, and `esp32:esp32:esp32:PartitionScheme=min_spiffs`; it is prior build evidence, not validation of these proposed edits. Preserve the current rotation `TFT_ROTATION = 3`, calibration constants `{343, 3436, 266, 3381, 1}`, GPIO27 active-high backlight, preferences, onboarding, weather/manual-city behavior, four-alarm support, calendar cache, all navigation, developer flow, factory-reset confirmations, and safe OTA behavior. Do not add indoor sensors or change release identity as part of a renderer refactor.

## 1. Stop cross-screen writes and make invalidation page-aware

### P0 — Settings handlers paint home content over the active Settings page

**Observation.** In `loop()`'s General category, the handler ends with `saveSettings(); showSettingsPage(); drawStatusBlock(); return;` at line 1930. `drawStatusBlock()` clears `(0,226,240,94)` black and draws home status controls. In the Calendar category, line 1946 calls `showSettingsPage(); drawRightPanel();`; `drawRightPanel()` clears `(241,0,239,320)` black and paints home weather/calendar. Both operations occur while `screen == S_SETTINGS`. This is a deterministic source-level ownership defect, not an inferred panel artifact.

**Recommendation.** Remove those two direct home-painter calls from the Settings handlers. Preserve the settings mutations and `saveSettings()` calls. Mark the corresponding home fields dirty for the next `showHome()` instead. As a defensive second layer, require that the active render context is Home before a direct home-region painter can write to the TFT. Keep separate entry wrappers and paint functions so a Settings toggle can repaint only its own row without entering another screen.

### P0 — Background refresh can replace setup or damage an alarm overlay

**Observation.** `refreshSdState()` at lines 1093–1112 runs before the screen-specific handlers. On an SD-state change it calls `showSettingsPage()` whenever `screen == S_SETTINGS`. `showClassroomSetup()` and `showSyncSetup()` deliberately also use `S_SETTINGS`, distinguished by `firstSetupClassroom` and `firstSetupSync`. An SD change can therefore replace a first-run wizard with ordinary Settings while the touch handler still interprets wizard choices. `showAlarmRinging()` sets `alarmRinging` but does not create a separate `Screen`; normally the underlying screen remains `S_HOME`. The SD refresh can paint a home status region over that overlay. The home inactivity check also precedes the `alarmRinging` guard.

**Recommendation.** Keep `refreshSdState()` responsible for sampling/updating SD state, but replace immediate screen rebuilding with invalidation events. The renderer's page key must include `screen`, `firstSetupClassroom`, `firstSetupSync`, `settingsAtRoot`, `settingsCategory`, `alarmListMode`, the alarm overlay, and a theme generation. A wizard or alarm overlay owns the framebuffer until dismissed. Queue underlying home invalidation rather than flushing it over the overlay. Exclude an active alarm overlay from ordinary Home sleep eligibility; do not alter alarm scheduling, dismissal, or persistence.

At minimum, add first-setup and overlay guards before the current redraw dispatch. The more maintainable edit is a single `renderPendingUi()` in `loop()` which paints only the current page's dirty regions. SD removal/insertion should update its visible indicator or Storage row, not rescan Wi-Fi, reset editing state, or rebuild unrelated pages. Mark the weather icon dirty too when asset availability changes; the current Home dispatch redraws status only.

### P1 — Separate page entry from repainting editable state

**Observation.** Several `show...`/`draw...` functions combine drawing with navigation state. Most are currently used as entry wrappers, which is reasonable, but reuse for partial repaint is hazardous. A concrete example is `showDeveloperPinPage()` at line 1393: it resets `developerPinInput = ""`, and `loop()` calls it again after every PIN key at line 2001. The page therefore clears accumulated input during repaint. `drawAlarmList()` and `drawAlarmEditor()` likewise set `alarmListMode`, so they are not interchangeable paint-only helpers.

**Recommendation.** Initialize PIN input only when entering the PIN page; add a paint-only PIN-field function and derive its mask from the retained input. Keep current PIN, Clear, Enter, Back, and unlock behavior. Similarly, keep page-entry state changes in wrappers and make row/time/date/field painters free of navigation, input resets, SD writes, or blocking acquisition. An invalidation event must never reinitialize `password`, `alarmDraft`, `clockDraft`, or wizard choices.

## 2. Repair the PNG path before relying on SD illustrations

### P0 — `pngDraw()` returns the decoder's stop value

**Observation.** `pngDraw(PNGDRAW* d)` at lines 283–287 converts and pushes a row, then returns `0`. In PNGdec 1.1.6, the callback is correctly an **`int`**, but `DecodePNG()` treats a zero callback return as **`PNG_QUIT_EARLY`** and exits. The current callback therefore stops after its first emitted row. `drawPng()` at lines 289–296 ignores `png.decode()`'s return and reports `true` whenever `png.open()` succeeds. [1] [2]

**Recommendation.** Keep the existing `int` callback signature; return **`1` after each successfully processed row**, including intentionally clipped/skipped rows. Return `0` only to abort on a real safety failure. Capture the result of `png.decode()` and report success only when it equals `PNG_SUCCESS`. Close the decoder/file and balance the display transaction on every path. Do **not** revert to a `void` callback copied from an older TFT_eSPI PNG example; that conflicts with the documented PNGdec 1.1.6 type.

The required control flow is:

```cpp
// Within a bounded, successfully opened drawPng() implementation:
tft.startWrite();
const int result = png.decode(nullptr, 0);
tft.endWrite();
png.close();
return result == PNG_SUCCESS;

// At the end of a successfully handled pngDraw() row:
return 1;
```

This fragment is not a complete safe replacement: apply the dimension, file, clipping, alpha, and transaction rules below as well.

### P0 — TFT clipping does not protect the RGB565 conversion buffer

**Observation.** `lineBuf` is a static `uint16_t[480]` at line 216. `pngDraw()` passes it to `getLineAsRGB565()` without checking `d->iWidth`. Conversion writes the decoded source row before `tft.pushImage()` can clip it. PNGdec's internal pitch limit is not this application's 480-pixel limit; a wide indexed/grayscale PNG can pass decoder checks while exceeding `lineBuf`. `drawPng()` also has no widget dimension limit, so a valid large image can overwrite neighboring weather text or calendar content even if it fits the panel. [1] [2] [4]

**Recommendation.** After opening and before decoding, reject nonpositive dimensions, width beyond `sizeof(lineBuf) / sizeof(lineBuf[0])`, and assets beyond the weather-icon slot. The existing installer uses `ICON_SIZE = 96` and preserves alpha, so a **maximum 96 × 96 weather asset**, centered inside a defined 96 × 96 slot, is a conservative compatible contract. Permit smaller assets and center them from `png.getWidth()`/`getHeight()`; reject or preprocess oversized assets rather than inventing runtime scaling. `PNGdec::decode()` does not automatically scale to the destination rectangle.

Repeat the width check in `pngDraw()` before conversion. Clip rows and horizontal spans against the assigned icon rectangle, not just the physical display. If changing from global `pngX`/`pngY` to a callback context, pass that context via `decode(context, options)` and `PNGDRAW::pUser`. Keep the single global decoder non-reentrant; future background fetch tasks must not invoke it concurrently.

### P1 — Alpha currently does not mean transparency

**Observation.** `getLineAsRGB565(..., PNG_RGB565_BIG_ENDIAN, 0xffffffff)` at line 284 explicitly **disables alpha blending** in PNGdec 1.1.6. `tft.pushImage()` then writes every converted pixel; the underlying `drawWeatherFallback()` is not preserved by transparent PNG pixels. The comment in `drawWeatherIcon()` describing transparent overlays is therefore inaccurate. PNGdec expects the blending background in **`0x00BBGGRR`**, not RGB565 or ordinary `0x00RRGGBB`. [2] [3]

**Recommendation.** Render the icon on its actual opaque parent surface and supply the correctly converted background; for the current black Home this is simply `0x00000000`. Do not pass a raw `UI_BACK`/`UI_KEY` RGB565 value as the 32-bit PNG background. Prefer composing the whole icon slot in a small sprite: clear its background, try decoding the installed asset, and if any row fails, clear the partial result and draw the complete built-in fallback. Push the completed slot once. This preserves SD artwork when valid and a complete readable weather symbol when absent/corrupt, without a first-row remnant or a fallback cloud showing through an otherwise successful illustration.

Cache only the bounded rendered icon when useful. Invalidate it on icon-code/asset changes, SD changes, and parent-background/theme changes. A title/due-date refresh should not reopen and decode the same weather file.

### P1 — Make file adapters and byte order explicit

**Observation.** `pngOpen()` always returns `&pngFile`, even when `SD.open()` fails. `pngSeek()` returns `File::seek()`'s boolean, not the achieved position. PNGdec 1.1.6's decoder ignores that seek return at its chunk seek site, so the boolean is a contract defect but is **not established as the cause** of the first-row problem. Its own memory seek callback returns a byte position. PNGdec 1.1.6 also returns zero from `PNG::open()` if the open callback returns null, so a null handle alone is not sufficient application-side success detection. [2] [3]

**Recommendation.** Return null on failed file open, record/check a valid file and positive dimensions in `drawPng()`, and close a file left open after header/decode failure. In `pngRead()` reject invalid/nonpositive requests; in `pngSeek()` validate the offset, perform the seek, and return the achieved `position()` on success with a consistent failure sentinel. Use the supplied `PNGFILE::fHandle` where practical instead of silently ignoring the callback's handle.

The existing `PNG_RGB565_BIG_ENDIAN` conversion is compatible with the TFT's default `setSwapBytes(false)`; it is not itself a proven color-order bug. Save/set/restore swap state around external image writes so new code cannot silently double-swap colors. TFT_eSPI 2.5.43's `TFT_eSprite::pushSprite()` already saves/restores the parent TFT swap state. For a 16-bit sprite target, use a consistent big-endian source plus that sprite's `setSwapBytes(false)`, or native-endian source plus its appropriate swapping setting. Do not apply a blanket `tft.setSwapBytes(true)` to fix all image paths. [4] [5]

## 3. Give every label an actual pixel slot

### P1 — Existing text metrics are useful, but not sufficient layout protection

**Observation.** `txt()` at lines 134–139 sets a font, transparent text color, and datum, then calls `drawString()` without bounds. `fitText()` at lines 141–146 measures pixel width, which is better than a character count, but it removes individual bytes, repeatedly allocates `value + "..."`, and still returns `"..."` when even the ellipsis exceeds `maxWidth`. Both leave font state on the global TFT. Neither pins text size/padding or defines a clip rectangle.

TFT_eSPI 2.5.43's `textWidth()` uses glyph advances and the last glyph's extent; it is not a universal ink bounding box. `fontHeight()` for free fonts returns `yAdvance`, while `drawString()` datums use ascent/descent calculations. Negative bearings and descenders require margins. Its **String overloads of `textWidth()` and `drawString()` create runtime-sized stack buffers**. Very long SD/cache strings can therefore cause substantial transient stack use before they are visually shortened. [4]

**Recommendation.** Introduce a bounded text helper taking a target, font, foreground, parent background, rectangle, alignment, and fitting mode. Establish `setTextSize(1)` and `setTextPadding(0)` explicitly. Use `drawString(prepared.c_str(), ...)` and `textWidth(prepared.c_str())` after bounded display preparation to avoid the dependency's String-copy stack buffers. Retain transparent glyph drawing only over a freshly composed/cleared field; a shorter replacement must erase the whole previous field including descenders.

For one-line values, handle `maxWidth <= 0` and ellipsis-too-wide with an empty result or a fitting dot sequence. Walk complete UTF-8 codepoints, not bytes. Normalize unsupported characters in a **display-only copy**, then fit it; preserve raw SSIDs/passwords/city values used for connection and persistence. Use a bounded candidate buffer or one reserved String, not a fresh concatenation on every removal. For helper text, implement explicit word wrapping with a maximum line count and line spacing from metrics; `setTextWrap()` affects the print stream and is not a substitute for wrapping a `drawString()` label.

### P1 — Specific source-derived collisions to fix

The following measurements were calculated from the bundled TFT_eSPI 2.5.43 glyph tables at text size 1, not observed on hardware. F12 and F12B have 13-pixel ascent, up to 5-pixel descent, and 22-pixel `yAdvance`; F18B has 17/6/29; F24B has 25/8/42. Use these numbers as baseline evidence, then verify actual rendered rectangles during implementation.

**`drawWifiPageStatus()` (1640–1647).** The right-aligned status may occupy up to `x=287..447`; the subsequent `drawWifiSignal(364,74,...)` clears `x=344..383` and paints over that text. Its clear rectangle also starts at `y=52`, above the row's `y=54`. Reserve separate slots and draw the icon before text. For example, keep Network at `x=30`, fit the status into roughly `x=130..389`, and place the icon near `(421,77)` so its existing clear box lies inside the row. Define the slots once and clip them independently. Do not simply reorder the current overlapping coordinates; that only changes which item wins.

**`drawStatusBlock()` (1077–1091).** F12B `"Settings"` is approximately **71 px** wide inside a **58 px** pill. F12 `"100%"` is approximately **47 px** wide starting at `x=142`; the pill begins at `x=171`, so it covers the percentage. Use the existing `drawSettingsGear()` inside the unchanged Settings touch area, or allocate a truly sufficient text control and update its hit rectangle together. Move percentage text into a separate small slot above the battery/icon row if retaining the present home regions. Preserve the battery readout option and Settings access; do not silently hide the value to solve the collision.

**`settingsGroupRow()` (1433–1438).** The label may extend to `x=275`, while a 160-pixel right-aligned value can start at `x=287`, leaving only 12 px. Keep that separation deliberate and add bearing/gutter protection. `settingsRow()` has a much tighter 7-pixel nominal gap between its 125-pixel label and 64-pixel value; reduce budgets or derive them from a shared row rectangle.

**`drawSettingsOption()` (1452–1460).** A non-toggle title can consume 340 px from `x=26`, ending at `x=366`; detail text can occupy `x=300..418`, a 66-pixel overlap. Derive title width from the actual detail/disclosure/toggle allocation. If both detail and toggle are supplied, ensure the detail ends before the toggle at `x=408`; the current detail endpoint `418` is unsafe for that combination. Keep text centered vertically relative to the row's center rather than mixing `y+17` text with `y+23` disclosure centers and `y+23` switch centers.

**`drawPasswordField()` (910–916).** Keeping the last 20 characters is not a pixel fit: 20 capital W glyphs are approximately **340 px**, exceeding the field's roughly 310-pixel text interior. Retain the input and the newest-character tail behavior, but fit the tail by pixels, including a cursor gutter. Masked passwords can use the same bounded path. For Show/Hide, repaint only this field plus the Show/Hide key; `loop()` currently calls `showKeyboard()` at line 1823 and needlessly clears the whole page.

**`drawAlarmEditor()` (1210–1228).** F24B `"12:59 PM"` at `TC_DATUM y=58` has ink through about `y=83`; the F12 `"12-hour time"` at `y=80` starts around `81` and extends through `93`. The mode label also reaches into the hour/minute buttons beginning at `92`. The F18B date at `y=136` extends through about `153`, overlapping the date buttons beginning at `151`, which are painted afterward. Rebudget the vertical bands: for example use an F18B summary at `y=54`, the F12 mode label at `y=76`, and the date at `y=129`, while preserving the current button/hit locations. Alternatively move whole bands and their hit rectangles together. `drawClockEditor()` has different spacing; do not reuse its offsets without checking each font's ink box.

**`drawRightPanel()` (1114–1130).** F12 `"No upcoming assignments"` is approximately **212 px**, while populated assignment titles use 196 px. Fit the empty-state message too, and fit the city line to the weather slot. Do not rely on city length or default Philadelphia to guarantee bounds.

**Long explanatory/status strings.** F12 Wi-Fi footer text in `showWifiPage()` is approximately **647 px**, and the Weather footer in `showWeatherPage()` is approximately **606 px**, both drawn as a single centered line on a 480-pixel panel. The two Factory Reset explanation lines are approximately **466 px** and **481 px** inside a 444-pixel card. `showScan()`'s no-networks message is approximately **513 px** with F12B. Wrap these into bounded lines instead of truncating destructive-action explanations. Apply the same helper to `showSdCardPage()`, `showSyncSetup()`, `showProfilePage()`, keyboard errors/header, and update status. Preserve full warnings, recovery text, and connection identities; visual truncation must not change stored data.

### P1 — Unicode literals are not supported by the selected fonts

**Observation.** F12/F12B/F18B/F24B reference `...7b` free fonts whose range is `0x20..0x7E`. Degree sign `°` in `drawRightPanel()`, `‹` in the Settings Back label, `✓` in the appearance selection, and `…` in `drawWifiPageStatus()` are outside that range. Enabling `SMOOTH_FONT` does not magically add those glyphs to the selected free font. They may disappear and their width may be excluded. This is a rendering issue, not evidence that these UTF-8 literals fail C++ compilation. [4] [6]

**Recommendation.** Draw the checkmark and Back chevron with vector lines, as existing disclosures already do. Use ASCII `"Connecting..."`. Draw a degree circle at a measured offset next to the temperature, or a clearly supported `" F"` suffix while keeping the current Fahrenheit behavior. For non-ASCII external names, use a display fallback or a deliberately supplied font with verified glyph coverage; do not substitute unsupported punctuation as an ellipsis.

## 4. Replace erase-and-repaint loops with changed-field rendering

### P1 — Wi-Fi redraws a stable connected row every 180 ms

**Observation.** In `S_WIFI`, `loop()` calls `drawWifiPageStatus()` every 180 ms regardless of connection state. That function clears the entire 448 × 38 row and redraws its text and icon. A connected row therefore has a blank phase about 5.6 times per second. `tryConnect()` clears/redraws the glyph every 80 ms even though `drawWifiSignal()`'s frame changes only every 180 ms. The current connected-state behavior is not stable at the renderer level.

**Recommendation.** Cache connection status, SSID display text, RSSI level, palette generation, and the last animation frame. Repaint the full Network row only when its visible content changes or on page entry; animate just the bounded glyph while connecting, and only once per new frame. Do not animate a stable connection. Compose the small glyph in a sprite and push the finished frame rather than erase it on the panel. Preserve the connection timeout and scanner flow. In Home, where status currently refreshes every 30 seconds, schedule a glyph-only animation if connecting feedback is required rather than accelerating battery/SD sampling.

### P1 — Weather and calendar share an unnecessarily large repaint

**Observation.** `drawRightPanel()` always clears the entire 239 × 320 panel, decodes weather, and paints calendar. `loop()` invokes it after weather refresh and after each 60-second cache load. After `WEATHER_REFRESH_MS` expires, an offline/disconnected `fetchWeather()` returns before advancing `lastWeatherFetch`; the Home tick can therefore call `drawRightPanel()` every second even with unchanged weather. Network/HTTP failures after the early guard do advance the timestamp, so this particular repeated-redraw problem is specific to that early-return path. The source's cache load is every 60 seconds despite README language about a one-second summary refresh; do not conflate cached display refresh with live Google synchronization.

**Recommendation.** Split `drawRightPanel()` into a static parent/separator painter, a weather widget, and a calendar-summary widget. Compare the actual displayed temperature, icon, city, validity, title, due text, and visibility flags before invalidating. A failed fetch that leaves existing visible state unchanged needs no paint. Keep network refresh behavior separate from display scheduling; if an attempt timestamp is introduced, preserve prompt recovery on reconnection rather than accidentally suppressing it for a full weather interval. Keep cache reads and all existing services, but repaint only after visible cache data changes. A calendar-title change must not trigger an SD PNG decode.

### P1 — Home time redraw can be smaller and more reliable

**Observation.** `drawTimeBlock()` clears 240 × 224 on each minute change, then paints digits, AM/PM, and date. `loop()` caches only `ti.tm_min`, which does not cover validity, hour/date/time-format changes with the same minute number. `showHome()` paints everything but does not reset those local loop caches. Existing `drawBigTime()` geometry itself is sound: four 44-pixel digits, a 20-pixel colon, and four 8-pixel gaps total **228 px**, fitting the 240-pixel column with 6-pixel gutters. Do not enlarge it blindly for visual impact.

**Recommendation.** Snapshot time once per render tick and compare a full visible key: valid/invalid, formatted hour/minute, AM/PM, date, and `settings.use24Hour`. Invalidate the digit strip only on time/layout change, AM/PM only when needed, and date only on date change. Update render caches when entering Home so the next tick does not duplicate the initial paint. For 12-hour transitions between a one-digit and two-digit hour, repaint the complete time strip because `drawBigTime()` recentering changes all x coordinates. Preserve `--:--` and the current time-format controls.

A small monochrome sprite is suitable for the seven-segment strip; gray date text can remain a separate field. Simpler bounded whole-strip composition is preferable to complex per-segment erasure until measured. If doing segment deltas later, clear both old and new extents, and handle changed digit count explicitly.

### P1 — Editors, switches, and OTA should not redraw their full pages for value changes

**Observation.** Alarm switches call `drawAlarmList()`; hour/minute/date/repeat controls call `drawAlarmEditor()`; manual clock changes call `drawClockEditor()`; each clears the screen/header. `drawUpdatePage()` clears the full panel for every call. `updateProgress()` caches only a static last percentage, not an update session or status. In the existing `BL::installCandidate()`, progress is reported after `Update.writeStream()`, not continuously during streaming, so renderer improvements alone will not produce smooth intermediate download progress.

**Recommendation.** Keep full repaint for navigation, first entry, theme change, and recovery from a destroyed framebuffer. For same-page changes, repaint only the affected alarm row/switch, time/date summary, repeat selection, enabled button, or Settings toggle. Draw OTA card/chrome once per session; update bounded status, bar, and percentage fields when changed. Reset the update progress cache in `runUpdate()` and include status/session identity in its key. Clamp tiny progress fills: `fillRoundRect()` does not validate that radius 3 fits a 1- or 2-pixel fill; use a plain narrow fill or a radius capped to half its width. Preserve up-to-date, failure, installed, and reboot states. Do not fabricate streaming progress or change `Update` write safety within a rendering-only edit.

## 5. Smooth primitives without relying on excess RAM or implicit state

### P1 — Use bounded composition, not a full-screen framebuffer

**Observation.** `setup()` allocates a 100 × 100, 16-bit `spr` without checking `createSprite()` at line 1730. The loader sprite path `animateLoader()` exists but has no call site in the current `.ino`; active connection feedback uses `drawWifiSignal()`. This allocation consumes about **20,000 bytes** without benefiting the active path. The release record's 96,352 bytes of static/dynamic build-reported allocation does not include all later sprite, TLS, JSON, and String heap pressure. No PSRAM requirement is established in this repository.

**Recommendation.** Keep the loader implementation available, but allocate/check its sprite lazily or repurpose the existing sprite through an explicit bounded buffer strategy. Every `createSprite()`/allocation must have a null check and a clipped direct-draw fallback. Reuse buffers rather than allocate/free during each animation frame. Never store a large RGB565 image or variable-length external string buffer on the loop task's stack.

For planning, a 480 × 320 × 16-bit buffer requires **307,200 bytes**; a double buffer requires **614,400 bytes**, before allocator overhead. Prefer one reusable 448 × 50 row sprite (44,800 bytes), a 96 × 96 icon sprite (18,432 bytes) only if budget permits, or smaller strips. A 240 × 96 one-bit clock strip needs roughly **2,880 bytes** before library overhead. One-bit/low-depth buffers are useful for monochrome glyphs, but 8-bit RGB332 or palette reduction can visibly degrade subtle theme colors; do not apply it indiscriminately to all Apple-style surfaces. [5]

Sprites remove the visible clear-before-draw phase, not the panel's scan timing. At the configured 40 MHz SPI, writing one 307,200-byte frame has an ideal pixel-transfer lower bound of about **61.4 ms**, before command/software overhead. A full right-panel clear alone is about **30.6 ms**. No tearing-effect synchronization is currently implemented, so do not promise tear-free physical output solely because a sprite is used.

### P1 — Parameterize drawing targets and preserve backgrounds

**Observation.** `arcLovy(TFT_eSPI& g, ...)` and `drawLoaderFrame(TFT_eSPI& g, ...)` already accept a display target. TFT_eSprite genuinely inherits TFT_eSPI and overrides primitive drawing, so passing `spr` here is valid in 2.5.43; it is **not** a compile error. Most other painters, including `txt()`, `drawDigit()`, and weather fallback, directly use global `tft`, preventing composition into the existing sprite. However, **`pushImage()` is not virtual**, so a generic TFT_eSPI reference holding a sprite must not be used blindly for image blitting. [4] [5]

**Recommendation.** Add target-aware text, digit, vector-icon, and rounded-control painters using `TFT_eSPI&` for supported inherited primitives. Use an explicit TFT-versus-sprite image target or a compile-time template for PNG row blits so the correct `pushImage()` implementation is selected. Keep existing global-TFT wrappers until all callers are migrated. Avoid changing rendering library to LovyanGFX; `arcLovy()` is only a name/angle adapter in the current TFT_eSPI implementation.

Create explicit Home palette constants, such as `HOME_BG`, `HOME_TEXT`, and `HOME_MUTED`, if Home intentionally remains black while settings themes vary. `drawSdCardIcon()` currently uses `UI_BACK` for an absent card, even when called on the black Home. Add a parent-background argument and pass the actual surface at each call. In `drawStatusBlock()`, use the documented ready/absent/invalid presentation (`sdOk`, `sdPresent && !sdOk`) instead of passing `sdPresent, false`; retain SD detection and preparation logic unchanged.

Transparent text, erased animation pixels, and anti-aliased edges must all use the same actual parent surface. Do not introduce gradients, shadows, or textured cards without retaining/recomposing that parent in every dirty rectangle; erasing with flat `UI_BACK` would punch rectangular holes into them.

### P2 — Apply anti-aliasing selectively with exact supported APIs

**Observation.** Existing `fillRoundRect()`/`fillCircle()` are non-antialiased; `arcLovy()` already uses `drawArc(..., true)` with an explicit background. TFT_eSPI 2.5.43 provides `fillSmoothRoundRect()`, `fillSmoothCircle()`, `drawWideLine()`, and `drawSmoothRoundRect()`. The outline function's parameter order differs from ordinary `drawRoundRect()`. [4]

**Recommendation.** Start with controls and small status glyphs, not a wholesale replacement of every primitive. Use the actual parent background explicitly:

```cpp
// Correct TFT_eSPI 2.5.43 parameter order:
g.fillSmoothRoundRect(x, y, w, h, radius, face, parentBg);
g.fillSmoothCircle(cx, cy, radius, face, parentBg);
// Outline: x, y, outer radius, inner radius, width, height, fg, bg.
g.drawSmoothRoundRect(x, y, outerR, innerR, w, h, outline, parentBg);
```

Do not omit the background and trigger slow/unverified TFT readback for antialiasing. Derive any inner/outer backgrounds correctly for a border over an already filled card. Include a small fringe margin in dirty regions, and benchmark optional smooth primitives against the existing fast rounded shapes. Validate dimensions before ordinary `fillRoundRect()` calls and cap radii to `min(w,h)/2`; the existing `drawProfileIcon()` uses radius 12 for a 16-pixel-high shape and should be normalized. Clamp `drawBattery()` input to 0–100 at its boundary and validate `drawDigit()`'s `d`, `w`, `h`, and thickness before indexing `SEG_MASK[d]` or producing negative segment dimensions. Current callers pass valid digits, but a generalized renderer should not rely on that forever.

## 6. Keep the refactor compilable and its input handling bounded

### P1 — Preserve Arduino prototype and dependency contracts

**Observation.** The sketch already has explicit `Key`/`AlarmState` forward declarations and default-bearing function declarations at lines 27–39 to work around Arduino prototype generation. `F12B`, `F18B`, `F24B`, and `F12` are aliases, not actual point-size names. The fonts are included through TFT_eSPI's `LOAD_GFXFF` path; a repository-local `Free_Fonts.h` is not present. `config.h` does not configure the TFT library compilation; the installer supplies that configuration globally.

**Recommendation.** Put any new `Rect`, `TextStyle`, `RenderKey`, or callback-context type into a deliberately included support header, or forward-declare it before every use in manually declared prototypes. Define default arguments once, not on both declaration and definition. Include required headers explicitly for newly used C/C++ facilities; avoid unverified library methods, desktop containers, variable-length arrays, or guessed sprite DMA support. Keep type-compatible `min`/`max`/`constrain` arguments to avoid overload/macro surprises on the ESP32 toolchain.

Build the redesigned source with the recorded core/FQBN and the exact library versions first. Preserve installer flags `USER_SETUP_LOADED`, `ST7796_DRIVER`, controller portrait dimensions **320 × 480**, `USE_HSPI_PORT`, TFT/touch pins, `LOAD_GFXFF`, and the recorded frequencies. The sketch's logical `TFT_W`/`TFT_H` remain **480 × 320 after rotation 3**. Do not change the controller's compile-time portrait dimensions to 480 × 320 merely to match UI constants. Retain the independent VSPI SD wiring. Add a runtime diagnostic/assertion that `tft.width() == TFT_W` and `tft.height() == TFT_H` after rotation; log mismatches rather than silently stretching the UI.

The installer currently lists dependency names without pinning `LIBS` versions. A new build must record the versions actually selected; this report does not authorize modifying installer manifests or dependencies. Do not infer successful compilation from the old binary, TODO checkboxes, API familiarity, or this review.

### P1 — Bound display copies and protect fixed arrays without corrupting original data

**Observation.** Existing keyboard layout builds **43 keys into `keys[48]`**, and its last row starts at `y=280` with height 36, ending at exclusive `316`; those current bounds fit the panel. `keyLabel()` uses `strcpy()` into current `char lb[8]` callers; all present labels fit. The existing fixed `snprintf()` time/date/path buffers are appropriately sized for their normal inputs, but `addKey()` has no capacity check and `keyLabel()` has no length contract. External assignment titles/course names, profile/location text, and SD filenames use dynamic Strings. These are hardening opportunities, not a claim that current ASCII key labels overflow.

**Recommendation.** Add a capacity check in `addKey()` and return failure rather than increment `nk` beyond the array. Give `keyLabel()` an output-capacity argument and use bounded formatting/copying, or return constant labels directly with a separate two-character buffer for character keys. Validate `layer` before indexing `ROWS[layer]`, and guard `addRow()`'s count, available width, and nonzero weight sum before making it reusable.

Bound display-only copies of external text before metric calls and wrapping, leaving raw connection/persistence data intact. Check `snprintf()` results for truncated icon paths or large update status formatting and fall back safely. Reserve recurring small Strings where they are reused; do not churn new allocations every animation frame. Keep JSON, HTTP, SD reads, and ADC acquisition out of pure paint functions: `readBatteryPct()` presently adds roughly 20 ms of delay and should remain on its existing sampling schedule, with painters receiving the sampled value. Avoid broad data-service redesign in this rendering change.

### P1 — Viewports and transactions need explicit ownership

**Recommendation.** Define widget rectangles with signed coordinates and half-open bounds, intersect them with `TFT_W`/`TFT_H`, and reject empty rectangles before painting. With existing absolute coordinates, use `setViewport(x, y, w, h, false)`: the default `true` changes the origin to the viewport and would offset current calls twice. Restore the previous viewport and origin on every exit, or prohibit nesting and reset at controlled top-level boundaries. A shared viewport/origin leak can move later text, icons, and even the panel clear. Keep draw and touch rectangles in the same source definitions when moving controls; do not alter calibration to compensate for layout bugs. [4]

Batch direct-TFT primitives with one clearly owned `startWrite()`/`endWrite()` pair per widget/frame operation. These transactions reduce bus overhead; they do **not** make a multi-operation erase-and-redraw atomic. Do not nest an outer transaction around the current `drawPng()` and assume its inner `endWrite()` preserves the outer lock; the library uses transaction-state flags rather than a general nesting counter. Do not hold a TFT transaction across HTTP requests, delays, or touch reads. Preserve SD/TFT bus separation. Future asynchronous acquisition should return data to a single UI owner, not draw from a Wi-Fi/OTA callback task concurrently.

### P1 — Complete the wake frame before enabling the backlight

**Observation.** `wakeDisplay()` keeps GPIO27 low for sleep-out/display-on, but at line 1635 enables the backlight **before** calling `showHome()`. Its comment claims the Home is restored without a white wake flash, but the ordering still exposes the clear-and-draw sequence and any uncertain panel contents. `setup()` similarly enables backlight immediately after TFT initialization before the final initial page is rendered.

**Recommendation.** In `wakeDisplay()`, retain the current controller commands/delays and intentional return to Home, invalidate the destroyed framebuffer, render the complete Home while GPIO27 is low, then enable the backlight. Do not restore `screenBeforeSleep` without a separate behavior requirement. For boot, hold the backlight dark through a black clear and enable it only after the first complete intentional screen; keep connection/onboarding feedback visible once that screen exists. `sleepDisplay()`'s black fill and explicit backlight-off remain. Panel sleep timing and GPIO27 behavior still require physical verification; this is an ordering recommendation, not proof that a particular controller flash is eliminated.

## References

[1]: https://raw.githubusercontent.com/bitbank2/PNGdec/1.1.6/src/PNGdec.h "PNGdec 1.1.6 callback types, dimensions, RGB565 conversion API, and buffer declarations"
[2]: https://raw.githubusercontent.com/bitbank2/PNGdec/1.1.6/src/png.inl "PNGdec 1.1.6 draw-callback continuation, row conversion, pitch checks, and seek behavior"
[3]: https://raw.githubusercontent.com/bitbank2/PNGdec/1.1.6/src/PNGdec.cpp "PNGdec 1.1.6 open/decode return handling and 00BBGGRR alpha background contract"
[4]: https://raw.githubusercontent.com/Bodmer/TFT_eSPI/V2.5.43/TFT_eSPI.cpp "TFT_eSPI 2.5.43 text metrics, text drawing, primitive implementation, viewports, and byte-order defaults"
[5]: https://raw.githubusercontent.com/Bodmer/TFT_eSPI/V2.5.43/Extensions/Sprite.h "TFT_eSPI 2.5.43 sprite inheritance, allocation sizes, and sprite image APIs"
[6]: https://raw.githubusercontent.com/Bodmer/TFT_eSPI/V2.5.43/Fonts/GFXFF/FreeSansBold9pt7b.h "Bundled FreeSansBold9pt7b glyph metrics and ASCII-only range"

API signatures were also checked in [TFT_eSPI 2.5.43's header](https://raw.githubusercontent.com/Bodmer/TFT_eSPI/V2.5.43/TFT_eSPI.h), and sprite swap restoration in [Sprite.cpp](https://raw.githubusercontent.com/Bodmer/TFT_eSPI/V2.5.43/Extensions/Sprite.cpp). The [official Arduino 2.5.43 archive](https://downloads.arduino.cc/libraries/github.com/Bodmer/TFT_eSPI-2.5.43.zip) supplied the matching font tables for the pixel calculations. Repository build evidence is `.source/releases/ClockOSv2.8.md:22–33`; TFT setup is `.source/install/install.py:71–79`; icon size/conversion is `.source/install/install.py:47,590–593`.

## Ordered implementation checklist

1. **Fix framebuffer ownership first:** remove `drawStatusBlock()`/`drawRightPanel()` calls from the Settings handlers, guard wizard/alarm ownership in `refreshSdState()`, and make background events invalidate rather than enter/rebuild pages.
2. **Repair PNG correctness and safety:** keep the `int` callback, return 1 to continue, propagate decode failures, check handles and dimensions before conversion, guard `lineBuf`, constrain weather assets to their 96 × 96 slot, and close all failure paths.
3. **Make image composition explicit:** correct alpha/background encoding, restore swap state, dispatch TFT versus sprite `pushImage()` correctly, discard partial corrupt-image output, and retain a complete vector fallback.
4. **Define shared geometry and bounded text helpers:** establish scale/padding, safe `c_str()` metric/draw calls, UTF-8/display fallback, ellipsis edge cases, bearing/descender margins, clipping, and bounded word wrapping. Keep raw connection and persistence data unchanged.
5. **Resolve the concrete layout collisions:** Wi-Fi status/icon, battery percentage/Settings control, Settings title/detail/toggle budgets, password tail, Alarm time/mode/date bands, empty-calendar/city labels, and wrapped scan/Wi-Fi/Weather/Factory Reset text. Keep moved draw and hit rectangles synchronized.
6. **Separate page entry from repaint:** prevent PIN/input/draft resets during paint; preserve all wizard, alarm, offline clock, appearance, navigation, and confirmation behaviors.
7. **Introduce page-aware dirty keys and snapshots:** include subpages, overlays, theme generation, full visible time, and SD/asset state. Split weather from calendar, animate only changed Wi-Fi frames, sample battery separately, and reset caches on entry/update sessions.
8. **Add checked, reusable small buffers:** prioritize the Wi-Fi glyph, text fields/rows, weather slot, and clock strip. Keep direct clipped fallbacks, avoid full-screen/double buffers and per-frame allocation, and measure free/largest heap blocks under TLS/JSON load.
9. **Apply optional smooth primitives selectively:** use the exact 2.5.43 signatures, correct parent backgrounds, safe radii/dimensions, and fringe-aware invalidation. Keep Home black and preserve theme/SD-state semantics.
10. **Fix wake/boot ordering:** render the complete intended frame with GPIO27 dark, then enable the backlight; preserve rotation, calibration, sleep commands, and intentional Home wake behavior.
11. **Build the proposed source in a separate implementation task:** use the recorded ESP32 core/FQBN/TFT flags and exact dependency versions; resolve actual compiler diagnostics, including prototype/default-argument and PNG callback types. Record RAM/flash use and leave release artifacts untouched until a real build passes.
12. **Validate rendering explicitly before claiming completion:** exercise long ASCII and UTF-8 SSIDs/cities/titles, empty/shortening fields, all themes, 0/1/2/100% progress, oversized/corrupt/transparent/missing PNGs, allocation failure, SD changes during wizard/alarm/editor, repeated Settings toggles, minute/date/12-hour-width transitions, Wi-Fi animation versus connected stability, and sleep/wake. Use render-call/dirty-rectangle instrumentation and deterministic geometry checks in a later authorized test task; then perform physical Hosyond display/backlight/touch checks. **No physical validation has been performed by this review.**
