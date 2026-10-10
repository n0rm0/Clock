# ClockOSv2.8 — StandBy-inspired home, welcome, and ambient-view review

Retain the existing **240/240 landscape interaction split**, make the seven-segment time the dominant element, replace the bright dividing rules with quiet, inset surfaces, and give weather and cached assignments independently redrawable regions. Keep the current one-minute black-panel sleep policy; “StandBy-inspired” describes the awake screen’s appearance, not a new always-on mode.

**Review only, 2026-10-10.** This is a source inspection and implementation specification, not an implemented redesign. No application source, configuration, manifests, TODOs, binaries, tests, or Git metadata were changed. No firmware build, display capture, physical touch test, backlight measurement, or hardware validation was performed for this review. Coordinates and typography below are proposed logical pixels for `TFT_W = 480`, `TFT_H = 320`, not measurements from the Hosyond display.

## 1. Preserve the implemented contract, not historical completion claims

The supplied [TODO](../todo.md), [conventions](../agent.md), [README](../../README.md), [current sketch](../../.source/uncompiled/updates/ClockOSv2.8/ClockOSv2.8.ino), and [configuration](../../.source/uncompiled/updates/ClockOSv2.8/config.h) were read. Only the installer’s weather-icon sizing and Classroom cache construction were additionally inspected because they directly determine what the home renderer receives. No other release sketch, compiled image, updater implementation, or installer UI was reviewed.

`config.h:10–20` specifies `TFT_W`, `TFT_H`, `TFT_ROTATION = 3`, `TFT_BACKLIGHT_PIN = 27`, and calibration `{343, 3436, 266, 3381, 1}`. Preserve these values and the `ensureCalibration()` call. Do not rotate a mockup differently, reload SD calibration, change the pressure threshold, or “correct” the physical orientation as part of visual work.

The older conventions still describe indoor temperature/humidity and a partly pending repository baseline. The later TODO and actual firmware specify outside weather only. Do not introduce room sensors, humidity, an RTC, a charging detector, motion detection, or an ambient-light sensor. The README’s release/build statements are repository documentation, not validation performed by this review.

### Observations that materially affect the design

- **There is no rendered next-alarm row in this sketch.** `drawTimeBlock()` draws time, AM/PM, and date; `drawStatusBlock()` draws status and Settings; `drawRightPanel()` draws weather and Calendar. `alarmTimeText()` and `alarmRepeatText()` exist, but neither is used on the home screen. No next-occurrence selector is implemented. The checked historical TODO item cannot establish that this view is present in v2.8. Reserve and implement a read-only next-alarm summary as a recommendation, without claiming to preserve an existing rendered row.
- **The home assignment is not selected as the global nearest upcoming item.** `loadClassroomCache()` copies up to eight assignments and sets `nextAssignment`, `nextAssignmentCourse`, and `nextAssignmentDue` from index zero. `assignmentEpochs[8]` is declared but never populated or consumed. The installer requests `dueDate asc` within each course, then appends courses’ items without a global sort or overdue filter (`install.py:647–678`). Preserve the currently selected cached title and due date in this visual change; a genuine nearest-due selection is a separate data-behavior fix.
- **There is no distinct ambient screen or `S_AMBIENT` state.** The ambient experience is `S_HOME` while awake, followed by `screenSleeping` and an entirely black, backlight-disabled panel. `screenBeforeSleep` is recorded but not used to restore a page; `wakeDisplay()` always calls `showHome()`.
- **The date’s visible position and its touch action disagree.** The date is centered at `(120,180)` in `drawTimeBlock()`, but `loop()` routes the left side above `y=190` to `showAlarmEditor()`. The format-toggle zone starts at `y=190`. Move the date’s visible content into its actual zone rather than moving the zone.
- **The README’s battery/settings tap description is broader than the code.** Battery-region taps at `94 <= x < 166`, `y >= 270` fall through to the 12/24-hour toggle, not Settings. The Settings route starts at `x=166`. Preserve this implemented dispatch unless a separate interaction change is approved.
- **SD state is misrepresented specifically on home.** `drawStatusBlock()` calls `drawSdCardIcon(70,294,sdPresent,false)`, so an inserted but unready card is shown filled without an X. Other pages use `sdOk` and `sdPresent && !sdOk`. Use those same readiness states on home; do not change preparation or persistence semantics.
- **Some home renderers can overwrite Settings.** After General changes, `loop():1930` calls `showSettingsPage(); drawStatusBlock();`. After Calendar changes, `loop():1946` calls `showSettingsPage(); drawRightPanel();`. Both home drawing functions lack a screen guard. A new home renderer must not preserve this cross-page painting defect.

Apple describes StandBy as a landscape, distance-readable presentation of clocks and widgets, including a night display adapted to ambient light. [1] A secondary description illustrates adjacent clock/calendar widgets and a larger clock view. [2] Borrow the glanceable hierarchy and restrained black background, **not** iPhone charging requirements, widget swipes, Siri, automatic red night mode, snooze, or always-on behavior.

## 2. Exact existing render and event paths

Line numbers refer to the supplied `ClockOSv2.8.ino` and are useful anchors, not a substitute for function names.

**First-run entry.** `setup():1720–1769` initializes the TFT, rotation, backlight, sprite, SD layout, calibration, settings, alarm data, and Classroom cache. After attempting saved networks, it chooses `showWelcome()`, `showClockEditor()`, or `showHome()`. `showWelcome():775–790` sets `S_WELCOME` and draws two buttons. The `S_WELCOME` branch of `loop():1788–1792` accepts any touch at `y >= 190`, selects the left or right path at `x=240`, and calls `openWifiScanner(S_WELCOME)` or `showClassroomSetup()` respectively. Preserve the subsequent Classroom/sync setup, `settings.setupComplete`, saved-network resume, and offline clock-entry flow.

**Home entry.** `showHome():1689–1695` sets `S_HOME`, fills black, draws the vertical rule at `x=240` and horizontal rule at `y=225`, then calls `drawTimeBlock()`, `drawStatusBlock()`, and `drawRightPanel()`. It is also called after page navigation, alarm dismissal, manual clock editing, wake, and update handling. This is the correct full-frame invalidation boundary.

**Clock drawing.** `SEG_MASK[10]:964`, `drawDigit():966–975`, and `drawBigTime():977–995` provide asset-free seven-segment digits. `drawBigTime()` hardcodes `w=44`, `h=92`, `t=10`, `gap=8`, and colon width `cw=20`; a five-character time occupies 228 pixels. `drawTimeBlock():997–1014` clears `(0,0,240,224)` every invocation, considers local time usable when `getLocalTime(&ti,0)` succeeds and `ti.tm_year > 120`, formats `HH:MM` or `H:MM`, adds AM/PM for 12-hour time, and formats the date as `DD/MM/YYYY`. Invalid time displays `--:--` and omits the date/meridian.

**Weather.** `fetchWeather():340–376` uses Open-Meteo current `temperature_2m` and `weather_code`, explicitly requests Fahrenheit, uses `WEATHER_LAT`/`WEATHER_LON` for Philadelphia, or uses the saved manual city coordinates. A failed fetch returns without clearing a previously valid sample. `WEATHER_REFRESH_MS` is ten minutes. Preserve these sources, units, fallback location, and retention of last-good values. `weather.label` is not drawn on home; do not add a long conditions paragraph.

`weatherIconForCode():298–307`, `drawWeatherFallback():378–414`, `drawWeatherIcon():416–423`, `drawPng():289–296`, and `pngDraw():283–287` form the icon path. A vector fallback is drawn first, then an installed PNG is overlaid from `ICON_DIR`. The installer’s `ICON_SIZE = 96` and conversion at `install.py:590–593` set the longest PNG dimension to 96 pixels with alpha. Preserve the SD-independent fallback and do not require new assets for a usable home view.

**Right column.** `drawRightPanel():1114–1130` clears the entire `(241,0,239,320)` column. It draws the icon at `(312,2)`, temperature at `(360,112)`, an unbounded city string at `(360,137)`, the rule at `y=160`, and Calendar content below. Its assignment visibility predicate is exactly `settings.calendarEnabled && settings.notifications && classroomAssignmentCount > 0`. `settings.classroomEnabled` instead gates cache loading in `loadClassroomCache():690–712`. Keep that distinction; do not silently change feature gating while replacing the renderer.

**Status.** `drawStatusBlock():1077–1091` clears `(0,226,240,94)`, calls `rssiLevel()`, `drawWifiSignal()`, `drawSdCardIcon()`, `readBatteryPct()`, and `drawBattery()`, optionally draws a percent, then puts “Settings” in a 58-pixel pill. The pill uses `F12B` without a width check. `readBatteryPct():527–536` takes ten ADC samples with 2-ms delays and estimates a percentage; it is not a proven charging or battery-health signal. Keep `settings.showBatteryPercent` and the existing red/yellow/green battery thresholds.

`refreshSdState():1093–1112` checks roughly every three seconds, may create missing ClockOS folders through `ensureClockOsSdLayout()`, updates `sdPresent`/`sdOk`, and redraws home status on change. `clockOsSdReady()` and `CLOCKOS_SD_DIRECTORIES` remain the storage truth; visual work must not weaken their readiness checks.

**Awake maintenance.** In `loop():1853–1881`, home touches take precedence, `alarmDueNow()` checks the enabled alarms, and a roughly one-second block checks the displayed minute. Status is repainted every 30 seconds, weather fetch/redraw uses the ten-minute interval, Classroom cache loading/redraw uses 60 seconds, and OTA uses `UPDATE_CHECK_MS` (one hour). Despite the README/TODO’s once-per-second assignment-display wording, the source does not reload that cache every second. Preserve cadence and network behavior; compare display state each second without calling Google or decoding icons each second.

**Alarm overlay.** `showAlarmRinging():1266–1276` sets `alarmRinging`, turns the green LED on via active-low GPIO, and replaces the page. In `S_HOME`, any accepted touch dismisses through `dismissAlarm():1278–1285`; a one-time alarm is disabled and saved, while a repeating alarm remains enabled. Preserve this route, not just the visible Dismiss button. Background home painting must never overwrite the ringing overlay.

**Sleep and wake.** The top of `loop():1776–1786` handles `screenSleeping` before the home branch and sleeps home after `60000UL` since `lastActivity`. `readTouch():190–208` uses pressure 350, a 220-ms accepted-touch interval, updates `lastActivity`, and detects the existing swipe-back gesture. `sleepDisplay():1620–1628` fills black, sends display-off/sleep-in commands, disables GPIO27, and marks sleep. `wakeDisplay():1630–1638` holds the backlight off during sleep-out/display-on, then enables it **before** `showHome()` paints. The waking touch is consumed by the sleeping branch; preserve that safety behavior.

## 3. Concrete 480 × 320 layout, with unchanged hit routing

Use two equally weighted logical columns, but do not render a full-height divider. Black space at the split provides the separation. Keep the top-left clock dominant through scale rather than a colored box. Use restrained 16-pixel-radius surfaces only where they aid grouping; avoid gradients, blur simulations, moving backgrounds, heavy outlines, and scrolling home text.

The following names are **proposed new layout constants**, not existing symbols:

```cpp
HOME_LEFT_W          = 240;
HOME_RIGHT_X         = 240;
HOME_WEATHER_SPLIT_Y = 168;
HOME_FORMAT_SPLIT_Y  = 190;
HOME_STATUS_SPLIT_Y  = 270;
HOME_WIFI_END_X      = 52;
HOME_SD_END_X        = 94;
HOME_SETTINGS_X      = 166;

// Rectangles are x, y, width, height, with half-open extents.
CLOCK_SURFACE   = {  8,   8, 224, 174 }; // radius 16; ends before y=190
DATE_SURFACE    = {  8, 198, 224,  64 }; // radius 14; within format-toggle zone
WEATHER_SURFACE = {248,   8, 224, 152 }; // radius 16; entirely above y=168
AGENDA_SURFACE  = {248, 176, 224, 136 }; // radius 16; entirely below y=168
```

These surfaces stay on their respective sides of every implemented boundary. A clock surface may use pure black instead of a contrasting fill to make the digits feel less boxed in. Do not change the logical rectangles to match rounded corners: current margin/gutter taps still have an action.

### Left: clock, next alarm, date, and status

**Time:** change `drawBigTime()`’s geometry to `w=40`, `h=92`, `t=9`, `gap=6`, `cw=16`, drawn at `y=24` with the existing `areaW=240` centering. A five-character time is exactly `4*40 + 16 + 4*6 = 200` pixels wide and occupies `x=20..219`; a four-character 12-hour time occupies 154 pixels. Both fit inside the 224-pixel inset area. Preserve `SEG_MASK`, variable-length 12-hour formatting, leading-zero 24-hour formatting, and dash placeholders. Pass a semantic clock ink color instead of hardcoding `COL_WHITE` in the digits and colon.

**Meridian:** reserve a stable line centered at `(120,134)` using actual 12-point `F18B` or, if the measured font bounds crowd adjacent content, 9-point `F12B`. Show AM or PM in 12-hour mode. In 24-hour mode keep the space, with an optional quiet “24-hour” label; never shift the digits or alarm row when changing format. For invalid time, use a bounded “Time unavailable” label, not a misleading “Tap to set” instruction: this region opens Alarms, not the manual clock editor.

**Next-alarm row:** reserve `(16,152,208,34)`, center the existing `drawAlarmIcon()` at `(30,169)`, and start a 9-point summary at `(50,169)` with `ML_DATUM`, fitted to 168 pixels. Examples are `7:00 AM Mon`, `12:59 PM 31/12`, or `No alarms set`. This remains inside the existing alarm-opening zone. The row is new presentation derived from existing alarms, not a new alarm type or schedule. If clock time is unavailable, show `Alarms: time unavailable` rather than asserting a next occurrence. Do not fabricate a default 7:00 alarm when no enabled alarm exists.

**Date/format:** center a bounded weekday/date line such as `Sat 10/10/2026` at `(120,219)` with `F12B`, maximum width 200 pixels. Center `12-hour · tap to switch` or `24-hour · tap to switch` at `(120,246)` with `F12`, maximum width 200. Render a supported ASCII separator if the included font does not contain the middle dot. The displayed date remains `DD/MM/YYYY`; the added short weekday comes from the same `tm` snapshot. Invalid time uses an explicit placeholder, not the last date painted beneath an invalid clock. These two lines fit the real format-toggle region instead of straddling the alarm boundary.

**Footer:** use `y=270..319` as a quiet black status strip with no horizontal rule at `y=225`. Keep four separate hit cells. Wi-Fi stays centered at `x=26` with the existing `(26,294)` signal origin. SD is centered at `(73,294)`, inside `52..93`. Put the 32-pixel battery body near `(114,281)` and center its optional percent at `(130,306)` using `F12`, bounded to 64 pixels; when percent is hidden, vertically center the battery instead. Place `drawSettingsGear(202,285,...)` above a fitted `Settings` label centered at `(202,308)`, maximum width 68 pixels. This removes the undersized text pill without removing the named Settings action. Battery-region taps still toggle time format in this compatibility pass; do not expand the Settings target into that cell.

### Right: weather and assignment

**Weather:** keep the existing 96-pixel asset footprint at `(256,18)`, wholly within the weather surface. Give temperature a separate region `(360,42,104,48)`, centered at `(412,66)`, using `F24B` (actual 18-point bold) where its measured width fits. Fall back to `F18B` for a longer negative or three-digit value instead of clipping. Preserve rounded Fahrenheit temperature with the degree mark and invalid `--°`; no unit selector or indoor reading is introduced. Put the bounded city line at `(360,139)` with `F12`, maximum width 192 pixels. Preserve the current invalid-weather Philadelphia placeholder and last-good sample on fetch failure in the initial visual pass. A future stale-data label must be based on an actual successful-fetch timestamp, not on `lastWeatherFetch`, which records an attempt.

This horizontal icon/temperature composition uses the column’s width and leaves city text on its own line. It avoids adding oversized weather art or a conditions label above Calendar. PNG and fallback content must never extend into the `y >= 168` calendar target.

**Assignment:** place `Calendar` at `(264,188)` with `F12B` and `TL_DATUM`; an optional two-stroke disclosure chevron sits around `(456,198)`. Use two title lines at `(264,216)` and `(264,238)`, each at most 192 pixels wide with `F12`. Word-wrap before ellipsizing the second line; do not use a marquee. Place the due date in its own bounded line around `(264,288)` with `ML_DATUM`. Preserve the existing cached due-date representation (`YYYY-MM-DD`) unless a separately documented presentation formatter is added. Do not rewrite the cache merely to display a date.

The surface fits a 20-pixel heading line, two approximately 20-pixel title lines, and a separate due line. `nextAssignmentCourse` need not be added: it is loaded but not currently visible, and adding a fourth information row would sacrifice readable title space. Keep `No upcoming assignments` when the existing visibility predicate is false, wrapping it within the same region. An optional clearer disabled-state label must not change routing: the whole right lower half remains tappable to Calendar regardless of cache or feature flags.

### Touch dispatch is an explicit compatibility invariant

Retain the order of the `S_HOME` tests in `loop()`. Expressing them through shared constants is safer than separately hardcoding drawing and hit testing, but the resulting mapping must remain:

1. `x >= 240 && y < 168` → `showWeatherPage()`.
2. `x >= 240 && y >= 168` → `showCalendarPage()`.
3. `x < 52 && y >= 270` → `showWifiPage()`.
4. `52 <= x < 94 && y >= 270` → `showSdCardPage()`.
5. `x >= 166 && y >= 270`, after the right-column tests → `showSettingsPage()` for the remaining left region.
6. Remaining `x < 240 && y >= 190` → toggle `settings.use24Hour`, call `saveSettings()`, and refresh the changed time/meridian/date/alarm text.
7. Remaining `x < 240 && y < 190` → `showAlarmEditor()` (the existing alarm list entry point).

The battery cell and blank space in the left format zone deliberately retain their current format-toggle behavior. During `alarmRinging`, any accepted touch still dismisses instead of entering a tile. The first waking touch still only wakes. Do not add swipeable home widgets, a home profile button, long-press configuration, snooze, or a new clock-editor tap route.

## 4. Palette, typography, and icon rules

### Local semantic palette

Add a proposed `StandbyPalette`/`makeStandbyPalette()` for the home and welcome renderers, rather than redefining `COL_*` or replacing global `UI_*` values used by Settings and other pages. The current home is already black regardless of selected theme; keep that dark presentation while allowing the selected theme’s decorative accent to carry through. Preserve `appearanceTheme`, `applyAppearanceTheme()`, and `saveSettings()`; no new theme ID, JSON package, manifest, or preference schema is required.

Use these exact default RGB/RGB565 values, computed using the existing `C(r,g,b)` conversion convention:

- `background`: `#000000`, `C(0,0,0)`, `0x0000`.
- `surface`: `#111318`, `C(17,19,24)`, `0x1083`.
- `time`: `#F5F7FF`, `C(245,247,255)`, `0xF7BF`.
- `primary`: `#E5E9F0`, `C(229,233,240)`, `0xE75E`.
- `secondary`: `#ABB4C4`, `C(171,180,196)`, `0xADB8`.
- `divider`, only if a local separator is necessary: `#282C36`, `C(40,44,54)`, `0x2966`.
- Default decorative accent: `#5E9BFF`, `C(94,155,255)`, `0x5CDF`.
- Default primary welcome button: `#0066D6`, `C(0,102,214)`, `0x033A`, with `#FFFFFF`, `0xFFFF`, text.

For non-default appearances, obtain decorative accent from `UI_BLUE` and the matched action pair from `UI_ACTION`/`UI_ON_ACTION`; keep important home text on the neutral palette. Some theme blues are too dark for small text on black, so decorative accent is not an automatic body-text color. Do not alter the shared `UI_RED`, `UI_YEL`, or `UI_GREEN` thresholds to make a monochrome screenshot.

Nominal sRGB contrast calculations for the default tokens are 19.63:1 for time on black, 15.26:1 for primary on surface, 8.90:1 for secondary on surface, and 5.42:1 for white on the primary button. These are mathematical RGB comparisons, **not** panel measurements or a hardware accessibility certification. Verify the actual RGB565 rendering, inversion, color order, viewing angle, and low-light readability on the target board before sign-off. Black paint alone does not dim an illuminated ST7796S backlight.

### Existing fonts, measured widths

`F12B`, `F18B`, `F24B`, and `F12` at `ClockOSv2.8.ino:149–152` are historical names, not their actual sizes: they map to FreeSans Bold 9-point, Bold 12-point, Bold 18-point, and regular 9-point respectively. Use the included font family; do not introduce SF Pro or a downloaded font just to mimic iOS. Keep seven-segment time procedural.

`txt()` sets the free font, text color, and datum but does not clear or clip its region. `fitText()` measures width but truncates `String` one byte at a time and appends ASCII `...`. Keep width measurement through `tft.textWidth()`, but add a bounded two-line wrapper for assignment titles and a safe truncation policy for UTF-8 cache/city text. Do not split a multibyte codepoint or rely on FreeSans7b to display every Unicode character. Use a documented display-only transliteration/replacement fallback where a glyph is unsupported; leave the original cache untouched. Draw the degree symbol procedurally if the installed free font’s encoding does not render the existing literal reliably.

For each line, define the font, datum, maximum width, and measured pixel-height allowance explicitly. Clear its owning region before replacing shorter text; free-font transparent drawing alone leaves stale glyphs. Measure `Settings`, `100%`, `Philadelphia`, a long city, `23:59`, `12:59 PM`, and the date/helper strings with the actual linked fonts before treating the proposed coordinates as final. Do not repeatedly shrink every title: wrap first and ellipsize only where necessary.

### Icons stay procedural or use existing weather assets

Keep status glyphs optically consistent at roughly 24–32 pixels, with clear 1–2-pixel strokes at this resolution. Reuse `drawWifiSignal()`, `drawBattery()`, `drawSdCardIcon()`, `drawSettingsGear()`, and `drawAlarmIcon()` rather than importing an unrelated icon set or Apple symbol font. Do not add filled boxes behind every status glyph.

`drawWifiSignal()` already clears a 40×28 footprint using its supplied background; pass the correct local background. Its three rings and dot must continue to convey RSSI. Animate only while the existing state identifies a connection attempt, not while stably connected. Keep the connected/disconnected indication and existing RSSI thresholds in `rssiLevel()`.

For home SD, pass `inserted = sdOk` and `invalid = sdPresent && !sdOk`. An absent card stays outlined, an unready present card has the X, and a ready card is filled. `drawSdCardIcon()` currently uses global `UI_BACK` to fill an absent icon and a fixed blue; add explicit background/ink parameters or a home-specific wrapper so light theme backgrounds do not create a pale rectangle on black. Preserve the existing default arguments for other page callers, which were not redesigned here.

Center weather by its allocated 96×96 footprint, and normalize the vector fallback to the same optical center. The current clear-day center is `x+48`, but cloud parts extend to about `x+95`; variants should not visibly jump between conditions. Keep cloud/rain/snow/sun distinctions and a fallback that works without SD.

The current PNG overlay is not a reliable compositing guarantee: `pngDraw()` converts with a white background argument (`0xffffffff`) and pushes opaque scanlines, while `drawPng()` ignores the decode result after a successful open. Transparent pixels or a partially corrupt decode can cover the fallback rather than preserve it. Verify the installed PNGdec version’s transparency/decode contract before changing it. Render a successfully decoded PNG against the actual surface, preferably in a bounded tile; on failure clear the icon region and redraw the vector fallback. Reject or bound unexpected dimensions before decoding: `lineBuf[480]` is finite, and there is no current home-region clipping in `pngDraw()`. Keep existing `ICON_DIR` and installed files; no installer asset-size change is needed.

## 5. Next-alarm summary without changing scheduling

A proposed `findNextAlarm(now, index, due)` should be a **pure, read-only presentation helper** over `alarms[0..alarmCount)` and `MAX_ALARMS = 4`. It must not enable alarms, write SD files, alter `alarmDraft`, update `lastAlarmMinuteKey`, call `showAlarmRinging()`, or replace `alarmDueNow()`.

Use the same usable-time condition as the clock. For enabled one-time alarms, construct the stored civil date/time and reject already expired or invalid dates. For repeat alarms, inspect today through the next seven civil days, applying `repeatMask` with bit zero Sunday, just as `alarmDueNow()` does. Do not add fixed 86,400-second increments across DST; construct each candidate using `TIME_TZ` and `tm_isdst = -1`, and check normalized dates/times so an impossible local time is not advertised as a different scheduled time. Check `daysInMonth()` before normalizing a stored one-time date.

The scheduler is minute-based. An occurrence in the current minute can be shown as due now only if that minute has not already been consumed by `lastAlarmMinuteKey`; otherwise select the next eligible occurrence. Use the same minute-key expression as `alarmDueNow()` if this distinction is displayed. Resolve equal occurrence times in array order to match the existing alarm loop. If DST ambiguity or unavailable time prevents a dependable next occurrence, show a neutral summary rather than asserting a guaranteed firing time.

Format the time through existing `alarmTimeText()` so it follows `settings.use24Hour`. Add a bounded home-only weekday/date suffix; do not change `alarmRepeatText()` or the alarm editor’s date representation incidentally. Recompute the displayed summary at home entry and when time/format, alarm data, or the next occurrence changes. After `dismissAlarm()`, recompute from the existing updated enabled flags. Do not show all four alarms on home or move alarm editing into this row.

Likewise, preserve `nextAssignment` and `nextAssignmentDue` as the current cache-derived fields. Calling the tile “Calendar” is accurate; calling it a verified globally nearest future assignment is not. Correct sorting, overdue handling, `dueTime`, empty/stale-cache reset, and honoring a changed Classroom toggle should be tracked as separate data fixes, not silently folded into the layout.

## 6. Incremental redraw specification

### Split drawing responsibilities, then invalidate deliberately

Keep `showHome()` as the public entry point. It should set `S_HOME`, paint the static background/surfaces once, invalidate the home display snapshot, and paint all dynamic fields. Remove the full-height `0x7BEF` rule and the unrelated `y=225` rule from this entry function.

Preserve existing names as small orchestration wrappers where useful:

- `drawTimeBlock()` → clock digits/placeholder, meridian, date/format, and the proposed alarm summary, each with its own dirty check. No unconditional `(0,0,240,224)` clear.
- `drawStatusBlock()` → independently dirty Wi-Fi, SD, battery, percent, and static Settings affordance. No unconditional 240×94 clear or ADC read merely because SD state changed.
- `drawRightPanel()` → separate proposed `drawHomeWeather()` and `drawHomeAssignment()` dirty regions. A Classroom refresh must not erase/redraw weather, and a weather attempt must not repaint Calendar.

New names such as `HomeDisplaySnapshot`, `invalidateHome()`, and `renderHomeDirty()` are proposals. Add a guard at the home rendering boundary: **only paint when `screen == S_HOME && !screenSleeping && !alarmRinging`**. Non-home calls may mark state dirty but must not paint. Replace the post-Settings `drawStatusBlock()`/`drawRightPanel()` calls at lines 1930 and 1946 with invalidation, or make the wrappers guarded, so Settings remains intact. `refreshSdState()` can retain its existing other-page behavior; its home path should mark only SD/status dirty.

### Compare what is actually displayed

A compact snapshot should include usable-time validity, formatted time, meridian, local date/weekday, `settings.use24Hour`, alarm-summary key, displayed rounded temperature, weather icon/code/validity and city, assignment visibility/title/due, Wi-Fi state/RSSI level/connecting frame, SD readiness/presence, battery percentage and percent visibility, and a palette-generation key.

Compare formatted temperature rather than every floating-point change. Include actual hour and date, not only `tm_min`, so NTP/manual corrections, invalid-to-valid time transitions, midnight, timezone transitions, and a 12/24-hour toggle do not leave stale fields. Do not assume the function-local `lastMin` cache is sufficient after all page-entry paths. Reset/invalidate the snapshot on `showHome()`, wake, theme changes, and after a ringing overlay is removed.

For digit-only changes, clear and redraw the affected fixed cell, including formerly lit segments. A four-character/five-character change alters centered geometry; clear and redraw the whole `(20,24,200,92)` clock area when the character count or format changes. The colon is static. No second ticker, blink, or fade is required. Date and meridian are separate fields, so a routine minute change does not erase them or the next-alarm row.

Cache the last ADC result and obtain a new battery sample on the existing roughly 30-second status cadence. SD changes reuse that battery result. Poll Wi-Fi display state in the one-second home maintenance pass; a changed state or RSSI level invalidates only the glyph. While the existing `WL_IDLE_STATUS` connecting state is active, refresh that glyph on the helper’s 180-ms frame interval; stop animation when stable. This repairs home’s effectively static connecting glyph without rescanning or altering connection logic.

Keep `WEATHER_REFRESH_MS`, the 60-second Classroom cache reload, the three-second SD check, and `UPDATE_CHECK_MS`. On completion, compare the resulting snapshot and redraw only changed fields. In offline mode `fetchWeather()` returns before updating `lastWeatherFetch`; do not let a repeatedly due attempt force `drawRightPanel()` every second. A failed fetch that leaves all displayed weather values unchanged causes no weather redraw. Loading an unchanged Classroom cache causes no assignment redraw. Do not increase Google authorization or network-sync frequency.

### Bound clearing and memory use

A dirty field owns a rectangle entirely inside its surface. Clear with that surface’s color, not global `UI_BACK` or arbitrary black, then redraw only the field. Keep clears away from rounded corners. The icon region is exactly 96×96; title lines are 192-pixel-wide text regions; footer cells remain inside their hit cells. Batch compatible TFT operations with `startWrite()`/`endWrite()` while respecting the current PNG decoder’s own write boundaries.

Direct region painting is a valid first implementation. If a noticeable clear-then-draw flash remains, use small, checked sprites for the affected region and refactor the drawing target explicitly; simply reusing the global `spr` will not redirect helpers that draw to `tft`. A 200×92 16-bit clock tile costs 36,800 bytes; a 96×96 icon tile costs 18,432 bytes. Check allocation and fall back to direct bounded painting. Do not add a full 480×320 16-bit framebuffer (307,200 bytes), assume PSRAM, or commandeer the existing 100×100 loader `spr` allocated in `setup()`.

If PNG decoding is redirected into a tile, update `pngDraw()`’s output target and clipping deliberately; its current global `tft.pushImage()` cannot produce an atomic tile overlay. Do not repeatedly decode unchanged weather assets on minute ticks, SD status redraws, cache reloads, or connected Wi-Fi frames.

## 7. Welcome and black-panel ambient behavior

### Welcome: quieter, but exactly the same choices

`showWelcome()` already has a minimal clock mark and the correct two setup choices; refine it rather than introduce a new wizard. Use the same local dark palette as home. Remove or greatly quiet the large 420×172 card, keep a small procedural dial centered near `(240,66)` with a roughly 20-pixel radius and restrained accent hands, and use `F24B` for `ClockOS` around `(240,116)`.

Keep short, measured lines around `(240,153)` and `(240,181)`, such as `Time, weather, and your day` and `Choose how to get started`. Preserve the visible button rectangles `(36,222,196,44)` and `(248,222,196,44)` and the labels `Set up Wi-Fi` and `Set up offline`. Keep the later-Settings reassurance around `(240,294)`, fitted to 416 pixels. Avoid the current hardcoded light gray secondary text when a light theme is loaded; the local dark palette makes this welcome screen predictable for every saved appearance.

`drawAppleButton()` currently reads global semantic action/surface colors. Add a scoped home/welcome button helper or optional explicit palette parameters with unchanged defaults for other callers. The primary welcome action can use the existing matched `UI_ACTION`/`UI_ON_ACTION` pair; the secondary uses the local dark surface and neutral ink. Do not temporarily overwrite global `UI_*` colors to draw this page.

Preserve the **broad existing welcome hit behavior**: any touch at `y >= 190` selects a setup path by `x < 240` versus `x >= 240`, including margins and the bottom reassurance text. This is surprising but narrowing the hit region would be an interaction change, not a visual preservation. If strict button-only hit testing is desired later, document and approve it separately. A short local pressed-state redraw can acknowledge the chosen half without repainting the whole welcome page or delaying network setup unnecessarily.

### Ambient: black means off, and wake should reveal a completed frame

Keep the one-minute timeout, home-only automatic sleep, `readTouch()` activity updates, `0x28`/`0x10` sleep commands, `0x11`/`0x29` wake commands, and active-high GPIO27 control through `setDisplayBacklight()`. Do not replace them with a red clock that remains visible or a color-only “dim” state. No PWM brightness control or ambient-light automation is implemented.

Recommended incremental wake ordering:

1. Hold GPIO27 off through `setDisplayBacklight(false)`.
2. Send sleep-out, retain the existing 120-ms wait, then display-on and the existing 20-ms wait.
3. Clear `screenSleeping`, invalidate home, and call `showHome()` while the backlight is still off.
4. Reset `lastActivity` for the waking touch and enable the backlight **after the complete home frame has been drawn**.

This changes rendering order, not navigation, timeout policy, or the touch-consumption contract. It avoids revealing a black/partly drawn frame or a stale pre-sleep surface. Do not claim this eliminates hardware flashes until panel/backlight behavior is observed on the actual device. `screenBeforeSleep` may remain as-is; restoring arbitrary prior pages is not part of this home-only sleep redesign.

**Existing alarm safety limitation:** the sleeping branch returns before `alarmDueNow()` is checked, alarm checking occurs only in `S_HOME`, and the inactivity sleep test precedes the `alarmRinging` handling. Consequently this source can miss alarm handling while asleep/on another page and can sleep an active alarm view after inactivity. This is visible statically and is a production sign-off blocker for alarm reliability. Do not silently rewrite scheduler placement or add alarm-triggered wake in a cosmetic patch; address it as an explicit follow-up behavior fix with separate validation. A next-alarm row must not imply that this limitation has been resolved.

## 8. Validation required before this design is called production-ready

The review provides concrete implementation bounds; it does not certify the current firmware as production-ready. First validate the renderer with the actual linked TFT_eSPI/FreeSans/PNGdec versions, then build using the project’s documented ESP32 core/library baseline. Do not publish a renamed release, binary, or manifest based on this document alone; any later implementation follows the repository’s release policy and real build evidence.

For source-level or captured-frame checks, exercise `9:59 → 10:00`, `11:59 AM → 12:00 PM`, `23:59 → 00:00`, format toggles, invalid-to-valid time, NTP/manual corrections within the same minute, no enabled alarms, repeat alarms, one-time alarms, and short/long titles. Verify no title, city, percent, or Settings text exceeds its region, and shorter replacements leave no glyph remnants. Include an empty/disabled cache and a failed weather request retaining a valid sample. Distinguish the current first-cached item from a true nearest-due selector in any test expectation.

Record redraw activity rather than infer it from comments: a minute change should update only the affected time/summary fields; an unchanged cache should redraw nothing; a weather change should not repaint the assignment tile; battery reads should not occur on every SD tick; stable Wi-Fi should not animate; and the Settings toggles at lines 1930/1946 should not invoke visible home painting over Settings. Ensure `alarmRinging` cannot be overwritten by a dirty-render flush.

Physical checks remain mandatory for logical boundaries near `x=52/94/166/240` and `y=168/190/270`, all four corners and center under the supplied calibration, portrait/landscape orientation, SD absent/unready/ready states, readable weather fallback, actual PNG alpha behavior, backlight-off blackness, wake ordering, and consumed first wake touch. Confirm one-minute sleep timing and the existing whole-screen alarm-dismiss route. These checks were **not performed** here. Alarm scheduling during sleep must be assessed in the separately approved safety fix, not hidden inside a layout acceptance screenshot.

## References

[1]: https://support.apple.com/guide/iphone/use-standby-iph878d77632/ios "Apple iPhone User Guide — Use StandBy to view information at a distance while iPhone is charging"
[2]: https://uk.pcmag.com/ios/149125/standby-mode-setup-how-to-turn-your-iphone-into-an-alarm-clock "PCMag — Clear the Bedside Clutter. StandBy Mode Turns Your iPhone Into an Alarm Clock"

## Ordered implementation checklist

1. Freeze the existing home/welcome dispatch boundaries, rotation, calibration, touch debounce/pressure, setup flow, weather source/units, persistence, and sleep timeout as compatibility constraints; explicitly separate alarm reliability and true nearest-assignment selection from the visual patch.
2. Introduce local layout constants and a local `StandbyPalette`; retain existing `appearanceTheme` storage and global `UI_*` behavior for non-home pages. Measure the actual included fonts against every proposed width and vertical allowance.
3. Refactor `showHome()` into one static background/surface paint plus full snapshot invalidation. Remove the bright `x=240` and `y=225` rules without changing hit routing.
4. Adapt `drawBigTime()`/`drawDigit()` to the 200×92 clock geometry and explicit ink; split time, meridian, date/format, and alarm-summary regions. Move the visible date into `y=190..269` and preserve all current format-toggle routes.
5. Implement the read-only next-occurrence presentation helper over existing enabled alarms, including repeat masks, one-time dates, unavailable time, current-minute consumption, and civil-time/DST constraints. Reuse `alarmTimeText()` without modifying scheduling or alarm-editor behavior.
6. Split `drawRightPanel()` into bounded weather and assignment renderers. Keep the current cached item, visibility predicate, due data, weather fallbacks, and fetch cadence; add measured city fitting and two-line title wrapping without mutating stored text.
7. Rework `drawStatusBlock()` into independent footer cells; fix the home SD readiness arguments and explicit absent-icon background, keep battery thresholds/percent preference, and replace the cramped Settings pill with a fitted gear/label affordance.
8. Add guarded incremental rendering and a complete displayed-state snapshot. Remove/guard home painting after Settings toggles, cache ADC samples on the existing cadence, animate only connecting Wi-Fi, and stop redraws after unchanged/failed weather or unchanged cache loads.
9. Bound PNG dimensions/output, verify the installed decoder’s alpha and result handling, and redraw the vector fallback after a failed decode. Add small checked sprites only if direct region painting measurably flashes; do not allocate a full-screen framebuffer or reuse the loader sprite blindly.
10. Refine `showWelcome()` with the same local dark palette, measured copy, and existing visible button geometry while preserving its broad lower-half touch choices and all subsequent setup screens.
11. Paint the entire invalidated home frame with GPIO27 still off during `wakeDisplay()`, then enable the backlight. Retain panel commands/delays, consumed wake touch, and one-minute black sleep; do not introduce always-on/red-night behavior.
12. Build from the real implementation, verify field bounds/redraw counts and overlay/page guards, and perform the outstanding physical display/touch/SD/PNG/backlight checks. Resolve the separately scoped sleeping-alarm safety limitation before making a production-ready alarm claim; only then create versioned release artifacts through the normal validated release process.
