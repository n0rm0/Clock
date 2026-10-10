# ClockOSv2.8 visual system: quiet panels, explicit states, dependable text

**Retain the large seven-segment clock and the full-width Settings hierarchy. Consolidate their surrounding primitives into a small semantic token system before attempting a larger redesign.** The strongest direction is an original **ClockOS Quiet Panels** family: a dark, glanceable home display; opaque grouped surfaces for configuration; restrained action color; and state changes that remain understandable without color. This evokes StandBy's information-at-a-distance purpose and iPadOS-style hierarchy without importing Apple fonts, symbols, branding, glass effects, or navigation behavior. Apple's StandBy description supports the glanceable emphasis; its color guidance supports semantic roles and light/dark variants.[1] [2]

This is a **static source review**, not a rendered-device assessment. No firmware was compiled or flashed, no tests were run, and no physical ST7796S display, touch, backlight, battery, SD card, or Windows UI was validated. Application source, configuration, manifests, TODOs, binaries, tests, and Git state were not modified by this review.

The required baseline was read: [TODO](../todo.md), [conventions](../agent.md), [README](../../README.md), [ClockOSv2.8.ino](../../.source/uncompiled/updates/ClockOSv2.8/ClockOSv2.8.ino), and [config.h](../../.source/uncompiled/updates/ClockOSv2.8/config.h). Adjacent inspection was limited to the five theme packages and their README, the active `bootloader.h`, and the installer's weather-icon sizing/export path. Unless otherwise specified, line references below identify the supplied **ClockOSv2.8.ino** snapshot.

## Preserve the implemented behavior and hardware contract

`config.h` defines `TFT_W = 480`, `TFT_H = 320`, `TFT_ROTATION = 3`, active-high `TFT_BACKLIGHT_PIN = 27`, and the supplied `TOUCH_CAL_*` mapping. Leave these values and `ensureCalibration()` / `readTouch()` unchanged during visual work. Do not tune inversion, RGB/BGR order, touch pressure, swipe thresholds, or SPI settings to compensate for an unmeasured visual issue.

Keep the `Screen` routes, setup-resume sequence, offline manual clock entry, date/time toggle, alarm editing and repeat masks, enable switches, any-touch alarm dismissal, SD-only persistence, storage preparation, read-only file browsing, Classroom toggles/cache, Main/Side choices, developer flow, two-stage factory reset, OTA fallback, and the one-minute **home-only** sleep policy. Preserve all current data sources and limits: outside weather, Philadelphia default/manual-city choices, five networks, four alarms, eight cached assignment slots, and three items on the extended Calendar page. Do not introduce indoor temperature/humidity sensors, an RTC, ambient-light detection, automatic red night tint, always-on display, new sync transport, snooze, audio, or haptics as part of this design pass.

`README.md` and the latest TODO describe v2.8 as built but leave physical checks open. Older wording in `agent.md` describes a different baseline and indoor sensor ambitions; it is not evidence of current behavior. Similarly, a next-alarm row and abbreviated weekday are not drawn by the current `drawTimeBlock()` / `drawStatusBlock()`. This review does not treat those historical expectations as implemented widgets to preserve or silently add.

## 1. Colors: keep the semantic foundation, remove competing palettes

### Observations

`applyAppearanceTheme()` at lines 50–75 is already a useful starting point. It defines `UI_BACK`, `UI_KEY`, `UI_TEXT`, `UI_MUTED`, `UI_SEPARATOR`, `UI_SELECTION`, `UI_DISABLED`, and, importantly, the contrast-paired `UI_ACTION` / `UI_ON_ACTION`. `drawAppleButton()` at lines 1572–1575 uses the pair correctly. The selected theme is stored by `saveSettings()` and restored by `loadSettings()`; `appearanceTheme` initially defaults to **midnight**.

Theme consistency stops at the page helpers. `drawTimeBlock()`, `drawStatusBlock()`, `drawRightPanel()`, and `showHome()` use literal black plus `COL_WHITE` / `COL_DIM` and `0x7BEF` dividers. `drawUpdatePage()` defines its own dark palette locally. `drawKbKey()` defines another graphite/blue palette. Welcome, alarm, profile, developer, Wi-Fi, Calendar, and reset helpers contain many literal `C(...)` colors. Some are appropriate illustration colors; others are competing text, action, selection, or state roles.

`UI_BLUE` is both a decorative theme color and a selected-control fill. `alarmButton()` at lines 1174–1178 fills selected controls with `UI_BLUE` but always draws `UI_TEXT`. In **Graphite**, that is white text on a near-white fill. In **Ocean**, it is dark text on saturated blue. `settingsRow()` and `settingsGroupRow()` ignore the theme's `UI_BLUE` altogether and use local `C(0, 122, 255)` for accent values. `UI_RED`, `UI_YEL`, and `UI_GREEN` are identical across all themes, although luminous indicator fills are not necessarily suitable small-text inks on white cards.

Representative static contrast estimates expose these mismatches. Using standard sRGB relative luminance after RGB565 channel truncation and bit-replication expansion, Graphite's selected `alarmButton()` label is approximately **1.35:1**; Ocean's is **2.84:1**. Crystal's Welcome subtitle `C(190,194,202)` on white is **1.77:1**. Crystal's `UI_GREEN` Ready text on white is **1.98:1**, and `UI_RED` warning text is **3.41:1**. Midnight's alarm-list hint `C(92,96,106)` on its background is **3.19:1**. Sunrise's current white-on-`UI_ACTION` button is close to the small-text threshold at approximately **4.46:1**, despite looking adequate before quantization. These are software screening estimates, **not panel measurements or accessibility certification**.

### Recommended token contract

Keep the five theme IDs and all saved JSON keys unchanged. Define a future responsibility-specific `visual.h`, or an equivalent clearly bounded visual section, containing palette and metric definitions; leave hardware/data-path definitions in `config.h`. Initially maintain existing globals as compatibility aliases so the migration is incremental:

```cpp
// Proposed names; these are not present in the current firmware.
UI_BG                // existing UI_BACK: page canvas
UI_SURFACE           // existing UI_KEY: opaque card face
UI_SURFACE_PRESSED   // new: neutral pressed face, theme-specific
UI_TEXT_PRIMARY      // existing UI_TEXT
UI_TEXT_SECONDARY    // existing UI_MUTED; still readable small text
UI_BORDER            // new: structural outline when actually needed
UI_DIVIDER           // existing UI_SEPARATOR; decorative separation only
UI_ACCENT            // existing UI_BLUE: restrained brand/illustration tint
UI_LINK              // new: contrast-qualified action text/disclosure ink
UI_ACTION_FILL       // existing UI_ACTION
UI_ACTION_INK        // existing UI_ON_ACTION
UI_SELECTED_FILL     // existing UI_SELECTION
UI_SELECTED_INK      // usually UI_TEXT; explicitly tested on selection fill
UI_DISABLED_FILL     // existing UI_DISABLED where it is a disabled track
UI_DISABLED_INK      // new: never a replacement for essential explanatory text
UI_SUCCESS_INK       // new: theme-specific status text
UI_WARNING_INK       // new: theme-specific caution text
UI_DANGER_INK        // new: theme-specific warning/destructive text
UI_SUCCESS_FILL      // existing UI_GREEN may remain an indicator fill
UI_WARNING_FILL      // existing UI_YEL may remain an indicator fill
UI_DANGER_FILL       // explicit destructive/low-battery fill
UI_ON_DANGER         // tested ink on destructive fill
UI_TOGGLE_ON, UI_TOGGLE_OFF, UI_TOGGLE_KNOB
```

The critical distinction is **ink versus fill**, not a proliferation of subtly different colors. Keep literal white when the role is genuinely white artwork, such as a snowflake or a contrast-paired tile glyph. Do not globally redefine `COL_WHITE` as theme text: it is used by both the clock and illustrations. `UI_LIP` is currently assigned but not consumed elsewhere in this sketch; do not build a shadow system around that legacy name.

Retain current page/background/surface values initially. As an original accent direction, Midnight can use `C(128,175,255)` for link ink rather than repeating Apple's exact system-blue value everywhere; Crystal/Ocean can use `C(36,95,197)` for a strong light-surface link. Suggested **candidate** light-theme status inks are success `C(23,101,60)`, danger `C(179,38,36)`, and warning `C(122,74,0)`. Their RGB565 values are `0x1327`, `0xB124`, and `0x7A40`; normalized-channel contrast against white is approximately 7.1:1, 6.5:1, and 7.5:1. Candidate Midnight status inks are success `C(111,221,159)`, danger `C(255,140,134)`, and warning `C(234,196,116)`. They each screen above 8:1 against the current Midnight surface. For Sunrise, `C(176,59,26)` is a candidate action fill with roughly 5.9:1 white-text contrast. These are starting specifications, not a claim that every candidate has passed every theme/surface/state combination.

Use a conservative engineering acceptance target of **at least 4.5:1 for all small system text**, preferably 5:1 or better after RGB565 conversion. Aim for 3:1 for essential nontext control boundaries/glyphs where recognition depends on them. Dividers may be quieter when grouping remains clear through spacing. Apple's accessibility guidance motivates contrast and sufficiently sized controls, but its point dimensions do not directly translate to this panel's pixels.[3]

### Apply the roles to exact existing helpers

For `alarmButton()`, separate **selection** from **primary action**. Repeat-day and setup-choice selections should use `UI_SELECTED_FILL` + `UI_SELECTED_INK` with an outline or small drawn check, while Save/Enter/Enable-as-action uses the existing action pair through a shared button renderer. Preserve the boolean state and all current handlers. For `drawKey()`, replace the fixed pressed `C(62,68,82)` with the neutral pressed pair; for `drawKbKey()`, use explicitly scoped keyboard tokens instead of hard-coded face/white values. Do not use zero as the only “no face supplied” sentinel in a new API: a valid RGB565 black fill is also zero.

For `showWelcome()`, `drawAlarmList()`, `drawAlarmEditor()`, `showAlarmRinging()`, `showProfilePage()`, `showDeveloperModePage()`, and `showFactoryResetPage()`, replace hard-coded gray text with the semantic secondary ink appropriate to the actual surface. In `drawSettingsOption()`, `showSettingsPage()`, `settingsRow()`, `settingsGroupRow()`, and `showCalendarPage()`, use link or selection ink by role rather than decorative `UI_BLUE` or a local blue literal.

For Factory Reset, keep the first stage visually quiet but explicitly destructive in its label. At the armed second stage, use `UI_DANGER_FILL` / `UI_ON_DANGER`, not the generic primary blue selected by `drawAppleButton(..., factoryResetArmed)`. The two taps, readiness row, data-only scope, and `factoryResetClock()` execution path remain unchanged. Do not imply the entire SD card or firmware will be erased.

### Explicit home and keyboard scopes

Preserving the existing fixed-black home is a defensible StandBy-like choice, not necessarily a theme bug. Make it explicit:

```cpp
// Proposed fixed-dark home roles.
HOME_BG         = 0x0000;
HOME_TIME_INK   = 0xFFFF;
HOME_TEXT_INK   = 0xFFFF;
HOME_MUTED_INK  = /* tested gray, initially COL_DIM */;
HOME_DIVIDER    = /* quiet dark neutral, not body-text gray */;
HOME_CONTROL_BG = /* existing graphite control face, tokenized */;
```

Route `showHome()`, `drawTimeBlock()`, `drawBigTime()`, `drawStatusBlock()`, and `drawRightPanel()` through these roles. Let theme pages change while the nightstand home remains predictably dark. If a future decision makes home theme-responsive, treat that as an explicit separate design choice, not an accidental side effect of replacing `COL_WHITE`.

Similarly, either make the keyboard theme-responsive or declare a complete fixed-dark keyboard scope: tray, cap, pressed cap, ink, and field. The current combination of a light `UI_KEY` tray and hard-coded dark caps is incomplete. A fixed-dark tray is the lower-risk option because it retains existing cap contrast and layout. Do not add translucent blur or animated glass; an opaque surface hierarchy is legible and matches the actual primitive renderer.

## 2. Theme packages: acknowledge what is and is not active

### Observations

The five JSON packages at `.source/themes/appearance/` contain `id`, `name`, `version`, `style`, `background`, `surface`, `primary`, `text`, `muted`, and `accent`. **The current sketch does not parse these packages.** `THEMES_DIR` participates in SD directory creation/readiness checks, while `applyAppearanceTheme()` hard-codes the palette. `showAppearancePage()` and `showSettingsPage()` select built-in IDs, not downloaded palette values.

The package README says Crystal is the default, but current code initializes and falls back to Midnight when a saved setting is absent. Sunrise's package `muted` is `#8F6A60`; code uses `C(122,84,74)` instead. Package `accent` and `style` are not consumed by the sketch. The footer in `showAppearancePage()` says `/themes/appearance`, omitting the actual `/.source` prefix. None of these facts proves downloadable theme application currently works.

### Recommendations

For this visual migration, keep **built-in palettes as the runtime authority** and retain the current absence-of-setting Midnight default. Preserve the current unknown-theme fallback behavior unless a separately approved compatibility decision changes it: the `else` branch of `applyAppearanceTheme()` currently supplies Crystal colors even for an unrecognized ID.

Define a future schema mapping `background → UI_BG`, `surface → UI_SURFACE`, `text → UI_TEXT_PRIMARY`, and `muted → UI_TEXT_SECONDARY`. Treat the existing package `primary` as decorative accent, **not automatically as an action-button fill**. Add explicit action/state fields only in a later versioned schema with compatible defaults; do not pretend the current schema already supplies contrast pairs. Theme loading, validation, package updates, README wording, and installer changes are separate work and were not performed here. The visual report's implementation sequence can proceed without a new loader or any changes to persistence paths.

Appearance selection should use a stable drawn checkmark and a soft selected fill, with a readable theme name. Keep all five choices. A tiny paired background/surface swatch is optional after text and control fit is solved; it must not take over the row's hit target or replace the selected-state mark.

## 3. Typography: role names, verified glyphs, bounded strings

### Observations

The actual aliases at lines 149–152 are `F12B = &FreeSansBold9pt7b`, `F18B = &FreeSansBold12pt7b`, `F24B = &FreeSansBold18pt7b`, and `F12 = &FreeSans9pt7b`. Their names no longer describe the point sizes. This causes implementation ambiguity even though the existing hierarchy is sensible: compact body, a 12pt heading, and 18pt emphasis around the much larger drawn digits.

`txt()` at lines 134–139 sets the font, foreground color, and datum, then draws an unbounded string. It does not clear a previous field. `fitText()` at lines 141–146 uses measured width, which is better than character counts, but removes one byte at a time. It can split UTF-8 input and can still return `...` wider than a very small budget. Several callers bypass it: `drawPasswordField()` keeps the last 20 characters/bytes, `showKeyboard()` truncates a heading to 30, `showScan()` truncates SSIDs to 26, `drawUpdatePage()` truncates status to 52, and file browsing truncates names to 30 before measuring.

Upstream TFT_eSPI's `FreeSans9pt7b` and `FreeSansBold9pt7b` headers declare the glyph interval **0x20–0x7E** and `yAdvance = 22`.[4] [5] Local font headers from the actual build were not available for inspection, so this is upstream evidence, not proof of the linked release's complete font coverage. The current literals `✓` in `showSettingsPage()`, `‹ Back` through `drawAppleButton()`, and `Connecting…` in `drawWifiPageStatus()` cannot be assumed to render with these ASCII free fonts. The degree symbol in `drawRightPanel()` also requires verifying the actual `FreeSansBold12pt7b` coverage; do not assume it merely because the string compiles. SSIDs, city names, and Classroom titles may contain unsupported characters.

### Recommendations and exact mapping

Add semantic aliases without changing font pointers in the first pass:

```cpp
// Proposed aliases to existing font resources.
FONT_BODY        = F12;   // FreeSans9pt7b
FONT_LABEL       = F12B;  // FreeSansBold9pt7b
FONT_HEADING     = F18B;  // FreeSansBold12pt7b
FONT_EMPHASIS    = F24B;  // FreeSansBold18pt7b
BODY_LINE_PITCH  = 22;    // starting value; confirm linked font metrics
```

Use regular body text for help, metadata, and long values; bold labels for actionable rows and short headings; heading size for page titles and alarm-list time; emphasis only for setup identity, active alarm time, and manual time entry. Do not shrink body text below the existing 9pt resource to make long explanations fit. Do not introduce SF Pro or SF Symbols to achieve the resemblance: keep FreeSans and original geometry, then improve spacing and weight discipline.

Retain `txt()` as the low-level drawing primitive. Add proposed `drawTextInRect()` and `drawWrappedText()` helpers that accept a rectangle, font, foreground, background, datum/alignment, and maximum line count. Clear the **owned field rectangle**, not only the width of the new text. This prevents old digits or labels remaining when a string gets shorter. Make fit operations use the same font and size state as drawing; explicitly reset any text padding/size state so callers do not inherit unrelated renderer settings.

Improve `fitText()` to handle zero/negative budgets and an ellipsis wider than the budget. Iterate at complete code-point boundaries, or create a display-only supported-character string before measuring. A displayed ASCII fallback must **never mutate stored SSIDs, credentials, Classroom titles, city input, or the bytes used for network connection**. Keep `...` rather than a Unicode ellipsis until coverage is verified. Draw checkmarks, back/disclosure chevrons, and the small degree circle with original line/circle primitives; doing so removes font dependency for essential symbols.

For `drawPasswordField()`, measure a tail that fits its inner width, approximately 310 pixels in the current 330-pixel face. Preserve masking, Show/Hide, maximum input length, city reuse, and existing keyboard layer behavior. The displayed suffix can change without changing the password buffer. For `showKeyboard()`, bound the title between the Back control and the right edge, and give `errMsg` a separate measured one- or two-line rectangle above the keyboard tray.

For `showScan()`, fit SSIDs to the actual 460-pixel row minus internal padding. Wrap `scanStatus` when no networks are found instead of placing the entire message on one line. For `drawRightPanel()`, fit the city and all empty-state labels to a consistent right-panel content width; retain the current icon/temperature/city hierarchy and absence of a long weather-condition label. For `showCalendarPage()`, measure titles and due text separately where space allows, so an ellipsis on an assignment title does not unnecessarily remove its due date.

Long footer text in `showWifiPage()` and `showWeatherPage()` is especially likely to exceed a 480-pixel line. Allocate a two-line, centered or left-aligned footer within the bottom content area, about 420 pixels wide and 44 pixels high. Apply the same bounded treatment to `showSyncSetup()` compatibility text, `showProfilePage()` profile name/help, `showFactoryResetPage()` explanatory lines, `showSdCardPage()` feedback, and `drawUpdatePage()` status. Long text must never intrude into an adjacent button or below y=319.

## 4. Cards, radii, dividers, and spacing: one family, not one size

### Observations

There is no shared metric vocabulary. Existing radii include 3 for small status shapes, 6 for keyboard/category caps, 8 for the password field, 9 for `alarmButton()`, 10/11 for settings rows, 12 for alarm rows, 13 for `drawSettingsCard()`, 14 for warnings/reset, 16 for the keyboard tray, 18 for Update, 22 for Profile, 24 for Welcome/ringing cards, and `h/2` for every `drawAppleButton()`. The result mixes capsules, individual rounded rows, and larger rounded panels without a clear hierarchy.

`drawSettingsCard()` at lines 1446–1450 correctly groups rows and draws inset separators. Its fixed x=64 separator start fits the root category list's icon/text column, but not detail pages whose titles start at x=26. `settingsGroupRow()` draws each full-width row as a separate 38-pixel card. `settingsRow()` draws 210-pixel cards and budgets 125 pixels for the label plus 64 for the trailing value: within the 186-pixel interior between x+12 and x+198, those budgets can overlap by three pixels before allowing any gap.

`drawSettingsOption()` puts title/detail centers at y+17, while its toggle and chevron center around y+23. In 50-pixel rows, content is not vertically aligned. Its maximum title span x=26..366 and detail span x=300..418 can overlap by 66 pixels. These are helper-budget defects even when current short titles happen not to collide.

On Home, `drawBigTime()` uses `w=44`, `h=92`, `t=10`, `gap=8`, and colon width `cw=20`. A five-character time such as `12:59` or `--:--` occupies **228 pixels**, leaving only six pixels on each side of the 240-pixel column. A three-digit time such as `9:59` occupies 176 pixels and is centered separately. Enforcing a universal 16-pixel home margin without changing digit geometry would therefore clip the longer time.

### Recommendations

Use a **4-pixel spacing unit**, but retain explicit exceptions needed by current layout and touch geometry. Suggested future tokens are:

```cpp
SPACE_1 = 4;  SPACE_2 = 8;  SPACE_3 = 12;
SPACE_4 = 16; SPACE_5 = 20; SPACE_6 = 24;
PAGE_INSET = 12;
ROW_TEXT_INSET = 14;
INLINE_GAP = 12;
RADIUS_ICON_TILE = 6;
RADIUS_FIELD = 8;
RADIUS_CONTROL = 10;
RADIUS_CARD = 14;
RADIUS_FEATURE_CARD = 20;
STROKE_ICON = 2;
STROKE_DIVIDER = 1;
SETTINGS_ROOT_ROW_H = 42;     // preserve six rows through y=303
SETTINGS_DETAIL_ROW_H = 50;
APPEARANCE_ROW_H = 46;
HOME_TIME_AREA_W = 240;      // preserve 228-pixel time fit
HOME_DIGIT_W = 44; HOME_DIGIT_H = 92;
HOME_DIGIT_STROKE = 10; HOME_DIGIT_GAP = 8; HOME_COLON_W = 20;
```

Use `RADIUS_CONTROL` for ordinary buttons and `RADIUS_CARD` for grouped containers; reserve capsules for toggles, small status chips, and progress tracks. Use `RADIUS_FEATURE_CARD` for Welcome, active-alarm, Profile, and Update panels. These values are proposed harmonization targets, not instructions to change every coordinate at once. Keep `drawDigit()`'s rounded segment shape and `SEG_MASK` intact; the seven-segment display is the project's strongest original visual signature.

Give `drawSettingsCard()` a separator-inset parameter, or create root/detail wrappers: root separator starts at 64, detail separator starts at 26, and both stop before the rounded right edge. Add a proposed shared disclosure/check helper rather than drawing slightly different chevrons in `drawSettingsOption()` and `showSettingsPage()`.

For `drawSettingsOption()`, receive or derive row height and use a common center `y + rowHeight/2` for label, value, toggle, and disclosure. Reserve trailing-control space first. Then allocate the detail width and a 12-pixel gap; only the remaining width belongs to the title. For `settingsRow()`, a value budget of 64 requires a label budget of **110 pixels or less** if a 12-pixel gap is kept. `settingsGroupRow()` already has a workable maximum label/value separation: x=30..275 and x=287..447. Preserve that geometry while centralizing it.

The root Settings list must remain full-width with six 42-pixel hit rows. Do not restore a sidebar. Small standalone settings rows can move toward grouped surfaces in a later pass, but retain their current touch targets and routes until draw/hit rectangles are migrated together. Introduce a shared rectangle representation for visual ownership and hit testing only as an incremental refactor; changing appearance must not silently alter which control a touch activates.

For Home, keep the x=240 split and existing touch partition. Replace the bright `0x7BEF` structural lines in `showHome()` and `drawRightPanel()` with `HOME_DIVIDER`; do not reuse the muted body-text color as a rule. Use visual spacing and optional very subtle module surfaces to establish hierarchy, rather than adding a border around every item. Do not add cards that reduce the time area below 228 pixels. Keep the right weather/calendar module boundaries and existing time/date/status regions identifiable.

For Calendar, align weekday labels with the seven numeric columns rather than relying on the string `"S    M    T    W    T    F    S"` rendered in a proportional font. Use `UI_DIVIDER` for its y=224 rule. Keep the current month/date/data behavior: the `first = 0` placement logic is a separate calendar-correctness issue, not something a spacing refactor should silently rewrite. A current-day ring or small selected background provides an additional state cue besides blue text.

## 5. Icons and status indicators: consistent optical scale and truthful states

### Observations

Original vector helpers already exist: `drawProfileIcon()`, `drawAlarmIcon()`, `drawSettingsGear()`, `drawWifiSignal()`, `drawBattery()`, `drawSdCardIcon()`, `drawSettingsCategoryIcon()`, and `drawAppleUpdateIcon()`. They range from thin one-pixel lines to filled silhouettes and three-pixel Wi-Fi arcs. `drawSettingsGear()` is defined but not called in the current sketch; the home Settings control uses text instead.

The home control at lines 1089–1090 is 58 pixels wide, but upstream Bold9 metrics give `Settings` an approximately **71-pixel** advance.[5] Its centered text extends outside the face. A regular9 `100%` value uses approximately 46 pixels from x=142 to x=188, which overlaps the Settings face beginning at x=171. This is a concrete horizontal budget issue, not merely an aesthetic preference.

Home calls `drawSdCardIcon(70, 294, sdPresent, false)`. Consequently, an inserted but not-ready card never receives the X shown in headers. `drawBackHeader()` instead passes `sdOk` and `sdPresent && !sdOk`. The helper's `inserted` argument actually determines whether the icon is filled; its absent-state fill is `UI_BACK`, which can produce a light or tinted rectangle on the fixed-black home.

`drawWifiPageStatus()` at lines 1640–1647 draws a trailing state/SSID ending at x=447 with up to 160 pixels of width. It then draws the Wi-Fi icon at x=364, whose clear rectangle covers x=344..383. This can erase part of the text it just drew. The Wi-Fi icon and text need separate owned regions.

Battery state uses thresholds `pct <= 20`, `pct <= 45`, otherwise green. The percentage comes from `readBatteryPct()`'s ADC estimate. There is no charger-state detection in this helper. Wi-Fi status is derived from `WiFi.status()` / `rssiLevel()`; no new failure or disabled state should be invented from a decorative animation frame.

### Recommendations and mapping

Adopt original **24-pixel utility glyphs**, **26-pixel category tiles**, and a **roughly 32 × 24 optical envelope** for home status symbols, with a two-pixel default stroke where appropriate. Filled profile silhouettes and Wi-Fi arcs can remain optical exceptions. Normalize placement using slot centers and bounds, not by forcing every icon to contain exactly the same number of pixels. Do not replace these helpers with copied SF Symbols.

Replace the overflowing home `Settings` text with `drawSettingsGear()` inside a small tokenized control face, preserving the existing `showSettingsPage()` route and x>=166, y>=270 activation region. One feasible visual budget is percentage x=142..188, an 8-pixel gap, and gear face x=196..228 with center x=212. This keeps the full `100%` value and fits within the current left column. The actual Settings page still supplies the readable title. Do not shrink the label into illegibility or hide battery percentage when the preference is enabled.

Make SD icon background explicit. Preserve its three visual states: **ready = filled**, **absent = outline**, **present but not ready = X-marked**. For Home, pass `sdOk` and `sdPresent && !sdOk`, plus `HOME_BG` through a proposed background-aware helper. For page headers/warnings, use the actual page/card background. Keep `sdPresent`, `sdOk`, `clockOsSdReady()`, `ensureClockOsSdLayout()`, and `refreshSdState()` detection/preparation semantics unchanged. Add no “saved successfully” visual claim merely because a filled icon was painted; persistence success is not established by the icon alone.

For `drawWifiPageStatus()`, allocate a left label region, a bounded SSID/state region ending before the icon, and a right icon slot. For example, reserve x=414..454 for the icon and end the trailing text at x=402; choose the remaining text width from the row interior. Preserve connected state, RSSI levels, and the existing connecting animation. Connected status can use `UI_SUCCESS_INK`, but it must retain an explicit readable state/name, not only a green mark.

Unify alarm and Settings switches through `drawSettingsToggle()` or a common underlying tokenized switch renderer. Preserve knob position as the noncolor state cue; use an appropriately contrasting off track and paired knob ink. The alarm list's current 50×24 switch and Settings' 46×28 switch may remain size variants under the same geometry contract. Do not reduce the whole-row alarm enable/edit target or enlarge it over neighboring rows.

Keep battery percentage and fill amount as noncolor cues, retain the 20/45 thresholds, and use tokenized status fills. Do not add a lightning bolt or “charging” label: that would claim a state the ADC helper does not supply. On category tiles, check glyph contrast independently of page text; white on the existing bright green/orange tiles is not automatically sufficient. A dark tile ink or a darker tile fill is preferable to thickening a low-contrast glyph.

`showCalendarPage()` deliberately calls `drawBackHeader("Calendar", false)`. Preserve the absence of profile/SD icons there. Token consolidation must not add global controls back to every header. Keep explicit Back buttons as alternatives to swipe-back, and make the text/chevron drawable with supported glyphs.

## 6. Weather illustrations: one composed result, with a reliable fallback

### Observations

`drawWeatherFallback()` at lines 378–414 uses a cloud/sun/rain/snow/fog palette independent of theme colors. That is reasonable for illustrations, but its pale cloud and white snow need deliberate contrast on any light surface. It has mostly one-pixel rays/rain/snow strokes. `drawWeatherIcon()` draws the vector fallback first, then overlays an SD PNG.

The installer's `ICON_SIZE = 96` and `icons()` at lines 590–593 scale the longest SVG dimension to 96 pixels and export with `alpha=True`. Normal installed assets therefore have an intended maximum 96-pixel dimension, not an arbitrary full-panel size. `drawPng()` itself checks neither dimensions nor decode success. `lineBuf[480]` only has room for a 480-pixel RGB565 scanline.

`pngDraw()` calls `png.getLineAsRGB565(..., 0xffffffff)` and then `tft.pushImage()` for the entire line. PNGdec documents **0xffffffff as disabling background color mixing**, not as a white background.[6] With no alpha mask in this callback, transparent pixels do not automatically preserve whatever was previously drawn underneath. The comment promising a readable fallback under transparent/corrupt PNGs is therefore stronger than this code establishes. `drawPng()` also returns true after a successful open even if the subsequent decode fails. The exact linked PNGdec revision still needs to be checked before implementation.

### Recommendations

Keep weather code mapping, city choice, Fahrenheit behavior, source cadence, asset path `ICON_DIR`, and every missing-SD fallback. Standardize a named **96 × 96 weather illustration box**, centered by decoded width/height within that box. The current fallback artwork is offset within its nominal box; normalize its optical center rather than merely moving the PNG origin. Use two-pixel minimum detail strokes for weather features that must survive distance viewing, without changing which condition a glyph represents.

Add named illustration roles such as `WEATHER_CLOUD`, `WEATHER_SUN`, `WEATHER_RAIN`, `WEATHER_SNOW`, and `WEATHER_FOG`, separate from action and status roles. Keep the current dark home as their initial canvas. If weather later appears on a light card, choose a dark cloud outline or light-theme artwork variant rather than relying on the current pale cloud against white.

Compose **one** illustration result: attempt the bounded PNG; if it fails, clear the illustration box and draw the vector fallback. For alpha PNGs, either blend against the known solid weather-surface color using the pinned decoder's documented background format, or use an alpha mask to skip transparent pixels. Do not unintentionally display both a vector sun/cloud and a partly transparent PNG variant. Preserve endian behavior until checked; do not change `PNG_RGB565_BIG_ENDIAN` to fix an assumed color-order problem.

After `png.open()`, reject dimensions outside the intended illustration box and buffer bounds; close the file on every outcome. Check `png.decode()` and restore the fallback if a corrupt image partially wrote pixels. This is a rendering-reliability change that preserves the implemented fallback intent, not a new weather-data feature. A bounded 96×96 RGB565 scratch surface would cost 18,432 bytes before overhead; use it only after measuring available heap and considering the existing sprite and decoder allocations.

## 7. Dynamic text and repaint ownership: protect the active page

### Observations

Home already avoids a full-screen redraw every second: `loop()` at lines 1868–1880 checks time every second but paints `drawTimeBlock()` on minute changes; `drawStatusBlock()` runs approximately every 30 seconds; weather fetch uses `WEATHER_REFRESH_MS`; and `loadClassroomCache()` / right-panel repaint run at approximately 60-second intervals. The supplied source therefore does **not** redraw cached assignment text every second as older README/TODO wording suggests. Preserve the actual cadence and distinguish displaying a cache from Google synchronization.

`drawPasswordField()` is already a limited repaint for character entry/backspace, and `tryConnect()` repaints only the connecting Wi-Fi glyph. These are good foundations. `drawWifiPageStatus()` nevertheless fills and redraws the entire 448×38 status row every 180 ms even when connected and unchanged. `drawRightPanel()` clears all 239×320 pixels, including weather artwork, for a cache update. `drawTimeBlock()` clears 240×224 pixels for each minute. `drawUpdatePage()` reconstructs the whole update page on each received progress value.

There are also **active-page ownership defects**. In the `S_SETTINGS` General branch, line 1930 calls `showSettingsPage(); drawStatusBlock();` after saving. The latter clears the lower-left home rectangle over the Settings page. In the Calendar branch, line 1946 calls `showSettingsPage(); drawRightPanel();`, which clears the entire right side of the newly drawn Settings page and paints the home weather/calendar module there. These are concrete visual corruption paths. Correcting repaint ownership does not require changing the settings values, saving behavior, or navigation.

`refreshSdState()` maps any `S_SETTINGS` screen to `showSettingsPage()`, although first-run Classroom/Sync pages also use `S_SETTINGS` plus `firstSetupClassroom` / `firstSetupSync`. A state-change repaint must respect those flags rather than repainting the normal Settings hierarchy during setup.

The active updater is not a smoothly streaming percentage producer: `BL::installCandidate()` calls the progress callback after `Update.writeStream()` returns, then again at success. `updateProgress()` and `drawUpdatePage()` can only represent the events they receive. Neither appearance nor an artificial tween can establish real intermediate installation progress.

### Recommendations

Give each page and dynamic field an **owned rectangle and last-rendered value/state**. Use current fetch/status/alarm/sleep intervals; only eliminate redundant drawing. In the General/Calendar Settings branches, redraw the Settings content alone and mark the home fields dirty for the next `showHome()`. Guard home-only painters by current page context, or separate data invalidation from immediate painting. Preserve each existing toggle, `saveSettings()`, and return path.

For first-run `S_SETTINGS` variants, dispatch SD/state repaints according to the setup flags, not only the enum. This preserves the wizard rather than treating token work as a new setup flow. Reset per-page rendered-state caches on navigation, theme changes, sleep/wake, and full redraw, so identical text is still painted onto a newly cleared page.

Split `drawRightPanel()` internally into weather and assignment regions while retaining it as the complete panel entry point. A cache refresh should repaint assignment text only when title/due/visibility changes, not decode the same weather PNG again. Split `drawStatusBlock()` into icon/value/control ownership so a percentage update cannot erase a neighboring control. For `drawWifiPageStatus()`, repaint the text/background only on state/SSID/theme changes; continue the existing ~180 ms glyph frames while actually connecting. Once connected, keep the icon stable unless RSSI level changes.

For time, retain the current complete clear-and-redraw first, with the existing large digits and AM/PM/date separation. A later digit-only repaint must clear segments that turn off; drawing only new lit segments leaves ghosts. Keep the placeholder `--:--`, variable-width 12-hour formatting, `DD/MM/YYYY` home date, and 12/24-hour behavior. The alarm list/editor currently formats one-time dates month/day/year via `alarmRepeatText()` / `drawAlarmEditor()`, unlike home/manual entry. Treat any date-format normalization as a separately approved consistency fix, not part of token extraction.

Keep `drawUpdatePage()` as the public entry point, but optionally separate a static frame from a bounded status/percentage/progress-track repaint. Use semantic update-surface roles or an explicit fixed-dark update scope; do not leave anonymous local colors. Preserve checking, up-to-date, failure, installed/restart messages, percent constraints, `runUpdate()` timing, and `BL::checkAndInstall()` semantics. Show an indeterminate/checking state only when progress is not known; never fabricate a gradually rising installation percentage or new streaming behavior.

`animateLoader()` / `drawLoaderFrame()` have no active caller in the reviewed flow; `tryConnect()` uses `drawWifiSignal()` instead. Do not prioritize a visual redesign of inactive rings or reconnect them merely to use the existing sprite. `setup()` allocates a 100×100 16-bit sprite, about 20,000 bytes before overhead. A full 480×320 16-bit framebuffer would require **307,200 bytes** before overhead; this review does not assume available contiguous heap or PSRAM. Prefer direct bounded paints, scanline PNG decoding, and small optional buffers over a mandatory full-screen double buffer.

## 8. Acceptance checks for a later implementation

A software review can establish color conversion, glyph bounds, paint-region ownership, and unchanged action/data paths. It cannot establish panel readability. Before a future source change is declared complete, capture host-rendered or instrumented drawing bounds for all five themes and exercise representative strings: `9:59`, `12:59`, `23:59`, `--:--`, AM/PM, `100%`, long SSIDs, repeated wide letters, empty titles, long Classroom titles, a long manual city, and unsupported/UTF-8 input. Check short-after-long replacements, selected/off/pressed/armed states, every Back label, checkmark, and degree glyph. Do not label a host approximation as an ST7796S screenshot.

Verify that no text rectangle overlaps its icon, value, switch, or neighboring row; no draw call extends outside 480×320; and no home helper paints over Settings. Compare the same semantic foreground/background pairs **after** RGB565 conversion, including selection, destructive actions, category glyphs, and disabled explanatory content. Verify all five theme IDs still save/load and that absence or invalidity of SD leaves session behavior intact. Weather cases should include absent icons, an alpha PNG, a partial decode failure, an oversized PNG, and SD removal. These are recommendations for later tests, not tests executed in this review.

On the actual Hosyond device, separately validate color order/inversion, text from near and intended glance distance, bright/dim surroundings, optical icon weight, touch targets at all corners/center, connecting animation, SD state distinctions, Settings-category navigation, factory-reset confirmation, and sleep/wake without a white flash. Preserve `sleepDisplay()`'s black fill, panel-off/sleep commands, and GPIO27 backlight sequence. A black RGB565 canvas on a backlit LCD is not equivalent to OLED pixel-off behavior, and no measured luminance or viewing-angle claim is made here.

## References

[1]: https://support.apple.com/guide/iphone/use-standby-iph878d77632/ios "Apple — Use StandBy to view information at a distance"
[2]: https://developer.apple.com/design/human-interface-guidelines/color "Apple Human Interface Guidelines — Color"
[3]: https://developer.apple.com/design/human-interface-guidelines/accessibility "Apple Human Interface Guidelines — Accessibility"
[4]: https://raw.githubusercontent.com/Bodmer/TFT_eSPI/master/Fonts/GFXFF/FreeSans9pt7b.h "TFT_eSPI upstream — FreeSans9pt7b glyph range and metrics"
[5]: https://raw.githubusercontent.com/Bodmer/TFT_eSPI/master/Fonts/GFXFF/FreeSansBold9pt7b.h "TFT_eSPI upstream — FreeSansBold9pt7b glyph range and metrics"
[6]: https://github.com/bitbank2/PNGdec/wiki "PNGdec public API — RGB565 conversion, alpha masks, and decode results"

## Ordered implementation checklist

1. **Freeze behavior and geometry first.** Record the current `Screen` routes, `loop()` hit rectangles, setup flags, font pointers, theme IDs, state/data limits, refresh intervals, and `config.h` hardware/calibration values. Choose a future versioned source slot according to the repository release policy; do not silently replace the current verified v2.8 source/binary pair.
2. **Fix active-page repaint ownership before aesthetic changes.** Remove home painting from the General/Calendar `S_SETTINGS` completion paths through deferred home invalidation or equivalent guards, and make `refreshSdState()` respect `firstSetupClassroom` / `firstSetupSync`. Keep toggles, saving, and navigation intact.
3. **Introduce compatibility-backed color and metric tokens.** Centralize the existing `applyAppearanceTheme()` values, explicit home/keyboard/update scopes, font-role aliases, spacing, radii, and illustration/status roles. Preserve current Midnight default and saved theme IDs; do not add a theme loader in this step.
4. **Repair state contrast.** Migrate `alarmButton()`, `drawKey()`, `drawKbKey()`, `settingsRow()`, `settingsGroupRow()`, switches, category tiles, warning/status text, and the armed factory-reset action to tested ink/fill pairs. Screen all five themes after RGB565 quantization.
5. **Make essential symbols independent of unsupported font glyphs.** Add original drawn checkmark, back/disclosure chevron, and degree helpers; verify the actual linked font resources. Keep credentials/data unchanged while making display-only text fitting safe for unsupported characters and UTF-8 boundaries.
6. **Standardize bounded text and alignment.** Extend `fitText()` or add rectangle-based text/wrap helpers; replace character-count clipping in scan, keyboard, password, update, and file views. Correct `drawSettingsOption()` vertical centers/trailing budgets and `settingsRow()`'s overlapping width budgets; wrap long help/reset/status text without shrinking body fonts.
7. **Repair status geometry and semantics.** Fit `100%` and a `drawSettingsGear()` control within the existing home status region; give `drawWifiPageStatus()` separate text/icon rectangles; render ready/absent/not-ready SD states consistently with an explicit background. Preserve battery thresholds, Wi-Fi state meaning, and all existing hit targets.
8. **Harmonize grouped surfaces, dividers, and utility icons.** Parameterize `drawSettingsCard()` separator insets, converge ordinary radii/strokes, align Calendar columns, and quiet home structural lines. Keep the full-width Settings root, x=240 home split, 228-pixel maximum time width, and Calendar's intentionally nonglobal header.
9. **Make weather composition robust.** Keep `ICON_DIR` and 96-pixel installed-asset intent; validate dimensions/decode outcomes, implement documented alpha handling, center by actual asset bounds, and restore `drawWeatherFallback()` on any failure. Check memory before allocating a weather scratch surface.
10. **Reduce redundant dynamic paints without changing cadence.** Split weather/assignment/status ownership, cache rendered state, animate only connecting Wi-Fi frames, and optionally separate Update's static/dynamic fields. Do not invent intermediate OTA progress or rewrite the updater transport.
11. **Build and validate the future implementation honestly.** Compile with the matching ESP32/TFT_eSPI/PNGdec/ArduinoJson toolchain, exercise software bounds/state cases, and verify persisted compatibility. Publish binaries/manifests or mark TODOs complete only in that separately authorized implementation task after actual validation.
12. **Perform and record physical validation separately.** Test ST7796S color/readability, orientation/calibration, touch, SD, alarms, Settings, OTA outcomes, and GPIO27 sleep/wake on the target device. Keep any unperformed hardware or native-Windows checks explicitly open; this review establishes none of them.
