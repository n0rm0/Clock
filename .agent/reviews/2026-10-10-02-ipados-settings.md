# Settings: full-width grouped-list specification for ClockOS

**Use one full-width category list, two visual groups, and a single-column detail page for each category. Keep all six existing categories visible, use 44 px root rows and 48 px detail rows, and share one set of row definitions between drawing and hit testing.** The current v2.8 approach is close enough to refine rather than replace. The immediate blockers are screen-inappropriate redraws after toggles, missing navigation provenance, and inconsistent row/accessory semantics—not the absence of a sidebar.

Review date: **2026-10-10**. Target: **480 × 320 landscape touch display**. This is an actionable design and static source review, not an implemented change, firmware build, visual capture of the running device, or physical hardware validation. Only this review document was added to the repository; no application source, configuration, manifests, TODOs, binaries, tests, or Git state were modified.

## Evidence and scope

Read the supplied [TODO](../todo.md), [conventions](../agent.md), [README](../../README.md), [ClockOSv2.8 sketch](../../.source/uncompiled/updates/ClockOSv2.8/ClockOSv2.8.ino), and [configuration](../../.source/uncompiled/updates/ClockOSv2.8/config.h). Firmware line references below refer to that supplied sketch, not an older version. No installer implementation, release manifest, binary, or test file was needed for this review.

The TODO's latest follow-up records the full-width Settings work as implemented and explicitly leaves physical layout/touch checks open (L298–304). The README identifies v2.8 as the current compiled release candidate and also leaves orientation/calibration checks open (L7–10, L64–68). Some earlier guide/TODO prose describes older baselines or validation claims. Those historical statements do not establish validation of the proposed design. The inspected source is the evidence for behavior described here.

The scope is Settings root, category/detail organization, shared list/chrome primitives, and the minimum destination/back-navigation changes needed to make that hierarchy coherent. Home, weather, calendar grid, alarm editor, setup wizard design, installer, updater, developer keypad, and storage implementation are **not** redesign targets. Their entry points and existing behavior must continue working.

Apple's guidance supports using list hierarchy, grouped surfaces, disclosure accessories for navigation, and a checkmark for an option selection [1]. Its button guidance recommends a 44 × 44 **point** hit region and a visible press state [2]. Its current accessibility table separately lists 44 × 44 pt as the iOS/iPadOS default control size and 28 × 28 pt as the minimum [3]. The pixel sizes in this report are explicit **ClockOS design choices**. They are not a point-to-pixel conversion, a measured physical target size, or a claim of Apple-platform accessibility compliance.

## What exists and what needs correction

### The existing full-width list is the right starting point

**Observation.** `settingsCategory` stores six numeric categories and `settingsAtRoot` distinguishes root/detail (L122–123). `showSettingsPage()` renders a 456 px-wide grouped card at `(12,52)` with six 42 px rows (L1477–1498). It already supplies category icons, trailing summaries and chevrons. General and Calendar use switches; Appearance uses five selectable themes. This is a full-width list, not a split view.

**Recommendation.** Keep that overall model and the six existing category identities. Increase root rows from 42 to 44 px, split the root into two cards, and stop deriving category identity from visual row index. There is no need to add scrolling, pagination, a sidebar, search, an account card, or a seventh category to solve this screen.

### Two toggle handlers paint Home over Settings

**Observation.** In `loop()`'s `S_SETTINGS` branch, a General toggle calls `saveSettings(); showSettingsPage(); drawStatusBlock();` (L1925–1930). `drawStatusBlock()` paints the Home-only rectangle `(0,226,240,94)` and its Wi-Fi/SD/battery/Settings controls (L1077–1091). Calendar toggles similarly call `drawRightPanel()` after `showSettingsPage()` (L1941–1946). That function clears and paints the entire right half `(241,0,239,320)` for Home weather/calendar content (L1114–1130). The Calendar call can overwrite the detail header, values, switches and row surfaces.

**Recommendation — first implementation priority.** Remove the Home renderer calls from these Settings handlers. Apply and save the preference exactly as now, then redraw the affected Settings row. `showHome()` already calls both Home renderers when Home is actually entered (L1689–1695). Do not change what the preferences mean or defer their persistence to a new Save/Done action.

### Accessory semantics and geometry do not agree

**Observation.** `drawSettingsOption()` places label/value centers at `y+17`, but its switch and chevron centers are at `y+23` (L1452–1459). The title can occupy 340 px from x26 while the detail occupies 118 px ending at x418: those reservations overlap. `drawSettingsCard()` always starts separators at x64, even when detail labels start at x26 (L1446–1449). Unselected Appearance rows have chevrons even though tapping selects a theme in place (L1515–1527). The selected row uses a UTF-8 checkmark with `F12B`/`FreeSansBold9pt7b`; no Unicode glyph mapping is shown. The detail Back label similarly includes `‹` (L1500).

**Recommendation.** Introduce explicit row types and non-overlapping content slots. Navigation gets a neutral disclosure glyph, switches get no chevron, selectable themes get a vector checkmark only when selected, and information rows get no accessory. Draw Back and checkmark symbols as vectors rather than depending on unsupported glyphs. Center every element on the row's actual midpoint.

### Several routes lose their parent

**Observation.** The General > Appearance shortcut overwrites `settingsCategory` with 2; Back subsequently returns to root, not General (L1928, L1915). Settings-root swipe-back redraws root instead of exiting like the Back button (L1913–1915). A Home Settings tap calls `showSettingsPage()` without explicitly selecting root (L1863). Storage can be entered from Home without setting a category, but its Back forces `settingsAtRoot=false` and renders whichever category was previously stored (L1862, L1963–1964). Files always return to the Storage page, even when opened directly from Settings' Storage category (L1950, L1969–1970).

`S_WIFI` returns to Home (L2017–2018), while Settings' separate Wi-Fi detail returns to Settings via `scanReturnScreen`. The duplicate `showAppearancePage()` presents a two-column grid unlike the Settings theme list (L1380–1389). `showProfilePage()` and `showAppearancePage()` have no incoming call from the inspected root/category code; profile icons in `drawBackHeader()` are not backed by header hit handlers. Do not confuse a defined function or decorative glyph with an exposed Settings feature.

**Recommendation.** Record the caller when opening a child/shared page, and make Back and swipe-back use the same parent operation. Share bodies for Wi-Fi, Storage and Appearance rather than adding more parallel layouts. Keep Home shortcuts, but make their shared pages return to Home when entered from Home. Do not invent on-device Google sign-in, profile navigation, theme downloads, recovery capability, or a Wi-Fi/offline switch.

### Sampling does not guarantee one activation per touch

**Observation.** `readTouch()` accepts another contact sample after 220 ms while the panel remains pressed (L190–207). `loop()` can therefore process repeated toggles or actions from a single held contact. It also tracks a rightward swipe after actions may already have fired. Row-coordinate division is spread among different branches; several child-page actions constrain y but not x.

**Recommendation.** Use a Settings interaction controller with a press/release latch. A single contact must cause at most one row activation, including across a page transition. A drag must cancel the row activation before a back gesture is dispatched. Retain the current calibration and pressure threshold; do not globally rewrite keyboard repeat, alarm handling or other touch flows as part of this task.

## Root information architecture

Keep the stored IDs unchanged: **0 General, 1 Wi-Fi, 2 Appearance, 3 Calendar, 4 Storage, 5 System**. Change only their visual order. The first group is **Display & content**: General, Appearance, Calendar. The second is **Device & maintenance**: Wi-Fi, Storage, System. General retains its Appearance shortcut, so no existing setting disappears.

The root has two grouped surfaces, not six separate pill buttons. Root section captions are intentionally omitted: the short display cannot fit six 44 px rows, a 44 px header, two caption bands and useful gaps. The two surfaces provide grouping; detail pages supply explicit section captions. This is a deliberate small-display adaptation of the grouped-list pattern, not an attempt to reproduce a full-sized iPad screen at reduced scale.

Use these exact root rectangles. Coordinates are half-open: `[left,right) × [top,bottom)`; the bottom/right edge belongs to the next region, not both.

```text
Canvas:              [0,480) × [0,320)
Header:              [0,480) × [0,44)
Back-to-Home target:  [0,104) × [0,44)       104 × 44 px
First card:          [12,468) × [48,180)     456 × 132 px
  General, ID 0:     [12,468) × [48,92)      456 × 44 px
  Appearance, ID 2:  [12,468) × [92,136)     456 × 44 px
  Calendar, ID 3:    [12,468) × [136,180)    456 × 44 px
Group gap:           [0,480) × [180,184)     noninteractive
Second card:         [12,468) × [184,316)    456 × 132 px
  Wi-Fi, ID 1:       [12,468) × [184,228)    456 × 44 px
  Storage, ID 4:     [12,468) × [228,272)    456 × 44 px
  System, ID 5:      [12,468) × [272,316)    456 × 44 px
Bottom margin:       [0,480) × [316,320)     noninteractive
```

No row expands invisibly into a neighboring row, group gap or side margin. The corner rounding does not shrink its rectangular touch target. Six root rows consume 264 px; header, margins and the group gap consume the remaining 56 px. All root categories remain visible without gesture-only discovery.

Each root row has a **28 × 28 px** icon tile at x26, `rowTop+8`, corner radius 6. The label starts at x64, with a maximum width of 200 px. The summary ends at x424, with a maximum width of 148 px; it starts no earlier than x276. This leaves a 12 px content gap. The disclosure glyph occupies x444–452 and is centered vertically. The entire row navigates, not just its icon or chevron.

General summarizes `settings.use24Hour` as `12-hour`/`24-hour`. Appearance uses the selected theme's **display name**, not the lowercase storage ID. Calendar summarizes `settings.calendarEnabled` as `On`/`Off`; that summary must not imply that Classroom authorization or a recent remote sync exists. Wi-Fi uses the connected SSID, `Connecting...` for the existing idle/connecting state, otherwise `Not connected`. Storage uses `Ready`, `Needs setup`, or `No card`, based on `sdOk`/`sdPresent`. System uses `CLOCKOS_VERSION`. Do not display network passwords, token contents, private identifiers, or extra health claims.

## Category/detail hierarchy and exact targets

All normal detail pages use a 44 px navigation header, a noninteractive section caption band `[48,68)`, and a card starting at y68. Cards are x12, width456, radius12. Detail rows are **48 px tall**. Labels start at x28; a row's center is `top+24`. Information does not acquire a hit target merely because it looks like a list row.

```text
Common Back target:  [0,104) × [0,44)       104 × 44 px
Section caption:     [24,456) × [48,68)      noninteractive
Row 0:               [12,468) × [68,116)     456 × 48 px
Row 1:               [12,468) × [116,164)    456 × 48 px
Row 2:               [12,468) × [164,212)    456 × 48 px
Row 3, if present:   [12,468) × [212,260)    456 × 48 px
Row 4, if present:   [12,468) × [260,308)    456 × 48 px
```

### General

Header: **General**. Caption: **TIME & DISPLAY**. Row 0 is `24-hour time`, a switch bound to `settings.use24Hour`. Row 1 is `Battery percentage`, bound to `settings.showBatteryPercent`. Row 2 is `Appearance`, a disclosure with the current theme display name. Its destination is the same five-theme page used by the root category; Back from this shortcut returns to General.

Both toggle rows are whole-row targets with the same single action for label, empty space and switch. Invoke `saveSettings()` once per completed tap. No explicit Save button is added. When `!sdOk`, use the lower free space for a two-line session-only notice; retain the existing exit warning as well. Do not paint Home status content here.

### Wi-Fi

Use **one shared Wi-Fi detail body** for Settings and the Home Wi-Fi shortcut. Header: **Wi-Fi**. Caption: **NETWORKS**. Row 0 is `Network`, read-only, with the existing signal glyph and SSID/connection state. Row 1 is `Choose network`, a disclosure to `openWifiScanner(...)`. Row 2 is `Refresh networks`, also a disclosure because the current `S_WIFI` behavior opens the scanner. Row 3 is `Saved networks`, read-only, showing `None` or the existing count. This exposes the existing refresh operation consistently; it does not introduce saved-network editing or deletion.

Reserve the Network row separately: label x28–208, glyph slot x220–260, value slot x276–452, maximum 176 px. Call the existing glyph with `cx=240`, `by=rowTop+30`: `drawWifiSignal()` clears `[cx-20,cx+20) × [by-22,by+6)`, which remains inside this row. Never let that clearing rectangle erase SSID text or a separator. Use ASCII `Connecting...` if the current font cannot draw an ellipsis glyph. Update this status slot only, preserving the existing 180 ms animation cadence in the `S_WIFI` branch.

The remaining footer band y272–308 can hold two short wrapped lines explaining that network configuration is available here and offline choices stay in setup. Keep the existing setup/offline-startup behavior; do not add a normal Settings switch for Wi-Fi on/off or Offline Mode. The supplied developer page does not expose an offline control, so the UI must not claim one exists there.

Scanner Back returns to the Wi-Fi page with its original caller intact. Keyboard Cancel still returns to the scanner. **The inspected successful-connection path calls `showClassroomSetup()` and then the existing sync/setup completion flow even for a later reconnect** (L1836–1842). This review does not authorize silently replacing that behavior. Preserve it and first-run ordering; any separate change to successful reconnect routing needs its own behavior decision and validation.

### Appearance

Header: **Appearance**. Caption: **THEME**. Five rows, in existing order: **Crystal, Midnight, Ocean, Sunrise, Graphite**, backed by the unchanged IDs `crystal`, `midnight`, `ocean`, `sunrise`, `graphite`. All five 48 px rows fit from y68 to y308. Do not add a footer that overlaps the last row.

Tapping any row selects the theme, calls `applyAppearanceTheme()`, saves through `saveSettings()`, and remains on Appearance. The selected theme gets a vector checkmark and a subtle `UI_SELECTION` surface. Unselected themes have **no chevron** because they do not navigate. Repaint separators after the selected-row background, and round only the outer corners of the group—not all four corners of every internal row. A theme change may require one complete redraw because the page palette changes; repeated taps on the already-selected option need not repaint the full screen or rewrite the same setting.

Use this same body in `showAppearancePage()` if that legacy entry point remains. Its current two-column grid should not remain a second theme-selection architecture. Do not claim an installed downloadable-theme browser: the present list is five hardcoded options. If theme-path help is shown on some other existing entry point, use the actual `THEMES_DIR` (`/.source/themes/appearance`), not the current `/themes/appearance` string.

### Calendar settings

Header: **Calendar**. Caption: **CALENDAR & CLASSROOM**. Three switch rows: `Calendar` → `settings.calendarEnabled`; `Google Classroom` → `settings.classroomEnabled`; `Notifications` → `settings.notifications`. These are **preferences**, not the calendar grid. The Home Calendar shortcut continues opening `showCalendarPage()`.

Keep the toggles independent, matching the current stored behavior. Do not auto-enable Classroom when Calendar is enabled, disable Notifications automatically, or imply that setting a switch performs OAuth or a cloud refresh. A short wrapped footer can explain: `Classroom uses the read-only cache from Clock Setup.` and `Run Clock Setup on Windows to refresh it.` Redraw only the changed switch row. Do not call `drawRightPanel()` while this page is visible.

### Storage

Open the shared `showSdCardPage()` from the root Storage row **directly**, rather than showing Settings' three-row Storage intermediary and then another Storage page under `Prepare Storage`. Header: **Storage**. Caption: **SD CARD**. Use four 48 px rows: `ClockOS storage` (read-only status), `Capacity` (read-only), `Prepare ClockOS storage` (command), `Browse files` (disclosure). The existing direct file-browse action remains available, now from the canonical Storage page.

The Prepare row has action-colored text but **no chevron**: tapping runs `prepareSdCard()` on this page. Do not rename it `Format` or `Erase`, add a destructive icon, or claim it repartitions the card. Keep it callable without Wi-Fi. If no card is present, it remains usable as the existing detection/retry operation; failure produces the existing message. Browse remains usable and can show the existing no-card/empty read-only state rather than silently deleting this behavior.

Footer y272–308 has two lines maximum. Show either the preservation/offline explanation or a wrapped `sdCardMessage`, using error text only for an error—not green status text with weak light-theme contrast. Capacity should fit its value slot; shorten the label/value presentation without inventing free-space or write-success guarantees. Back from the read-only file viewer returns to this Storage page. Back from Storage returns to Settings root if entered there, or Home if opened by the Home SD shortcut. File enumeration and preparation internals are outside this visual review.

### System

Header: **System**. Caption **DEVICE** at y48–68. One information row `[12,468) × [68,116)` contains `ClockOS version` and `CLOCKOS_VERSION`. Keep its existing hidden developer-entry gesture: five completed taps with the existing `versionTapWindow`/2200 ms reset rule open `showDeveloperPinPage()`. It must not show a chevron suggesting an ordinary About page that does not exist.

A second noninteractive caption **RESET** occupies y128–148. The separate destructive disclosure row is exactly `[12,468) × [148,196)` (**456 × 48 px**) and reads `Factory Reset`. Its action only opens `showFactoryResetPage()`. The row never calls `factoryResetClock()` directly. Lower helper text explains that this is ClockOS-local data reset, not firmware replacement or whole-card erasure. Do not add a manual Software Update row or a device-information submenu in this scope.

The existing reset confirmation page remains a separate destination with **two separate button taps** required to execute reset. For consistent targets, its existing confirmation button can become `(76,248,328,48)`, hit rectangle `[76,404) × [248,296)`; all other explanatory and readiness surfaces are read-only. Back cancels arming and returns to System. Opening the page starts unarmed. A held contact cannot count as both confirmations.

The reset's exact deletion behavior must remain `factoryResetClock()`'s current behavior (L1597–1608). It deletes the listed preference files and token/cache files when `sdOk`, then restarts. It does **not** explicitly remove `CLASSROOM_SECRET_FILE` (`/data/secrets/classroomsecret.json`) or every possible ClockOS file. Do not broaden deletion under a UI change, or make the copy promise complete OAuth-client removal or secure erasure. Retain firmware/unrelated-file preservation and the existing no-ready-card restart behavior.

## Row styling, iconography and states

### Surfaces, type and alignment

Use the existing theme tokens: `UI_BACK` for the canvas, `UI_KEY` for grouped surfaces, `UI_TEXT` for primary labels, `UI_MUTED` for secondary values/captions, `UI_SEPARATOR` for 1 px separators and `UI_SELECTION` for press/selection tint. Do not hardcode a light background or reassign theme IDs. “Full-width” here means the entire content width between 12 px side margins: **456 px, with no navigation rail**.

Use `F18B` (`FreeSansBold12pt7b`) for centered navigation titles. Use `F12B` for root category labels and short section captions. Prefer `F12` (`FreeSans9pt7b`) for normal detail labels and secondary values; the hierarchy should come from position and accessory semantics rather than making every word bold. The `F12`/`F18B` names are not pixel sizes. Measure with `tft.textWidth()` and the actual selected font. Do not shrink a font dynamically to fit a long SSID or theme label.

Text rows reserve space **before** drawing. For ordinary navigation/value detail rows, use a label slot x28–268 (240 px), a value slot x284–424 (140 px), and a disclosure slot x444–452. For information rows without an accessory, extend the value's right edge to x452; retain at least a 16 px gap from the label. For switch rows, labels occupy x28–380 (352 px), and no trailing textual value is drawn. Footer text wraps within x24–456 and uses roughly 18 px line spacing; constrain it to the stated footer band. Use `fitText()` for ASCII labels/values after reserving accessory space. Its byte-at-a-time truncation is not evidence of safe non-ASCII SSID handling; test or use UTF-8-safe truncation if such SSIDs must be displayed.

Root separators run x64–454 and sit at each internal row boundary. Detail separators run x28–454. They stop before the card's outer boundary and do not cross group gaps. A pressed/selected fill is clipped to its row and the group's outer rounded corners. Restore separators and accessories after painting that fill.

### Category icons

Refine `drawSettingsCategoryIcon()` rather than depending on SD assets or copied Apple symbols. Use the same 28 px tile and a roughly 18–20 px high, 2 px-stroke glyph for every category. Category color is a wayfinding accent, never the only identifier. General uses a gear with visible teeth instead of two plain circles; Wi-Fi uses a consistent fan/dot; Appearance uses a half-light/half-dark disk rather than ambiguous overlapping circles; Calendar uses a calendar page with binding strokes; Storage uses a recognizable SD-card outline/contacts; System uses an information circle. Keep the adjacent textual labels.

Retain the current category-color intent (neutral, blue, purple, red, green, orange) but audit the white glyph against each tile after RGB565 quantization. The current bright green/yellow-orange-style backgrounds can make white strokes weak. Darken just those tile backgrounds if needed; this is not a theme or settings-data change. Do not repurpose a green tile as proof that an SD card is ready.

### Switch treatment and exact target

Increase `drawSettingsToggle()` from its current 46 × 28 px track to a **50 × 30 px** capsule. For a detail row at `top`, draw it at `(402,top+9)` with radius15. Draw a 12 px-radius white thumb centered at x417 when off and x437 when on, both at `top+24`. The track ends at x452, leaving a 16 px right inset from the card edge.

The switch's affordance is smaller than its activation target. The trailing accessory zone is `[396,460) × [top,top+48)` (**64 × 48 px**), but it is part of the same whole-row 456 × 48 px target—not a second overlapping action. A label tap and switch tap each toggle once. Off is not disabled: use a distinct off-track role such as proposed `UI_SWITCH_OFF`, not the overloaded `UI_DISABLED` text token. Preserve green for on, use a neutral off track, and retain thumb-position distinction. A truly disabled control, if added in future, needs separate behavior and appearance; this review introduces none.

Use immediate visual feedback with no long blocking animation. If animating the thumb, keep it short and nonblocking, repaint only its slot, and never animate by filling the whole page. Its persistent state must always be read from the relevant `settings` field.

### Headers and disclosure

The header is a calm, solid `UI_BACK` band. Its title is centered at `(240,22)` and constrained to x112–368. The Back target is exactly 104 × 44 px. Draw a small vector left-chevron at x12–18, centered on y22, and the parent label from x28. Root shows `Home`; direct category pages show `Settings`; shortcut Appearance shows `General`; file viewer shows `Storage`; reset/developer destinations show their actual parent. If a parent name is too long, show `Back` rather than clipping it or stealing the title's space.

Back text uses `UI_ACTION`. Ordinary disclosure glyphs use `UI_MUTED`, not the decorative `UI_BLUE`. Draw a right-chevron approximately 8 × 12 px centered on the row, with a readable 2 px stroke. The full row is the target; the chevron is never an independent tiny hit region. Only rows that open another page get it. Prepare, switches, version information, counts and unselected themes do not.

Do not display the profile/SD glyphs from `drawBackHeader()` as button-like global header controls on Settings descendants. They currently have no corresponding header actions there. The Storage category already exposes storage state, and the normal root currently has no such globals. Add a Settings-specific header helper or call a revised shared header in a no-global mode; do not change Home status indicators, expose `S_PROFILE`, or remove the existing Profile function. Calendar's existing `drawBackHeader("Calendar", false)` remains out of scope.

### Selection, press, information and destructive states

A navigational row briefly uses `UI_SELECTION` during a valid press, then the child page replaces it. Returning to this single-pane root does not leave a category persistently selected as though a detail pane were still visible. A theme selection persists as a vector checkmark (for example points `(438,cy)`, `(443,cy+5)`, `(453,cy-6)`) plus a subtle row tint. A switch persists through track/thumb position. Information rows have no press fill or action except the intentionally hidden version gesture.

Dragging out of the original row cancels activation and clears its press fill. A back swipe cancels any pending row tap, then pops the parent. Factory Reset uses an error/destructive text role; its opening disclosure is not a prominent filled “erase now” button. The actual confirmation button preserves its armed/unarmed text distinction and two-contact requirement.

Use existing `UI_ACTION` for action text and checkmarks rather than assuming `UI_BLUE` has sufficient text contrast in every theme. Add a scoped semantic `UI_DESTRUCTIVE_TEXT` if needed: bright `UI_RED` is suitable for some dark surfaces but should not automatically become small red text on white. Evaluate source palette values **after RGB565 conversion** against the intended surfaces: target at least 4.5:1 for normal text and 3:1 for essential non-text control boundaries. Color alone must not encode readiness, selected theme, on/off, or destruction. These are acceptance criteria, not claimed measurements of the physical panel.

## Navigation and behavior-preservation contract

A small nonpersistent navigation context is necessary. A proposed `SettingsNavFrame` records `Screen`, category and root/detail status; a bounded parent stack, or equivalent explicit parent fields, records who opened a shared destination. These are **new suggested constructs**, not names already present in the firmware. Depth four covers the scoped nested routes, including root > System > PIN > Developer Mode and root > Wi-Fi > scanner > keyboard. Do not put navigation history into SD preferences.

The canonical routes are:

```text
Home > Settings root
  > General > Appearance                 Back to General, then root
  > Appearance                           Back to root
  > Calendar preferences                 Back to root
  > Wi-Fi > scanner > password            Cancel to scanner; Back to Wi-Fi
  > Storage > read-only files             Back to Storage, then root
  > System > Factory Reset                Cancel to System
  > System > hidden PIN > Developer Mode   Preserve existing protected flow
Home > Wi-Fi                             Back to Home
Home > Storage > read-only files          Back to Storage, then Home
```

Keep the reconnect success/onboarding behavior noted above separate from scanner cancellation. A drawing function must not push history, switch origins, clear a PIN field, or arm a reset merely because the page needs repainting. Push context at deliberate entry, pop once on Back, and render the current frame on refresh. The Settings root entry from Home explicitly initializes root state instead of inheriting stale `settingsAtRoot`/`settingsCategory`.

Back and swipe-back are the same operation. At a category/child they return to the immediate parent. At Settings root they call `leaveSettingsPage()`, so `showSdSaveWarning()` remains the existing 1600 ms warning when `!sdOk`. Do not show that warning on every child pop or suppress it by routing directly to `showHome()`. Retain the in-memory-session behavior when storage is unavailable, with the conditional helper notice providing earlier context.

Preserve the five theme IDs, all existing preference fields and JSON keys, five-network storage/count semantics, independent Calendar/Classroom/Notifications toggles, setup and sync choices, sync-version rules, SD presence/readiness distinctions, folder-only offline preparation, read-only browsing, hidden five-tap developer entry and PIN behavior, reset confirmation/deletion scope, Home shortcuts, and existing update/sleep paths. This proposal does not introduce a new account, cloud sync, location service, sensor, update mechanism or persistence backend.

In particular, leave `TFT_W=480`, `TFT_H=320`, `TFT_ROTATION=3`, `TOUCH_CAL_X0=343`, `TOUCH_CAL_X1=3436`, `TOUCH_CAL_Y0=266`, `TOUCH_CAL_Y1=3381`, `TOUCH_CAL_ROTATION=1` and the active-high GPIO27 backlight configuration unchanged. Do not call `loadCal()` to substitute SD calibration. Keep `ensureCalibration()`'s `tft.setTouch(calData)` and `readTouch()`'s pressure threshold 350. The source/config do not establish physical calibration correctness.

## Exact firmware change points

All application change points below are in `.source/uncompiled/updates/ClockOSv2.8/ClockOSv2.8.ino`. They identify where a later implementation should work; **none were changed for this review**.

### Primary rendering and row definitions

`showSettingsPage()` (L1477–1551) owns root grouping, root order, summaries and General/Appearance/Calendar/System detail bodies. Replace visual-index mapping with an explicit ordered list of existing category IDs. Define the geometry once and render by row type. Route Wi-Fi/Storage to the canonical shared pages rather than keeping divergent case-1/case-4 bodies.

`drawSettingsCard()` (L1446–1450) needs correct group radius, row-height arguments, context-sensitive separator inset and clipped selected/pressed-row fills. `drawSettingsOption()` (L1452–1460) needs an explicit row kind, common vertical center, disjoint label/value/accessory slots and optional vector disclosure/checkmark. Its current collection of `enabled`, `toggle`, `disclosure` booleans makes invalid combinations too easy. `drawSettingsToggle()` (L1440–1444) owns the new 50 × 30 track and thumb geometry. `drawSettingsCategoryIcon()` (L1462–1475) owns the tile/glyph normalization and category-specific shapes.

The suggested **new** layout constants are `SETTINGS_MARGIN_X=12`, `SETTINGS_CARD_W=TFT_W-24`, `SETTINGS_HEADER_H=44`, `SETTINGS_ROOT_TOP=48`, `SETTINGS_ROOT_ROW_H=44`, `SETTINGS_ROOT_GROUP_GAP=4`, `SETTINGS_DETAIL_TOP=68` and `SETTINGS_DETAIL_ROW_H=48`. A suggested `SettingsRowKind` distinguishes disclosure, toggle, choice, information and command. A suggested `SettingsRowSpec` carries its exact rectangle and action. These names are proposals; the existing firmware has no central Settings geometry constants or such structs.

Use the same rectangle data in rendering and `contains(x,y)` hit tests. An action dispatcher can special-case the hidden version gesture without making every information row actionable. Do not retain `(y-52)/42` or `(y-56)/50` as a second, independently maintained source of geometry after changing the drawing.

### Headers, destinations and return provenance

`drawBackHeader()` (L1416–1424), `drawAppleButton()` (L1572–1575) and the two header paths inside `showSettingsPage()` own today's mixed Back sizes/styles. Add a Settings-specific parent-aware header with the 104 × 44 target. Leave unrelated `alarmButton()` and app-wide button dimensions unchanged. Use `drawBackHeader(..., false)` or a dedicated helper for scoped shared destinations; retain other pages' existing behavior.

`showWifiPage()` (L1649–1656) and `drawWifiPageStatus()` (L1640–1647) become the canonical four-row Wi-Fi body and contained status redraw. `openWifiScanner(uint8_t returnScreen)` (L863–866) and `scanReturnScreen` carry or reference the richer origin context while preserving their existing return-screen behavior. Scanner/keyboard internals need only provenance plumbing and a contact latch at entry; this review does not authorize keyboard or network-algorithm redesign.

`showSdCardPage()` (L1332–1346) becomes the canonical four-row Storage category body. `showSdFilesPage()` (L1348–1368) needs the consistent parent header and correct return context only; preserve read-only enumeration. `showAppearancePage()` (L1380–1390) delegates to the canonical theme-list body if retained. `showFactoryResetPage()` (L1553–1561) receives the parent-aware header, consistent confirmation target and entry-time arming reset; keep the existing reset operation. `showDeveloperPinPage()` (L1392–1401) and `showDeveloperModePage()` (L1403–1414) need parent-aware header/back plumbing only. In particular, do not route periodic repaint through the PIN-entry reset performed by `showDeveloperPinPage()`.

### Touch dispatch, refresh and persistence boundaries

`loop()` (L1771–2084) is the essential behavior change site. Relevant branches are Home entry (L1853–1865), scanner return (L1794–1809), Settings including setup guards (L1899–1960), and `S_SDCARD`, `S_SDFILES`, `S_APPEARANCE`, `S_DEV_PIN`, `S_DEV_MODE`, `S_WIFI`, `S_FACTORY_RESET` returns. Replace duplicated Back conditions and y-only action bands with current-page rectangle tests. Remove the Home-only redraw calls after General/Calendar toggles. Route Storage prepare/browse against their new row rectangles; preserve the current operations. Match the new System version/reset rectangles instead of retaining y56–106/y122–172.

`readTouch()` (L190–208) is the touch sampling boundary. Prefer an additional Settings-only event/latch layer over changing all application's touch semantics. If the function is extended to report contact down/up/move, audit all callers before enabling different behavior outside Settings. Maintain last-activity updates, pressure threshold and corrected calibration. For Settings, do not execute the row on the first down sample and later also execute a recognized back swipe from the same contact.

**Do not interpret `readTouch()==false` as release:** its 220 ms throttle also returns false while a contact is still present. A scoped event sampler must observe actual `tft.getTouch(...,350)` contact state before that throttle, or receive explicit down/move/up events from a refactored sampling boundary. As starting constants, sample at 20 ms, confirm release with two consecutive no-contact samples, and cancel a pending row tap after more than 12 px movement from its origin. Preserve the existing swipe thresholds (rightward movement over 90 px, vertical deviation under 70 px, within 900 ms); dispatch at most one back operation and suppress the pending tap. Carry the consumed-contact latch across destination entry so a held navigation tap cannot activate a switch, scanner row or confirmation on the new page. The timing/movement constants need physical validation; they are not measured panel characteristics.

`refreshSdState()` (L1093–1112) must update the visible storage summary or canonical Storage body without losing the parent, press state or current category. Its existing `screen==S_SETTINGS` full redraw also needs a guard for `firstSetupClassroom`/`firstSetupSync`: these setup pages share `S_SETTINGS`, so an SD state change must not replace a wizard page with the normal category UI while the setup flags still own input. Keep storage detection/preparation semantics unchanged. Wi-Fi status remains a contained periodic update, not a full root/detail repaint.

`leaveSettingsPage()` (L1309–1312) and `showSdSaveWarning()` (L1301–1307) remain the one root-exit warning boundary. `showHome()` (L1689–1695), `drawStatusBlock()` (L1077–1091) and `drawRightPanel()` (L1114–1130) should **not** be redesigned; the fix is restricting their call sites to Home.

Keep `saveSettings()`/`loadSettings()` (L647–688), `saveCreds()`/`loadCreds()`, `prepareSdCard()`, `ensureClockOsSdLayout()`, `clockOsSdReady()`, `factoryResetClock()`, `applyAppearanceTheme()`'s theme identity handling and `syncVersionCompatible()` unchanged in behavior. `applyAppearanceTheme()` (L50–75) may receive scoped semantic color roles for readable action/destructive/off-track styling, but must not alter stored IDs. Configuration paths including `SETTINGS_FILE`, `WIFI_FILE`, `ALARM_FILE`, `TOUCH_FILE`, `THEMES_DIR` and the Classroom paths remain unchanged. There is no required `config.h` change for this layout.

## Acceptance criteria for the later implementation

This review ran no firmware build, automated application test, simulator screenshot, device interaction, or Windows validation. The following are required checks for the future implementation, not claims of completed validation.

At the geometry level, every defined rectangle stays within 480 × 320. Root group gaps, captions, footer text and side margins do nothing when tapped. Test each row's first/last included pixel, the adjacent excluded pixel and each separator. Five theme rows must remain visible at y68–308; the last theme must never be covered by help text. Use the same row specs for these assertions and runtime hit testing.

At the interaction level, hold each switch longer than 220 ms: it changes once, not repeatedly. Hold Factory Reset's confirmation: it only arms once, and reset requires a second released/repressed contact. Drag from a row to another row or a back swipe: no unintended toggle, theme selection or destructive activation occurs. Back/button and swipe-back have identical results, including root exit warnings. Opening Settings after reconnect/setup completion explicitly opens root. General > Appearance > Back returns to General; Home > Storage > Files returns through Storage to Home.

At the rendering level, toggling Calendar cannot paint weather over the right half, and toggling General cannot paint Home status controls at the bottom. Verify idle, connected and connecting Wi-Fi states without text/icon collisions. Check all five themes, long SSIDs, longest category labels, saved-network counts, all three SD statuses, capacity text and long preparation failure messages. Check vector Back/checkmark/disclosure glyphs instead of assuming UTF-8 symbols work. Selection and press fills must preserve group corners and separators. Validate palette contrast after RGB565 conversion; device appearance remains a separate hardware check.

At the behavior level, preferences still apply immediately and persist only through the current SD-ready paths. With no usable card, the session continues and root exit still warns. Storage preparation remains non-erasing/offline and browsing remains read-only. Classroom toggles do not perform authorization. First-run Classroom/sync choices remain ahead of normal Settings dispatch and survive SD-state refresh. Existing successful Wi-Fi connection/setup ordering and update behavior are preserved. Hidden developer access and exact reset deletion scope remain unchanged.

Finally, compile using the repository's documented ESP32/TFT_eSPI/PNGdec/ArduinoJson build setup and follow its release policy before publishing a later binary. Automated geometry/source checks and a successful build cannot establish panel rotation, physical touch accuracy or real switch usability. On the actual Hosyond board, verify all corners/center, edge rows, Back, holds, swipes, SD transitions and both reset confirmations; record those checks only when performed. Do not overwrite calibration or mark the existing physical-validation TODO complete from this report.

## References

[1]: https://developer.apple.com/design/human-interface-guidelines/lists-and-tables "Apple Human Interface Guidelines — Lists and tables"
[2]: https://developer.apple.com/design/human-interface-guidelines/buttons "Apple Human Interface Guidelines — Buttons"
[3]: https://developer.apple.com/design/human-interface-guidelines/accessibility "Apple Human Interface Guidelines — Accessibility"

## Ordered implementation checklist

1. **Fix screen ownership first:** remove `drawStatusBlock()`/`drawRightPanel()` from Settings toggle handlers while retaining immediate field changes and `saveSettings()` calls.
2. Introduce explicit root/category IDs, proposed shared row specs, row kinds and geometry constants; preserve the existing stored category IDs and all configuration/calibration values.
3. Add deliberate Settings-root entry and bounded parent/origin context; distinguish entering a page from repainting it and protect `firstSetupClassroom`/`firstSetupSync` dispatch.
4. Implement the 44 px header and 104 × 44 Back target with vector glyphs; route Back and swipe-back through the same parent/root-exit operation.
5. Render the two root cards at y48–180 and y184–316, with the ordered six 44 px rows, 28 px category icons, bounded summaries and neutral disclosures.
6. Rework `drawSettingsCard()`/`drawSettingsOption()` around row kinds, shared centers, non-overlapping slots and context-appropriate separators; add clipped press/selection painting.
7. Implement the 50 × 30 switch treatment inside whole-row 456 × 48 targets; separate off-track styling from disabled styling and preserve independent preference semantics.
8. Render General/Calendar with 48 px rows; keep the General Appearance shortcut and replace theme chevrons/Unicode checks with canonical five-row selection behavior and a vector checkmark.
9. Consolidate Wi-Fi rendering in `showWifiPage()`/`drawWifiPageStatus()`; preserve Choose/Refresh/scanner/password/setup behavior and keep status animation in its reserved slot.
10. Route root Storage directly to the shared four-row `showSdCardPage()`; retain capacity, non-erasing offline preparation, read-only browsing, failure messages and caller-correct returns.
11. Update System's exact version/reset rectangles and reset confirmation target; preserve the hidden five-tap/PIN flow, distinct reset contacts and `factoryResetClock()` deletion behavior.
12. Add a Settings-only press/release/drag latch with one activation per contact, rectangle-bound dispatch and swipe cancellation; leave pressure threshold and unrelated touch flows intact.
13. Make SD/Wi-Fi refreshes update only appropriate visible content without replacing setup pages or mutating navigation; retain the existing session-only exit warning and audit contrast/text fit across all five themes.
14. Validate geometry, navigation, holds, edge pixels and preservation cases; compile and publish only through the project's release process, then separately perform and record physical Hosyond display/touch checks before claiming hardware readiness.
