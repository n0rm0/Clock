# ClockOSv2.8 detail-page review

**Review ID:** `2026-10-10-03`
**Date:** 2026-10-10
**Scope:** Wi-Fi and its scanner/keyboard, Weather, Calendar, Alarms, Appearance, Storage and its file viewer, existing display-related preferences, Software Update, and Factory Reset. Settings category details are included only where they expose these capabilities. The Settings root and home-screen redesign are outside this review.

## Basis and limits

This is a **static source review**, not an implementation or validation of a new UI. I read:

- `.agent/todo.md`
- `.agent/agent.md`
- `README.md`
- `.source/uncompiled/updates/ClockOSv2.8/ClockOSv2.8.ino` in full
- `.source/uncompiled/updates/ClockOSv2.8/config.h`
- The same sketch's `bootloader.h`, solely to verify Software Update behavior and progress reporting

No application source, configuration, manifests, TODOs, binaries, tests, or Git state were modified. No build, automated test, simulator, or physical-device test was performed for this review. Line references below refer to the reviewed v2.8 source and are navigation aids, not stable identifiers.

The source and README establish **480 × 320**, `TFT_ROTATION 3`, active-high `TFT_BACKLIGHT_PIN 27`, and touch calibration `{343, 3436, 266, 3381, 1}`. Preserve these values. The TODO explicitly leaves physical orientation, corner/center touch response, and other device checks open. Earlier checked items and older conventions are not evidence of validation performed here. In particular, the conventions contain historical planning text; they should not override the implemented outdoor-weather-only behavior or current JSON paths.

**Design direction:** use the existing Apple-inspired semantic palette, compact typography, inset grouped surfaces, restrained separators, and clear hierarchy—not a new UI framework, image-heavy mockup, or removal of controls to make the page look cleaner.

## 1. Current drawing and touch inventory

### Shared mechanisms

| Existing function/constants | Observed responsibility | Detail-page implications |
| --- | --- | --- |
| `Screen` / `screen` (line 77) | Enumerates `S_WIFI`, `S_WEATHER`, `S_CALENDAR`, `S_ALARM`, `S_CLOCK`, `S_SETTINGS`, `S_SDCARD`, `S_SDFILES`, `S_APPEARANCE`, `S_UPDATE`, `S_FACTORY_RESET`, and supporting screens. | There is **no `S_DISPLAY`**. Software Update is a transaction/status screen, not a normal Settings detail with controls. |
| `txt()`; `F12`, `F12B`, `F18B`, `F24B` (134–152) | FreeSans drawing with caller-selected datum; the macros actually map to 9 pt regular, 9 pt bold, 12 pt bold, and 18 pt bold. | Reuse these fonts. The macro names are not their actual point sizes. Many captions are drawn directly without fitting or wrapping. |
| `fitText()` (141) | Measures pixel width and appends ASCII `...`. | A useful foundation, but fitting a caption to one line is not a replacement for wrapping important instructions or reserving value/icon columns. |
| `applyAppearanceTheme()`; `UI_BACK`, `UI_KEY`, `UI_TEXT`, `UI_MUTED`, `UI_SEPARATOR`, `UI_SELECTION`, `UI_ACTION`, `UI_ON_ACTION`, `UI_DISABLED` (46–75) | Five hard-coded theme palettes, with semantic action/foreground colors separated from decorative `UI_BLUE`. | The necessary design tokens already exist. Several detail functions still use literal colors instead. |
| `drawBackHeader(title, showGlobal)` (1416) | Clears the whole page, draws `alarmButton(8, 8, 78, "Back")`, top-anchored title at `(240, 24)`, and usually profile/SD icons. | Calendar alone passes `false`. These header profile/SD icons have **no corresponding touch actions** in the reviewed detail branches. |
| `settingsRow()` / `settingsGroupRow()` (1426–1438) | Separate rounded 210 × 34 tiles or 448 × 38 rows. Hard-coded blue accents and fixed label/value budgets. | Older detail pages use a different layout vocabulary from the newer Settings category details. |
| `drawSettingsCard()` / `drawSettingsOption()` / `drawSettingsToggle()` (1440–1460) | Newer grouped cards, detail labels, disclosure strokes, and 46 × 28 switches. | Reusable ideas, but detail separators should align to detail text rather than the root's icon column. `drawSettingsOption()` reserves 118 px for detail independently of the title budget; long title and value could overlap if used without an explicit combined budget. |
| `alarmButton()` / `drawAppleButton()` (1174, 1572) | 30 px outlined controls versus variable-height pill buttons. The former uses `UI_BLUE` with `UI_TEXT`; the latter correctly uses `UI_ACTION` with `UI_ON_ACTION`. | Selected controls are not consistently styled or contrast-safe across themes. |
| `readTouch()` (190) | Pressure threshold 350, 220 ms sampling gate, rightward swipe detection after >90 px movement with <70 px vertical movement and <900 ms duration. Updates `lastActivity`. | Preserve calibration/threshold. Touch handling is embedded in `loop()`, often uses only a Y band, and can process a press before the gesture is classified as Back. It does not require release between accepted touches. |
| `refreshSdState()` (1093) | Every 3 seconds checks SD presence, creates required folders if possible, verifies readiness, then redraws selected pages on state change. | Redraw currently covers Settings, Wi-Fi, Weather, and Storage, but not every page with an SD header icon. Do not turn this into a periodic unconditional full-page redraw. |
| `saveSettings()`, `saveCreds()`, `saveAlarm()`, `saveCal()` | SD-only JSON persistence gated by `sdOk`. | No SD means session-only changes. These functions return `void` and may silently return on write failure; a drawn “saved” state is not proof of successful writing. |

### Page-by-page inventory

| Page and drawing entry points | Current entry/touch behavior | Implemented capability to preserve |
| --- | --- | --- |
| **Wi-Fi:** `showWifiPage()`, `drawWifiPageStatus()` (1640–1656); Settings `showSettingsPage()` case 1 | Home Wi-Fi opens `S_WIFI`. Back/swipe returns home. Choose Network (`y=102..139`) and Refresh Networks (`148..185`) both call `openWifiScanner(S_WIFI)`. Settings category Choose Network calls `openWifiScanner(S_SETTINGS)`. Connected status is redrawn every 180 ms while `S_WIFI` is open. | Connected SSID, connecting/disconnected state, animated signal, network scanning, saved-network count, up to five persisted credentials tried in order. Saved Networks is **informational**, not a working management/disclosure page. Normal Wi-Fi details do not expose an enable/offline switch. |
| **Wi-Fi scanner:** `openWifiScanner()`, `showScan()`, `scanNetworks()` (822–866) | Back honors `scanReturnScreen`; Rescan is `y>=278`; scanned rows are selected using `(y-42)/46`. Up to five nonempty, unique SSIDs are shown. | Rescan, scan errors/empty state, choosing a network, and the existing return destinations. Hidden/empty SSIDs are skipped; manual SSID entry is not implemented. |
| **Password / city editor:** `showKeyboard()`, `drawPasswordField()`, `showCityKeyboard()`, `addRow()`, `keys[]`, `keyLabel()` (869–957) | `S_KEYS` uses each key's actual bounds. Includes character/space entry, case shift, symbol layer, backspace, Show/Hide, Enter, and cancel. Typing/backspace redraws the field; shift/layer/Show redraw the whole keyboard. | All keyboard layers, 63-character input limit, password masking, city entry, errors, and cancellation. Password success calls `saveCreds()`, time/weather helpers, and then the existing Classroom/sync setup sequence—even when entered from normal Wi-Fi settings. Do not silently change that flow in a visual refactor. |
| **Weather:** `showWeatherPage()` (1658–1665) | Home weather opens the page. Back/swipe returns home. Automatic row selects `manualWeather=false`, saves, and fetches; Manual US City selects `true`, saves, then opens the city keyboard; Refresh calls `fetchWeather()`. | Mode status, default/automatic choice, manual city and coordinates, geocoding, refresh, and existing offline/error paths. `fetchWeather()` uses **Philadelphia** for the nonmanual path; it does not perform Wi-Fi/IP location detection. |
| **Calendar:** `showCalendarPage()` (1667–1687), `daysInMonth()`, `loadClassroomCache()` | Home Calendar opens the current month. Back/swipe is the only page interaction. | Current-month grid, current-day emphasis, and up to three displayed cached assignment titles/due dates. Up to eight assignment records are loaded. No month navigation, day selection, event editor, or live OAuth sync is implemented here. Settings case 3 separately controls `calendarEnabled`, `classroomEnabled`, and `notifications`. |
| **Alarms list/editor:** `showAlarmEditor()` → `drawAlarmList()`; `beginAlarmEdit()`, `drawAlarmEditor()`, `alarmTimeText()`, `alarmRepeatText()`, `adjustAlarmDate()` (714, 1138, 1180–1232) | Home clock area opens the list. Four maximum (`MAX_ALARMS=4`). List rows edit except right-side switch band (`x>=388`), which toggles and saves. Add opens a draft; Done/Back returns home. Editor Back/swipe/Cancel discards the draft and returns to the list. Save replaces/appends and persists. | Four independent alarms, enable switches, hour/minute adjustment with wraparound, day/month/year controls with calendar clamping, seven repeat bits, one-time dates, draft enable state, 12/24-hour display, Save and Cancel. There is **no implemented Delete or Snooze** to retain or advertise. |
| **Ringing alarm:** `alarmDueNow()`, `showAlarmRinging()`, `dismissAlarm()` (729, 1266–1286) | Evaluated in `S_HOME`; any accepted touch dismisses while ringing. Green LED is asserted; one-time alarm is disabled and saved after dismissal. | Visual/LED indication, any-touch dismissal, and one-time versus recurring behavior. No audible alarm implementation was observed. Do not move alarm execution into detail pages or the OTA operation as part of this visual work. |
| **Appearance:** Settings `showSettingsPage()` case 2 and `S_SETTINGS` theme handler; legacy `showAppearancePage()` / `S_APPEARANCE` (1380–1390, 1515–1528, 1936–1939, 1977–1987) | The normal Settings category has five full-width 46 px rows. Selection applies the palette immediately and saves. The older page has five two-column 210 × 34 tiles. Its handler exists, but no normal launcher to `showAppearancePage()` was found; its visible call is the selection redraw. | Exact theme IDs `crystal`, `midnight`, `ocean`, `sunrise`, `graphite`; immediate preview, persisted selection, and the legacy entry point if reused. `applyAppearanceTheme()` is hard-coded; no on-device theme-package loader was observed. |
| **Storage:** `showSdCardPage()`, `prepareSdCard()`, `ensureClockOsSdLayout()`, `clockOsSdReady()` (1036–1075, 1314–1346); Settings case 4 | Home SD icon or Settings Prepare Storage opens `S_SDCARD`. Prepare and Browse use Y-only bands; Back returns to Settings with `settingsAtRoot=false` without setting `settingsCategory`. | Distinct absent/present-not-ready/ready states, card capacity and used MB, offline nondestructive folder preparation, current preference writes, feedback, and browsing. Preserve `CLOCKOS_SD_DIRECTORIES` and exact parity with installer layout. This is not formatting, repartitioning, or erasing a card. |
| **Files:** `showSdFilesPage()` (1348–1368) | Settings Browse Files or Storage Browse opens `S_SDFILES`; Back/swipe goes to Storage. | Read-only listing of up to nine **root entries**, directories marked `[DIR]`, files with byte sizes, and no-card/empty states. There is no file opening, recursive navigation, scrolling, or deletion handler. |
| **Display-related preferences:** Settings case 0, `drawClockEditor()`, `showClockEditor()`, `adjustClockDate()`, `applyClockDraft()` (1234–1264); `sleepDisplay()`, `wakeDisplay()`, `setDisplayBacklight()` (1611–1638) | General exposes 24-hour time, Battery Percentage, and an Appearance route. Offline startup opens the manual date/time editor. Sleep runs only while home is idle for 60,000 ms; first touch wakes to home. | Existing switches, manual clock Save/Cancel and date clamping, fixed sleep/wake behavior, GPIO27 handling. `screenBeforeSleep` is recorded, but wake currently restores home. **No dedicated Display page, brightness slider, configurable timeout, or 180° orientation toggle exists.** |
| **Software Update:** `drawUpdatePage()`, `drawAppleUpdateIcon()`, `updateProgress()`, `runUpdate()` (1563–1595, 1698–1717); `BL::checkAndInstall()` / `BL::installCandidate()` | Automatic check after connected startup and hourly from home via `UPDATE_CHECK_MS`; synchronous status transaction with reboot only on success. `loop()` has no `S_UPDATE` touch branch. Developer tools also call `drawUpdatePage()` for informational messages. | Release identity, checking/installing/up-to-date/failure/restart states, progress indicator, semantic candidate selection, inactive-slot write, abort on failure, and no success reboot before completion. There is no Settings Check for Updates action or cancellation mechanism. |
| **Factory Reset:** `showFactoryResetPage()`, `factoryResetClock()`, `factoryResetArmed` (1553–1561, 1597–1609, 2076–2082) | Settings System opens the page. Any X in `y=246..289` arms; another accepted touch in that band resets. Back/swipe clears arming and returns to Settings. | Two-stage destructive confirmation and ClockOS-data-only reset followed by restart. Firmware and unrelated SD files are not erased. Actual deletion list and readiness gate are detailed below. |

## 2. Highest-impact findings and change priorities

**Observations** are source facts. **Recommendations** below are proposed future work, not completed changes.

| Priority | Observation | Recommended source change and visible result |
| --- | --- | --- |
| **P0 — detail-page corruption** | In `loop()` General toggles call `showSettingsPage(); drawStatusBlock();` (1930). Calendar toggles call `showSettingsPage(); drawRightPanel();` (1946). Both latter functions draw black **home** regions, not Settings content. `drawRightPanel()` overwrites the entire right side; `drawStatusBlock()` overwrites the lower left. | Stop invoking home drawing while `screen==S_SETTINGS`. Refresh the changed detail row/switch, or initially redraw only the Settings detail. Let `showHome()` draw home when navigated there. Keep the setting mutations and `saveSettings()` calls. This is the clearest first fix: it removes deterministic wrong-screen paint rather than merely polishing styling. |
| **P1 — inconsistent shell and clipping** | Detail pages mix 34/38 px pills, 30 px Back/stepper controls, category cards, and hard-coded captions. Wi-Fi's long one-line footer and several Weather/Storage/Reset instructions are not measured or wrapped. Header title uses top anchoring rather than the button's vertical center. | Add a common bounded detail header, inset grouped card/row renderer, width-aware wrapped note, and shared rectangle hit testing. Reuse semantic colors. Apply first to Wi-Fi, Weather, Storage, and Reset, then category details. |
| **P1 — Wi-Fi status overlap** | `drawWifiPageStatus()` right-aligns a 160 px SSID at x=447, so it can occupy x=287..447, then `drawWifiSignal(364,74,...)` clears/draws x=344..383 inside that text region. The whole row is repainted every 180 ms even when connected. | Give the signal a separate reserved column. Repaint static SSID/state only on change; repaint only the glyph region during connecting. This produces an immediately cleaner and steadier status row without losing animation. |
| **P1 — reset hit target and explicit taps** | The drawn Reset button is x=76..403, but the handler checks only Y. The 220 ms touch gate can accept a held press again without release, so the code does not itself guarantee two distinct physical taps. Armed state uses the normal `UI_ACTION` primary treatment instead of a destructive treatment. | Bound arming/reset to the actual button rectangle and require release before accepting the confirming tap. Keep Back/swipe disarming. Use a theme-safe destructive button/confirmation treatment. Do not reduce the two-stage safeguard. |
| **P1 — alarm density and date inconsistency** | Editor has many outlined 30 px controls and single-letter repeat days; each adjustment clears/redraws the whole page. `alarmRepeatText()` and `drawAlarmEditor()` show month/day/year, while manual clock and README use day/month/year. | Keep every control, organize Time / Date / Repeat / Actions, increase actual targets toward 44 px, use clearer day labels, and repaint changed values/selection only. Format alarm **display** dates as DD/MM/YYYY without changing stored fields or scheduling semantics. |
| **P2 — truthful row semantics** | Saved Networks, Mode, Capacity, readiness, and version are informational. Weather says “Use Wi-Fi location,” although its automatic path is fixed Philadelphia. Capacity/instructions are squeezed into 160 px values. Appearance's legacy footer cites `/themes/appearance`, not `THEMES_DIR`. | Separate informational, toggle, choice, disclosure, and action rows. Use honest labels and move essential explanations to bounded secondary text. Correct the displayed theme path to `/.source/themes/appearance`; do not imply a loader that is not implemented. |
| **P2 — calendar correctness and legibility** | `showCalendarPage()` hard-codes `first=0`, placing every month's first day under Sunday. Weekday labels are one spaced string; title and due date are fitted as a single concatenation. | Compute the first weekday with the existing time/date machinery; draw each weekday at its column center; accommodate six weeks; reserve due-date width independently so a long title cannot hide it. Retain current-month/read-only behavior. |
| **P2 — redundant theme presentation** | Normal full-width theme rows and legacy two-column tiles coexist. The selected marker is Unicode `✓` drawn through a `7b` FreeSans font; other pages use `‹` and `…`. Font support should not be assumed. | Use one full-width five-choice renderer for both entry points, retaining all IDs and handlers. Draw checkmarks/chevrons with TFT primitives, as existing disclosure strokes already do. Use ASCII ellipsis or verified font support for text. |
| **P2 — update visual continuity** | `drawUpdatePage()` hard-codes a dark palette and clears the whole screen on progress changes; status truncates by 52 characters, not pixels. `BL::installCandidate()` calls progress **after** `Update.writeStream()`, not during streaming. Developer informational `S_UPDATE` pages have no exit handler. | Theme the transaction surface, fit/wrap status, and separate static drawing from status/progress updates. Do not fake intermediate percentages. Allow Back only for explicitly nonbusy informational states if implementing an exit; do not introduce unsafe cancellation during a write. |

## 3. A consistent pattern that actually fits 480 × 320

### Layout contract — proposed, not existing constants

Use a small shared geometry contract, with names such as `DETAIL_MARGIN`, `DETAIL_HEADER_H`, `DETAIL_ROW_H`, and `DETAIL_CONTENT_RECT`. These names would be **new**, unlike `TFT_W` and `TFT_H` which already exist.

| Region | Proposed bounds/usage |
| --- | --- |
| Header | y=0..47. Back touch rectangle `(8,2,92,44)`; quiet button or chevron-plus-label. Center title with `MC_DATUM` at `(240,24)`, not `TC_DATUM`. Reserve a bounded title slot; no fake tappable accessories. |
| Main inset surface | x=16, width=448; normally starts y=56. Radius about 12. One card per semantic group rather than one floating pill per row. |
| Standard row | 44 px high, vertically centered label/value; separators inset to x=32, not the root icon column at x=64. Five rows occupy y=56..275, leaving y=284..307 for one short note. |
| Four-row page | Four 44 px rows occupy y=56..231. A note region `(24,244,432,60)` can hold two short wrapped lines comfortably. Do not also place an action footer into that same region. |
| Page with action footer | Reserve y=264..307 for 44 px actions. Keep body content above y=252. Explanation, status, and controls must be laid out against this reservation rather than allowed to overlap it. |
| Text budgets | Use the actual font and pixel width. For a standard single-line label/value row, an example budget is 240 px label + 24 px gap + 152 px value within the 416 px text area. A disclosure/switch needs its own reserved column. Never fit title and value independently to budgets that can overlap. |
| Body type | `F12` / `F12B`; `F18B` for page titles or time emphasis. Use `F24B` sparingly when the available measured height permits it. Captions use `UI_MUTED` rather than literal gray. |

This is a **layout family**, not a requirement that every page become five identical rows. Calendar needs a grid, Alarms needs a compact editor, and Software Update needs a transaction surface. They should share the same header, colors, margins, typography, and status treatment while preserving their specialized content.

### Row/control semantics

1. **Information:** no chevron, no press state, no touch action. Examples: saved-network count, Capacity, current mode, reset readiness. Version retains its existing hidden multi-tap handler even if it looks informational.
2. **Toggle:** label plus `drawSettingsToggle()`, with a full bounded row target for ordinary setting toggles. Alarm-list switches need a **separate bounded switch target**, because the rest of the row edits.
3. **Choice:** names and a vector checkmark for the selected option. Use `UI_SELECTION` plus `UI_TEXT`, or `UI_ACTION` plus `UI_ON_ACTION`, not arbitrary `UI_BLUE` plus `UI_TEXT`.
4. **Disclosure:** only for a real next page, such as Choose Network, Manual City, or Browse Files.
5. **Action:** Refresh, Prepare, Save, Cancel. Use restrained accent text or a bounded footer button, not an unexplained chevron.
6. **Destructive action:** label and confirmation style explicitly red/destructive with a foreground that remains legible in all five themes. A separate destructive foreground token may be warranted; do not assume `UI_TEXT` contrasts against `UI_RED`.

### Header and navigation

`drawBackHeader()` should be the first shared seam, but its `showGlobal` contract must remain available for any caller that still needs it. For the reviewed details, **do not display profile/SD glyphs as if they were buttons when their handlers do nothing**. Removing those redundant decorations is not removal of an implemented touch capability. Keep SD readiness on Storage and persistence feedback where meaningful. Calendar's current no-global-icons behavior must remain.

Define draw and hit rectangles together, using half-open bounds: `x>=left && x<right && y>=top && y<bottom`. Do not leave a Y-only action band extending through gutters. Preserve Back/swipe navigation and draft cancellation; consume the detected swipe before page actions. A deeper release-based gesture redesign should be staged separately because `readTouch()` also serves setup, keyboard, and home.

Track the actual return context for reusable details rather than relying on stale `settingsCategory`. **Observed example:** opening Storage from home currently returns to whatever Settings category was last selected, since the handler sets only `settingsAtRoot=false`. Recommended behavior is an explicit home-or-Settings origin, retaining both routes; this is a navigation correction, not a reason to remove a launcher. For a conservative first visual pass, keep current destinations and introduce context tracking in a separately checked step.

### Rendering strategy

Split static page entry drawing from dynamic region drawing. Initial entry, theme change, and a genuinely changed card-presence state may redraw the whole page. Typing, toggling, steppers, connecting animation, and progress should not.

- Keep `showHome()` and home drawers confined to home rendering.
- `drawWifiPageStatus()` should maintain a small state snapshot: connection status, SSID, signal level, animation frame, and palette generation. Clear text and glyph subregions using their actual surface color.
- `drawAlarmEditor()` can retain full drawing as a fallback, with separate Time, Date, Repeat, and Enabled drawing helpers for interactions.
- Separate update-page static surface drawing from status/progress; reset any cached progress state at each `runUpdate()` entry (`updateProgress()` currently retains static `last` across runs).
- Avoid requiring a full-screen sprite. The existing sprite is only 100 × 100; this review does not assume enough free memory for an additional 480 × 320 framebuffer. Small region sprites are optional, not a prerequisite.

## 4. Page-specific implementation guidance

### Wi-Fi and dependent input screens

**Observed:** Wi-Fi has two presentations: Settings case 1 and `S_WIFI`. The former does not animate a signal; the latter does. Both retain scanner access. Long footer text is drawn in one unbounded line. Scanner truncates SSIDs by 26 characters, not pixels, and its error/empty-state text is unwrapped. Its row handler accepts row gaps and ignores X.

**Recommend:** share the same network-status/row primitives rather than immediately merging screen states. A workable `S_WIFI` layout is a 64 px status row at y=56, then three 44 px rows, ending y=252. Put Network/state and SSID on separate lines; reserve x=408..451 for the signal glyph, with text ending by x=392. Use two short note lines below the card. Keep Choose and Refresh distinct labels even though both currently launch a new scan, and leave Saved Networks informational.

Keep all five scanned choices. Five 44 px rows at y=48 occupy 220 px; a 44 px Rescan footer at y=272 ends at 316. Header, rows, gaps, and footer must share exact drawing/touch rectangles. Pixel-fit the SSID and wrap scan errors. Do not add or advertise hidden-SSID entry as existing behavior.

Retain every `Key::t` action and the current Enter flow. The keyboard already fits five rows of 36 px caps through y=315; do **not** inflate all keys to 44 px and push Enter off-screen. Treat dense input as a justified compact exception. Show/Hide should update only the field and key label rather than rebuilding the keyboard when the layer is unchanged. Keep password contents masked by default and out of logs/review artifacts. City cancel must still return to Weather; password cancel must still return to the scanner.

### Weather

**Observed:** the page manages location and refresh, not a forecast dashboard. `fetchWeather()` preserves the last successful `weather` state on many failures. `weather.valid` is therefore not evidence of a current connection. The Refresh value currently selects “Update now” versus “Connect Wi-Fi” using `weather.valid`, not connectivity. Selecting Manual City sets and saves manual mode before editing, so canceling input does not revert that setting. Preserve that current mutation order in a purely visual change.

**Recommend:** show Mode as information; show the nonmanual choice as **“Default location — Philadelphia”** or explain that Automatic currently means the default Philadelphia coordinates. Do not claim ISP/Wi-Fi geolocation. Keep Manual US City as a disclosure and Refresh as an action. Make refresh feedback depend on the attempt/connection, not just `weather.valid`, while retaining the last valid data. If geocoding fails, preserve existing input and show a bounded error—do not clear saved coordinates or add indoor sensors.

Avoid repeating two long footer sentences. A short note can explain default location and the Wi-Fi requirement for looking up a new city. Preserve the existing manual-city/geocoding fallback path; do not describe an offline keyboard submission as a verified new geocode.

### Calendar

**Observed:** the grid currently places day 1 in Sunday for all months. Month title is numeric `MM/YYYY`. The three cached-item lines can hide due dates behind a long title. The page has no active date cells. Classroom authorization and fresh sync are performed by the installer, not the calendar page.

**Recommend:** compute the weekday for year/month/day=1 using `struct tm` / `mktime()` in the existing timezone; keep `daysInMonth()` for leap years. Draw seven weekday headings independently at the same centers as dates. A six-week grid can fit centers at y=110,128,146,164,182,200; use a selected-day fill with semantic text contrast rather than only hard-coded blue. Keep the current month and no-calendar-data fallback.

Retain three agenda lines below the grid, with separately fitted title and due-date columns. They are informational and can be denser than 44 px because they are not touch targets. State “Cached Classroom items” and distinguish calendar-disabled/no-cache cases. Do not imply events, courses, or more than the loaded/displayed records are interactive. Leave month browsing/live sync as separate feature work.

### Alarms and manual date/time

**Observed:** the list supports four alarms, but no explicit empty-state text. The switch style differs from `drawSettingsToggle()`. Several editor touch bands are wider/taller than their drawn buttons, and repeat indexing `(x-68)/58` can map left gutter positions near the first button to Sunday because negative integer division truncates toward zero. The list switches also accept touches outside the card to its right.

**Recommend:** use bounded card/edit and switch rectangles, with a short empty-state message that does not obscure Add. Keep `MAX_ALARMS`, draft state, repeat masks, existing JSON migration, and Save/Cancel unchanged. Use semantic selection colors and clearer weekday abbreviations instead of ambiguous repeated “T” and “S”.

A **single-screen editor remains feasible without deleting controls**. One candidate packing, to be checked against font metrics:

- Header y=0..47.
- Compact formatted time/mode summary centered near y=68, using `F18B` rather than oversized emphasis. Four time stepper targets at y=92, h=44; x=16,120,264,368 with w=96.
- DD/MM/YYYY summary centered near y=152. Six date steppers at y=170, h=44; six 72 px controls with 3 px gaps occupy 447 px from x=16.
- Repeat label at x=24, with seven 50 × 44 day targets starting x=80 at 54 px pitch, y=222. The last ends at x=454.
- Enabled / Save / Cancel at y=272, h=44, retaining the existing three action columns within x=12..467.

Use measured text bounds before accepting this packing. It preserves all hour, minute, date, repeat, enable, save, and cancel capabilities; no wheel picker, scrolling state, deletion, or new snooze behavior is required. Update both alarm date renderers to DD/MM/YYYY; keep the stored year/month/day fields unchanged. Preserve 12/24-hour rendering through `alarmTimeText()` and do not remove AM/PM.

The manual clock editor can reuse the time/date groups but must retain its offline-after-restart explanation and `applyClockDraft()` semantics. Its existing Cancel returns home without applying the draft. Ringing remains its own interruption surface; maintain current **any-touch dismissal**, not an accidental restriction to the drawn Dismiss button.

### Appearance

**Observed:** the category renderer is already closest to the proposed pattern. Its five 46 px rows fit. The older two-column renderer is visually weaker and normally unlaunched. Theme IDs occur in several local arrays; the source does not read the installed JSON packages when applying a theme.

**Recommend:** consolidate names/IDs into one shared constant catalog and render five full-width choice rows in either entry point. Preserve the public drawing function and handlers until call paths are intentionally reconciled; do not delete a supported palette. Use a vector checkmark and avoid chevrons for immediate selections. Selection should still apply immediately, redraw with the new palette, and call `saveSettings()`.

Correct the legacy path caption using `THEMES_DIR`, but do not promise that copying arbitrary theme packages makes them selectable in this firmware. The five implemented palettes are the compatibility baseline. Test labels, action contrast, muted text, switches, and selected repeat days against **all five** palettes during subsequent implementation.

### Storage and read-only files

**Observed:** status is based on directory verification, not just successful `SD.begin()`. `prepareSdCard()` creates folders and calls the four preference writers without verifying their return values. Its success message says settings are saved, though those writers can silently fail. Both startup and `refreshSdState()` also create missing folders automatically. “Needs setup” is therefore an actual failure/incomplete-layout state, not a compulsory wizard for every fresh formatted card.

**Recommend:** keep Ready / Needs setup / No card distinct. Shorten the no-card Capacity value and put insertion guidance in a note. Show used/total MB either as an explicitly budgeted compact value or a two-line row; do not ellipsize the essential numeric total. Label Prepare as nondestructive and offline-capable. Keep `CLOCKOS_SD_DIRECTORIES`, `DATA_DIR`, `PREFERENCES_DIR`, `ICON_DIR`, `THEMES_DIR`, and compiled/uncompiled paths unchanged.

Bound Prepare and Browse to their rows. Preserve all current feedback and do not disable preparation merely because Wi-Fi is absent. Prefer “ClockOS folders ready” until successful preference writes are verified; a future persistence-status return value is separate robustness work, not something a renderer can prove.

The file viewer must remain read-only and display up to nine root entries. Dense 22 px metadata lines are acceptable here because there are no per-file actions. Use consistent filename/size alignment and a bounded no-card/empty state. Do not put disclosure chevrons on directories unless directory navigation is separately implemented. Do not expose protected tokens or add file deletion in this UI pass.

### Display-related detail

**Observed:** “Display” is a requested review category, but no dedicated page exists. General controls are the implemented display-related preferences; Appearance is separate. Sleep is hard-coded to 60 seconds **on home only**, and wake restores home.

**Recommend:** apply the same grouped row and toggle treatment to General and the same editor treatment to manual clock entry. Treat a dedicated Display page as optional information architecture, not a prerequisite for polish. If introduced later, move/alias existing controls without removing the General routes, and clearly distinguish informational fixed behavior from configurable controls. Do not show a brightness slider, sleep-timeout selector, or rotation toggle without an implementation behind it. Preserve `TFT_ROTATION`, all `TOUCH_CAL_*` values, GPIO27 sequencing, and the current wake destination.

### Software Update

**Observed:** this is blocking automatic update execution, not an interactive update browser. `runUpdate()` itself does not restore the previous page after failure/up-to-date; its existing callers restore home. Progress is reported only after the stream write; an apparently stalled percentage is not evidence of a live progress callback. No signed-recovery installation was observed; Developer Revert merely draws an informational message.

**Recommend:** retain the rounded transaction card but derive its palette from `UI_BACK`, `UI_KEY`, `UI_TEXT`, `UI_MUTED`, and `UI_ACTION`; keep the release identity from `CLOCKOS_NAME` / `CLOCKOS_VERSION`. Wrap status by pixels rather than slicing 52 characters. Draw checking, transfer/installing, completion, up-to-date, and failure as explicit presentation states. Do not add animated fake percentages.

If real progress later requires replacing `Update.writeStream()` with chunked writes, that is **OTA logic work** requiring separate compile/error-path verification, not the first visual change. Preserve `BL::versionNumber()`, `BL::newestCandidate()`, `BL::checkAndInstall()`, inactive-slot handling, `Update.abort()`, and success-only reboot. Never add Back/cancellation that can abandon an active flash write. A separate nonbusy state could safely return from the current Developer informational dead-end without pretending that recovery installation exists.

### Factory Reset

**Observed exact scope:** when `sdOk` is true, `factoryResetClock()` attempts to remove `SETTINGS_FILE`, `ALARM_FILE`, `WIFI_FILE`, `TOUCH_FILE`, `CLASSROOM_CACHE_FILE`, `CLASSROOM_TOKEN_FILE`, and legacy `/.source/data/secrets/classroom_token.json` / `classroom_client.json`, then attempts `SD.rmdir(PREFERENCES_DIR)`. It does **not** remove `CLASSROOM_SECRET_FILE`, all of `/data/secrets`, the firmware, unrelated files, or `HISTORY_FILE`. Individual deletion return values are not checked. When `sdOk` is false, deletion is skipped, but the device still restarts. Preserve this scope; do not widen it under a styling change.

**Recommend:** use a compact title, two or three wrapped explanation lines, readiness information, and a 44 px destructive footer action. On the first accepted, bounded tap, visibly change the warning and button to Confirm Reset; require release and another bounded tap. Back/swipe must disarm. A stale armed state must not survive leaving/re-entering the page. Keep two explicit stages and do not offer a single-tap shortcut.

Text should say the **named ClockOS preferences, saved networks, alarms, and Classroom token/cache** are targeted. Do not claim all Google credentials/OAuth client secrets or every preference-directory file will be erased. Distinguish no ready SD data from a present-but-unready card; do not claim successful deletion without checking it. Preserve the distinction from Windows Setup's separately confirmed whole-card erase. This review recommends safer presentation and hit handling, not expansion of the destructive operation.

## 5. Implementation boundaries and later verification

- Preserve `AppSettings` fields, default values, SD-only persistence, five-network storage, four-alarm limit, legacy JSON migrations, all five theme IDs, weather source/fallbacks, Classroom cache limits, and existing setting mutations.
- Do not change firmware identity/version paths, OTA selection logic, manifests, compiled artifacts, installer behavior, calibration, or hardware pins as a side effect of page polish. A later release must follow the repository's release policy; this report does not establish a new release or build.
- Keep root Settings layout and home design out of scope except preventing their drawing functions from painting details.
- Validate future changes with a matching firmware build and source/draw-rectangle checks. Useful coverage includes long SSIDs/cities/captions, no-card/incomplete/ready card states, no cache/disabled Calendar, five themes, all four alarms, zero alarms, AM/PM boundaries, leap dates, and last-row/footer visibility.
- Verify touch gutters, switch-versus-edit separation, editor Save/Cancel, scanner Back destinations, swipe-versus-action behavior, and distinct release-separated reset taps. Verify the updater does not gain an unsafe navigation path or fictitious progress.
- Physical 480 × 320 display legibility, exact font rendering, touch corners/center, color order, backlight behavior, SD hot-plug, alarms, and OTA remain **future target-device checks**, not results of this review.

## 6. Ordered implementation checklist

1. [ ] **Fix wrong-screen redraws first:** in `loop()` General and Calendar setting handlers, retain mutations/persistence but stop `drawStatusBlock()` / `drawRightPanel()` from painting while `screen==S_SETTINGS`.
2. [ ] Establish shared detail geometry, bounded header/title, semantic row types, wrapped notes, and draw/hit rectangles. Reuse existing font/palette helpers; do not redesign root Settings or change calibration.
3. [ ] Repair `drawWifiPageStatus()` text/signal overlap and change its 180 ms loop to glyph-only animation plus change-driven status redraw. Preserve connecting animation and saved-network count.
4. [ ] Apply the shared surface to Wi-Fi, Weather, and Storage; shorten/wrap their notes, distinguish information from actions/disclosures, and correct weather/default-location wording and capacity presentation.
5. [ ] Harden Factory Reset presentation and hit handling: actual button bounds, release-separated two taps, clear destructive armed state, Back/swipe disarming, truthful scope/readiness, and no expanded deletion list.
6. [ ] Rework `drawAlarmList()` / `drawAlarmEditor()` into consistent grouped controls with bounded switch/edit/stepper/day targets, DD/MM/YYYY displays, all four alarms, all seven repeat bits, AM/PM, draft Enabled, Save, and Cancel retained.
7. [ ] Replace full alarm/input redraws where possible with region updates; preserve all keyboard layers, 63-character limit, Show/Hide, input errors, and existing setup/return flow. Reuse time/date groups in `drawClockEditor()` without changing offline clock semantics.
8. [ ] Share the five-theme catalog/renderer between Settings Appearance and the legacy `showAppearancePage()` entry point; use vector selection markers, semantic contrast, immediate apply/save, and correct `THEMES_DIR` text.
9. [ ] Correct Calendar's first weekday, align weekday/date columns, fit a six-week grid, and separate cached assignment titles from due dates. Preserve current-month/read-only behavior and all three Calendar toggles.
10. [ ] Polish Storage file/status drawing and feedback without adding destructive or recursive file actions; preserve nine root entries, offline folder preparation, and exact directory-layout compatibility.
11. [ ] Apply the shared presentation to existing General/display-related preferences only; do not invent brightness, timeout, orientation, or wake-restoration capabilities.
12. [ ] Theme and split static/dynamic Software Update drawing; fit status by pixels, reset progress presentation per run, preserve safe OTA behavior, and handle only explicitly nonbusy informational exits if added.
13. [ ] Add explicit origin tracking for reusable details/scanner navigation in a separately checked step, retaining every existing launcher and draft-cancel route; prevent swipes from also activating detail actions.
14. [ ] Build future source changes with the matching toolchain, check every layout/state/rectangle and all five themes, then perform and record actual target-device display/touch/SD/backlight/OTA checks. Do not mark hardware validation complete from static review alone.
