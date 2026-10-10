# ClockOSv2.8 — Touch, navigation, and direct-manipulation review

**Review ID:** `2026-10-10-04`
**Focus:** Touch interaction, navigation, state transitions, swipe-back, modal flows, controls, and display sleep/wake.
**Method:** Static inspection of the current firmware and supplied project documentation. **No physical hardware validation, build, or runtime interaction testing was performed for this review.**

## 1. Scope and evidence

Read in full:

- `.agent/todo.md`
- `.agent/agent.md`
- `README.md`
- `.source/uncompiled/updates/ClockOSv2.8/ClockOSv2.8.ino` (2,085 lines)
- `.source/uncompiled/updates/ClockOSv2.8/config.h`

Also inspected the adjacent `bootloader.h` **only** to establish the update modal's blocking/progress behavior and the point at which installation owns input. No installer, release, historical firmware, binary, manifest, or test changes are proposed or performed here. Firmware line references below refer to this inspected v2.8 sketch; configuration/helper references explicitly name their file.

The supplied TODO and README leave physical touch mapping, orientation, Settings touch response, and backlight/OTA checks open. Older checked TODO items do not establish that the interactions described here were physically validated. The report treats the code as implementation evidence and separates that evidence from proposed interaction behavior.

> **Design objective:** An Apple-like interaction model means that the object under the finger responds immediately, a contact has one owner, navigation is reversible, and visible controls match their actionable area. This is a design proposal for this 480 × 320 device, not a claim of Apple guideline compliance or a conversion between display pixels and Apple points.

All coordinates below are **logical screen pixels after** `TFT_ROTATION` and `tft.setTouch(calData)`. Retain `TFT_W = 480`, `TFT_H = 320`, `TFT_ROTATION = 3`, and `TOUCH_CAL_X0/X1/Y0/Y1/ROTATION = {343, 3436, 266, 3381, 1}`. Do not change orientation, calibration, or the existing pressure threshold in response to an interaction defect without separate physical evidence.

## 2. Behavior that must remain available

These are preservation constraints, not suggestions to remove features:

| Existing capability | Source anchor | Preservation requirement |
| --- | --- | --- |
| First-run Wi-Fi or offline setup, Classroom choice, Off/Main/Side sync choice | `showWelcome()`, `showClassroomSetup()`, `showSyncSetup()`, `finishFirstSetup()`, `setup()` | Keep both setup paths, setup-resume handling, and `setupComplete` saving before automatic OTA. Do not introduce normal Settings Offline Mode. |
| Wi-Fi selection, rescan, password entry, case/symbol layers, Show/Hide, saved-network attempts | `showScan()`, `openWifiScanner()`, `showKeyboard()`, `tryConnect()`, `trySavedNetworks()` | Keep five scan results/saved-network capacity and existing network functions; fix contact ownership and return context rather than replacing the flow. |
| Time format and battery-percentage settings; Calendar/Classroom/Notifications toggles | `showSettingsPage()`, `drawSettingsToggle()`, `settings.use24Hour`, `settings.showBatteryPercent`, `settings.calendarEnabled`, `settings.classroomEnabled`, `settings.notifications` | Keep full-row toggle convenience and immediate session effects. |
| Crystal, Midnight, Ocean, Sunrise, Graphite themes | `applyAppearanceTheme()`, Settings category `2`, `showAppearancePage()` | Keep all five selections and SD-backed persistence. |
| Automatic/default weather and manual US city entry | `showWeatherPage()`, `showCityKeyboard()`, `geocodeManualCity()`, `fetchWeather()` | Keep both location modes and Philadelphia fallback; do not silently represent an unresolved city as a verified location. |
| Up to four enabled/disabled one-time or repeat-day alarms | `MAX_ALARMS`, `drawAlarmList()`, `beginAlarmEdit()`, `drawAlarmEditor()`, `alarmDueNow()` | Keep `MAX_ALARMS = 4`, existing draft/save/cancel semantics, time/date wrapping, and repeat masks. Do not add deletion, snooze, audio, or new alarm scheduling semantics as part of this interaction repair. |
| Manual offline date/time entry after restart | `showClockEditor()`, `drawClockEditor()`, `applyClockDraft()`, `setup()` | Preserve Save applying time and Cancel returning Home. Do not turn the Home format shortcut into a different action without a separate product decision. |
| Non-erasing SD preparation and read-only file display | `prepareSdCard()`, `showSdCardPage()`, `showSdFilesPage()` | Keep preparation available without Wi-Fi and preserve unrelated files. Do not introduce formatting or a destructive storage action. |
| Developer entry and informational recovery/rollback controls | Version-tap branch in `loop()`, `showDeveloperPinPage()`, `showDeveloperModePage()` | Keep the five-tap entry, existing PIN comparison, and informational/no-change outcomes. Do not claim recovery images are installed or implement rollback in this UI repair. |
| Two-tap Factory Reset | `showFactoryResetPage()`, `factoryResetClock()` | Keep two **distinct deliberate taps**, existing deletion scope, restart behavior, and preservation of firmware/unrelated SD files. |
| Automatic update flow with safe failure return | `runUpdate()`, `updateProgress()`, `BL::checkAndInstall()` | Preserve automatic checks, no reboot on failure, and reboot only after successful installation. Do not add an unimplemented cancellation path while writing firmware. |
| Home-only display sleep after a minute, touch wake to Home, explicit GPIO27 backlight control | `loop()`, `sleepDisplay()`, `wakeDisplay()`, `setDisplayBacklight()`, `TFT_BACKLIGHT_PIN` | Keep the one-minute Home policy and wake-to-Home behavior. This review does not propose sleeping editors or restoring arbitrary screens on wake. |
| Session-only changes without ready SD storage; warning when leaving Settings | `saveSettings()`, `saveAlarm()`, `saveCreds()`, `leaveSettingsPage()`, `showSdSaveWarning()` | Continue operating without SD and continue explaining that changes are temporary. |

## 3. Highest-priority findings

**Priority definitions:** P0 = unintended destructive action possible from ordinary contact handling; P1 = stranded, misleading, corrupted, or missed core flow; P2 = inconsistent/discoverability or unnecessary-redraw issue. These are static-risk classifications, not observed device incident rates.

| Priority | Observation grounded in source | User-facing risk | Recommended repair |
| --- | --- | --- | --- |
| P0 | `readTouch()` accepts a held contact again after `> 220` ms; Factory Reset arms and executes on successive accepted readings in the same y band (`190–208`, `2076–2082`). | One long press can count as both reset taps. | Dispatch reset on debounced **release**, require a new contact ID after arming, bound the button in x and y, and expire the armed state. |
| P1 | Initial touch samples execute actions before swipe intent is known; the same contact can continue on a new screen. | Starting a swipe can first open a row, toggle a setting, select a network, or press a key on the destination. | Capture a target on Down, cancel it when a gesture claims the contact, commit ordinary actions on Up, and suppress contact-through-navigation. |
| P1 | `loop()` draws Home regions **after** `showSettingsPage()` when General or Calendar toggles change (`1930`, `1946`). | `drawStatusBlock()` overwrites the lower Settings area; `drawRightPanel()` overwrites its right half while `screen` remains `S_SETTINGS`. Visible content and touch dispatch disagree. | Invalidate the corresponding Home region for the next Home render; never draw Home content into Settings. |
| P1 | `refreshSdState()` redraws `S_SETTINGS` with `showSettingsPage()` regardless of `firstSetupClassroom`/`firstSetupSync` (`1093–1111`). | SD-state changes can replace wizard visuals with normal Settings while wizard hit zones remain active. The first successful periodic card check can also count as a change. | Route-aware invalidation must redraw the active wizard step, not just its shared `Screen` enum. |
| P1 | Developer keypad calls `showDeveloperPinPage()` after each input, and that function clears `developerPinInput` (`1392–1401`, `1988–2001`). | PIN cannot accumulate through the displayed keypad; literal `****` conceals this defect. | Separate PIN-session initialization from rendering; render actual masked length and retain the draft between keys. |
| P1 | Developer recovery/rollback controls call `drawUpdatePage()`, setting `S_UPDATE`; there is no `S_UPDATE` input branch (`2002–2009`). | Informational actions leave a permanent noninteractive page; Home-only sleep does not provide an escape. | Show a dismissible informational modal returning to Developer Mode. Reserve the blocking update mode for actual installation. |
| P1 | Sleep is checked before `alarmRinging` and `alarmDueNow()`; the sleeping branch returns before alarm evaluation (`1778–1785`, `1853–1867`). | Sleeping prevents alarm evaluation; an already displayed alarm can be put to sleep after Home inactivity. Alarms are also not evaluated on other screens. | Run alarm/time service independently of display/navigation, give ringing modal priority, inhibit sleep while ringing, and wake/render the alarm when due. |
| P1 | Wi-Fi Enter always opens Classroom setup on successful connection (`1836–1843`), including a scanner opened from configured Settings/Wi-Fi. | Changing networks unexpectedly replays first-run Classroom/sync choices. Offline/disabled flags are not normalized by the normal connection-success path. | Distinguish onboarding from network maintenance and return to the recorded caller after maintenance. Define successful user-requested connection as explicit online intent. |
| P1 | City editing reuses `selSsid` and `password`, persists `manualWeather = true` before editing, and accepts geocode failure because `settings.manualWeather` is already true (`951–956`, `1829–1835`, `2026`). | Cancel changes mode; failed entry can pair a new name with old coordinates; later `prepareSdCard()` calls `saveCreds()` with city-edit values. | Give city/password editors separate draft state; commit location atomically, and preserve an explicit unverified/offline-entry option instead of silently claiming success. |
| P2 | Several hit tests cover the full width or include gaps; date/settings/battery hits do not follow the visible Home objects. | Blank margins and labels trigger unexpected actions, while the displayed date can open Alarms. | Use shared render/hit geometry with half-open rectangles and intentional, nonoverlapping expansion. |
| P2 | Back behavior is spread across state branches; Settings root swipe only redraws the root, Storage returns to a remembered category, and Settings entry from Home retains prior root/detail state. | Back does not reliably mean parent; fresh Settings entry can start on an old detail page. | Centralize parent navigation using a small route context that includes Settings and editor substates. |
| P2 | `tryConnect()`, `scanNetworks()`, HTTP operations, warning delays, and update calls block input; Wi-Fi status redraws every 180 ms even when unchanged. | Feedback is delayed or misleading, and unnecessary redraw makes pages unstable. | Use explicit busy phases, bounded operations, cooperative service where feasible, and changed-field rendering. |

## 4. Touch contract: one physical contact, one interaction

### 4.1 Current sampling is rate limiting, not debouncing

**Observation — `readTouch()` (`190–208`):**

- A pressure-valid reading is returned when `millis() - last > 220`; a stationary finger therefore creates repeat actions at roughly four accepted readings per second, subject to loop delays.
- No Down/Move/Up event is exposed. Callers cannot distinguish a new tap from a hold.
- During throttling, the function calls `tft.getTouch()` again to detect release. The two reads can disagree; a single absent/noisy second result resets the swipe origin.
- `startX/startY/startAt` live across screens and are reset only on an absent reading, a detected swipe, or the next accepted sample after 900 ms.
- `lastActivity` updates only on accepted readings, not every valid contact sample.
- The first sample can mutate state before later samples satisfy `tx > startX + 90`, `abs(ty - startY) < 70`, and `< 900` ms.

**Recommendation:** Retain pressure `350` initially, but replace the event semantics around `readTouch()`. Poll once per sample period, validate coordinates, stabilize contact transitions, and emit typed events. The following names are **proposed constants**, not existing firmware constants or measured hardware values:

| Proposed constant | Initial value | Exact role |
| --- | --- | --- |
| `TOUCH_PRESSURE_MIN` | `350` | Preserve the threshold already passed to `tft.getTouch()`. Revisit only after physical pressure/noise measurements. |
| `TOUCH_SAMPLE_MS` | `16` ms | One touch read per due sample; do not perform a second read in the same decision. |
| `TOUCH_DOWN_STABLE_MS` | `30` ms | Require continuous valid contact for at least this duration before Down; at a 16 ms cadence this generally means three valid samples. |
| `TOUCH_UP_STABLE_MS` | `35` ms | Require continuous absence before Up; a brief missing sample must not create a second contact. |
| `TAP_SLOP_PX` | `12` px | Maximum displacement for an ordinary tap; larger unclaimed movement cancels the tap. |
| `TARGET_RETENTION_PX` | `8` px | Small retention halo around the captured target; never extend ownership into a different target. |
| `BACK_EDGE_PX` | `24` px | Left-edge band for swipe-back initiation, where a parent exists. |
| `BACK_CLAIM_PX` | `16` px | Claim a rightward gesture after this movement and horizontal intent checks. |
| `BACK_VERTICAL_CLAIM_PX` | `24` px | Maximum vertical displacement at gesture claim; require `dx > 1.5 × abs(dy)`. |
| `BACK_COMMIT_PX` | `90` px | Retain the existing swipe distance as the initial release threshold. No fling shortcut in the first repair. |
| `BACK_VERTICAL_LIMIT_PX` | `44` px | Cancel an already claimed back gesture if vertical deviation exceeds this bound. |
| `BACK_MAX_MS` | `900` ms | Preserve the existing upper gesture-duration bound. |
| `REPEAT_DELAY_MS` | `400` ms | Earliest repeat for explicitly repeat-enabled steppers/backspace, never ordinary controls. |
| `REPEAT_INTERVAL_MS` | `120` ms | Subsequent step/backspace repeats while the same target retains ownership. |
| `RESET_ARM_MS` | `5000` ms | Suggested confirmation expiry for the second, distinct reset tap. |
| `UI_FEEDBACK_TARGET_MS` | `50` ms | Design target for visible feedback after a stable Down, not a measured guarantee. |

Use `uint32_t(now - timestamp)` comparisons so timings remain valid across `millis()` wraparound. Reject coordinates outside `[0, TFT_W)` and `[0, TFT_H)` before hit testing. A median of the last three valid positions is an optional small filter; verify its effect on latency physically rather than stacking filters blindly over TFT_eSPI's existing behavior.

### 4.2 Event and ownership rules

The proposed event states are `Idle → StabilizingDown → Tracking → StabilizingUp → Idle`, with an owner of `TapTarget`, `BackGesture`, `StepperRepeat`, `SwitchDrag`, `WakeContact`, or `BlockedOperation`.

1. **Down:** Allocate a monotonic contact ID and capture the active route generation plus the one highest-priority target under the finger. Draw that target's pressed state. Do not execute navigation, save, reset, Enter, row toggle, theme selection, or a character yet.
2. **Move:** Update only the captured target. A child target, such as an alarm switch, cannot also activate its row. If an edge-back gesture claims the contact, cancel the target's press and draft action. If a non-gesture move exceeds tap slop, cancel the ordinary tap.
3. **Up:** Execute one action only if the original target is still eligible, the route generation matches, and the contact was not cancelled/claimed. Blank areas have no action.
4. **Transition:** Increment a route generation and clear press/gesture state. Any contact held across a programmatic screen change, modal opening, or wake is ignored until stable release. Do not apply its coordinates to the destination screen.
5. **Navigation:** One contact may perform at most one parent transition. It must not pop multiple pages by continued hold/movement.
6. **Busy interval:** Ignore rather than queue arbitrary stale taps through an operation that cannot poll input. When it completes, wait for stable release before accepting a new contact. A cooperative busy UI may accept only its explicitly supported actions.
7. **Repeat:** Opt in only hour/minute/date steppers and keyboard backspace. A short tap takes one step on Up. A held control begins stepping at 400 ms, then every 120 ms; release adds no extra step once repetition has begun. Leaving the retention region stops repeat. Draft changes are not persisted until the existing Save action. No repeat for Reset, Save, Cancel, Enter, navigation, switches, theme/radio choices, version taps, or letter keys.

`drawAnyKey(..., true); delay(60); drawAnyKey(..., false)` (`1817`) should become a contact-driven pressed state, not a timed flash that blocks the next sample. Fast typing then depends on successive released taps, not a global 220 ms cooldown. Redrawing a key should not rebuild the keyboard or clear the text draft.

## 5. Precise hit-box model

### 5.1 Common geometry rules

**Recommendation:** Introduce a small rectangle/control descriptor used by both rendering and dispatch. A target contains an ID, bounds, enabled state, action, and repeat policy. This is a proposed helper, not an existing function.

- Use half-open bounds: `x >= left && x < right && y >= top && y < bottom`.
- Define rectangles before doing row/column division. Negative division in C++ truncates toward zero and is not a valid bounds check.
- Aim for **44 × 44 logical pixels** for ordinary controls on this device. Expand visual glyphs within a partition, not into neighbors. Dense keyboard characters are an explicit compact-layout exception, not a reason to use full-width y bands elsewhere.
- A read-only status/count row has no action, press state, or disclosure marker. An actionable row highlights as one unit; child controls get explicit priority and exclusive ownership.
- Small rounding/visual gutters may lie inside a target only when that expansion is deliberate, shared with rendering, and cannot trigger a different target.

### 5.2 Home: make every visible object tell the truth

**Observation — `drawTimeBlock()`, `drawStatusBlock()`, `drawRightPanel()`, Home branch (`997–1014`, `1077–1091`, `1114–1130`, `1858–1865`):**

- The displayed date is centered at `(120, 180)`, but the format-toggle test starts at `y >= 190`; tapping the date itself can open Alarms.
- Battery at `(102, 286)` and part of its percentage are in the format-toggle fallback, not Settings. The README describes a battery/settings preference area, while code gives this battery area a different action.
- Weather/Calendar use right-half bands with a touch split at `168`, while the drawn separator is at `160`.
- Status icon targets are asymmetric, and the Settings pill is only `58 × 34`. Large blank areas below the date toggle the format.

**Recommended Home partitions, preserving the current actions:**

| Target | Proposed logical hit rectangle `[left,right) × [top,bottom)` | Existing action |
| --- | --- | --- |
| Time / alarms | `[0,240) × [0,156)` | `showAlarmEditor()` → alarm list; add a compact alarm cue/hint rather than changing the action. |
| Date / time-format shortcut | `[16,224) × [158,204)` | Toggle `settings.use24Hour`, `saveSettings()`, invalidate time block. Add a small format cue or first-use hint. |
| Weather module | `[241,480) × [0,160)` | `showWeatherPage()` |
| Calendar module | `[241,480) × [164,320)` | `showCalendarPage()` |
| Wi-Fi | `[4,48) × [268,312)` | `showWifiPage()` |
| SD status | `[48,92) × [268,312)` | `showSdCardPage()` |
| Battery/percentage | `[96,166) × [268,312)` | Open Settings General, matching the visible preference association; the format shortcut remains available on the date and in General. |
| Settings pill | `[166,236) × [268,312)` | Fresh Settings root entry. |

The battery association is a **recommended correction**, not a description of current dispatch. Keep the date format toggle available; do not silently replace it with `showClockEditor()`. Make `[0,240) × [204,268)` and gutters inert unless a visible control is deliberately added. Separator pixels should not act as invisible navigation. Expand or simplify the Settings pill so its label and target are legible within the same partition.

### 5.3 Settings and common Back

- **Common Back:** Normalize to `[8,100) × [0,44)` and render a corresponding button/label with `drawBackHeader()` / `drawAppleButton()`. Replace scattered `x < 100 && y < 45` tests with this same rectangle.
- **Settings root:** Existing rows are `42` high at `y = 52 + i × 42`. For a 44 px target, a workable proposal is `[12,468) × [48 + i×44, 92 + i×44)` for six rows, ending at `312`. Reflow the card/icons/text together; do not extend the old targets into neighboring rows.
- **Settings details:** Retain `[12,468)` and the existing `50` px rows starting at `56`. Only the actual drawn row count is eligible. General and Calendar have three rows; Wi-Fi and Storage have three rows with read-only rows explicitly inert; System has its version target `[12,468) × [56,106)` and reset disclosure `[12,468) × [122,172)`.
- **Appearance details:** Retain five `46` px rows at `56 + i×46`, ending at `286`. Selecting the already selected theme is a no-op rather than a write/redraw.
- **Settings toggles:** Existing `drawSettingsToggle(408, y + 9, ...)` draws a `46 × 28` track. Keep full-row tap convenience. If thumb drag is implemented, it owns the same action exclusively; it cannot also toggle the row on Up. The track remains visually small but the row supplies its touch-sized area.

**Observation:** `drawBackHeader()` paints profile and SD icons at `(405,24)` and `(438,24)`, but corresponding touch handlers are absent. `showProfilePage()` has no normal caller in this sketch; the older `S_APPEARANCE` page also has no normal entry call. The active Appearance flow is the Settings category, not that legacy page. Do not assume those icons or pages already work as navigation.

**Recommendation:** First decide and encode whether the header glyphs are status or navigation. The minimal behavior-preserving repair is to style them as noninteractive indicators. If exposing the existing Profile/Storage pages is explicitly desired, use disjoint 44 px targets (for example, profile `[380,424) × [0,44)` and storage `[428,472) × [0,44)`), move the glyphs to their target centers, and preserve the caller as the Back parent. Do not wire an unsupported on-device OAuth action. Keep Calendar's existing `drawBackHeader("Calendar", false)` free of global icons.

### 5.4 Other controls and margins

| Area | Current observation | Concrete recommendation |
| --- | --- | --- |
| Welcome | Any `y >= 190` activates the left/right setup choice, including explanatory copy and bottom margins. Buttons are actually `(36,222,196,44)` and `(248,222,196,44)`. | Hit only `[36,232) × [222,266)` and `[248,444) × [222,266)`. Highlight the captured choice, then transition on Up. |
| Classroom setup | Visible choices are 30 px high at `(28,144,190)` / `(262,144,190)`, but hits cover `y=135..204` and entire left/right halves. | Expand each visual choice to a 44 px target, e.g. `[28,218) × [138,182)` and `[262,452) × [138,182)`; outside areas are inert. |
| Sync setup | Visible buttons are at x `20/170/320`, y `132`, width `140`, height `30`; current y band extends to `205`. | Use three disjoint `[20,160)`, `[170,310)`, `[320,460)` targets at `[126,170)`. Keep Off/Main/Side actions unchanged. |
| Scanner | `(y - 42)/46` accepts each row's visual gutter and any x; Rescan accepts any x at `y >= 278`. | Share result rectangles with `showScan()`. Existing row pitch 46 can support hits `[10,470) × [40+i×46,84+i×46)`. Use Back `[8,100) × [0,40)` if preserving the scan header layout, and a visible 44 px footer such as Rescan `[10,470) × [276,320)`. Bounds before index. |
| Keyboard | Character keys use explicit bounds, but inclusive right/bottom comparisons differ from the rest; normal keys are 36 px high. | Keep compact 36 px character caps and 3/4 px gutters; assign each gutter to no more than one key, or leave inert. Use half-open bounds. Expand Back and Show/Hide within their available header partitions; do not change `Key::c` / `Key::t` semantics. |
| Weather, Wi-Fi, Storage pages | Action rows check y only; touches in left/right margins work. | For the 38 px rows drawn by `settingsGroupRow()`, use `[16,464) × [y-3,y+41)` at the existing 46 px pitch. Share those bounds with drawing and keep informational rows inert. |
| Alarm list | Any x in an alarm's y band edits it; `x >= 388` toggles it even outside the card. Footer actions accept all y below `264`. | At `y0=68+i×46`, hit `[16,464) × [y0-2,y0+42)`. Body `[16,388)` edits; switch region `[388,464)` toggles. Footer Add `[16,236) × [260,304)`, Done `[244,464) × [260,304)`. No invisible Add target at `MAX_ALARMS`. |
| Alarm/clock steppers | Hour/minute and date strips divide the whole screen, accepting visual gaps and edge margins. | Register each drawn button separately with 44 px vertical expansion; preserve its x span. Alarm time can use y `[86,130)`, date `[144,188)`. Clock time can use `[76,120)`, date `[142,186)`. Blank center time area does not redraw or step. |
| Alarm repeat days | `(x-68)/58` admits some x values left of the first day because negative integer division truncates toward zero; label/gaps can select Sunday. | Check each of seven day rectangles individually. To separate 44 px vertical targets from date controls, move the repeat buttons from y `188` to y `196`, with hit y `[190,234)` and each existing 50 px x span. Keep `repeatMask` bits unchanged. |
| Alarm editor footer | x-only divisions in y `[244,286)` include edges/gaps; Save can fire just before its drawn x start. | Enabled `[12,157)`, Save `[167,312)`, Cancel `[322,467)`, all at y `[240,286)`. Keep drafts and persistence unchanged. |
| Clock editor footer | Any `x >= 240` in the action strip cancels, far beyond the button. | Save `[140,240) × [238,282)`, Cancel `[245,355) × [238,282)`; render to the matching area. |
| Developer PIN | `(x-45)/145` and `(y-112)/42` allow leading margins and some space above the first row. | Register the twelve actual keys. Reflow to four 44 px target rows, e.g. `[106+row×46,150+row×46)` with x spans matching the keys and no row overlap. |
| Factory Reset | Any x in y `[246,290)` arms/resets. | Only `[76,404) × [244,290)` is eligible; change the visual height to match. No action outside that button. |
| Ringing alarm | The visible Dismiss button is not the actual boundary: any accepted Home touch calls `dismissAlarm()`. | Preserve tap-anywhere dismissal as an explicit policy and add “Tap anywhere to dismiss.” Keep the button as the primary cue; require a released tap, not a hold or swipe. |

The stepper/repeat suggestions require a small reflow in `drawAlarmEditor()`; do not just expand the current date/repeat rectangles until they overlap. Header and keypad examples likewise require rendering and hit geometry to move together. Validate all final rectangles against `TFT_W/TFT_H` and neighbors.

## 6. Navigation and swipe-back

### 6.1 Current return paths are not a single hierarchy

**Observations:**

- `showSettingsPage()` changes only `screen`; it does not normalize `settingsAtRoot` or `settingsCategory`. Home's Settings action can therefore reopen a prior detail.
- Settings swipe (`1913`) always sets `settingsAtRoot = true` and redraws Settings, even when already at the root. Button Back at the root calls `leaveSettingsPage()`, so root swipe and button Back differ and only one executes the SD-save warning.
- `S_SDCARD` Back forces `settingsAtRoot = false` and shows whatever category was last remembered. A Storage page opened from the Home SD icon can return to a stale or unrelated detail.
- `S_SDFILES` always returns to Storage, even if opened directly by Settings > Storage > Browse Files.
- General > Appearance changes `settingsCategory` in-place; no context remembers General as the previous detail.
- Scanner Back preserves some caller context through `scanReturnScreen`; password Back calls `showScan()` and starts a fresh scan rather than simply returning to the already obtained list.
- Swipe is not handled in `S_SCAN`, `S_KEYS`, or `S_CLOCK`. Wizard branches run before Settings swipe handling. This is not an all-screen gesture system.

### 6.2 Recommended route context

Keep the existing `Screen` enum and screen-rendering functions. Add a lightweight **proposed** route context with: `Screen`, Settings root/detail and category, alarm list/editor state, setup step, editor purpose, and parent route. A fixed stack of roughly six contexts is sufficient for the current hierarchy; guard overflow rather than allocating an unbounded navigation stack. The exact capacity is an implementation decision to confirm against all reachable paths.

Use a single **proposed** `requestBack()` policy called by the visible Back button and swipe completion. Keep existing page functions as renderers, not independent navigational authorities.

| Current route / entry | Recommended Back destination |
| --- | --- |
| Home → Settings | Fresh Settings root on entry; root Back → `leaveSettingsPage()` → Home. |
| Settings root → category | Root; preserve the category's immediate changes. |
| General → Appearance detail | General; Appearance entered directly from root → root. |
| Home → Wi-Fi / Weather / Calendar / Storage | Home. |
| Settings Storage detail → Storage page | Settings Storage detail. |
| Storage page → files | The Storage page. |
| Settings Storage detail → files directly | Settings Storage detail. |
| Scanner opened by Welcome / Wi-Fi / Settings | Recorded caller, retaining the appropriate Settings substate. |
| Password editor | Existing scan results without forced rescan; explicit Rescan still performs scanning. |
| City editor | Weather, without committing a cancelled draft. |
| Alarm list | Home, as currently implemented. |
| Alarm editor | Alarm list; Cancel/Back discards the draft, Save commits once then returns to list. |
| Clock editor | Home on Cancel; Save applies `clockDraft` once then Home. Preserve its offline-startup purpose. |
| System → Developer PIN / Factory Reset | System detail. Clear PIN session or reset arm when leaving. |
| Developer Mode informational action | Developer Mode on acknowledgement; Back from Developer Mode returns through its recorded parent. |
| Profile or older standalone Appearance page, if intentionally exposed | Its recorded caller, not an unconditional Home/Settings jump. |

Do not silently save an alarm/clock/city draft on Back. Ordinary settings continue to apply/save immediately. A later dirty-draft confirmation may be a separate product decision; it is not required to retain today's cancel behavior.

### 6.3 Direct-manipulation back gesture

**Recommendation:** Back is an edge gesture, not any rightward movement anywhere on a page.

1. Only start in x `[0,24)` on a route with a parent and no exclusive modal/installation owner. Visible Back remains available for users who cannot perform the gesture.
2. Track samples at the input cadence, not the old 220 ms action cadence. Once the horizontal claim checks in Section 4 pass, cancel the captured tap. No setting, key, or row may already have committed.
3. Show progressive feedback tied to `dx`: ideally translate the active page right and reveal the parent, without entering that parent or running its side effects yet. A cancelled gesture restores the same route/draft.
4. Commit exactly one `requestBack()` on stable release when `dx >= 90`, duration is under 900 ms, and vertical deviation stays within 44 px. Otherwise cancel. Do not add a velocity shortcut before the basic path is validated.
5. Complete/cancel with a short nonblocking animation, initially 120–180 ms at about 30 frames/s. Input during settling cannot activate the destination.
6. A parent preview is a renderer of the saved route, not a call to `showSettingsPage()` or `showHome()` that mutates navigation. SD warnings run only after back commits.

A full RGB565 480 × 320 sprite needs 307,200 bytes before other allocations. The current `spr` is only 100 × 100. Do not assume this board can hold two full-screen snapshots. Measure available memory and SPI timing before choosing full-page animation. If full translation is too expensive, use a smaller cached reveal strip or an edge progress affordance coupled to `dx`; keep the exact cancellation/commit contract. Animation is secondary to correct ownership and reversible state.

## 7. Editor, setup, and modal state

### 7.1 Separate initialization from rendering

**Recommendation:** Existing `show…()` functions currently mix route entry, state initialization, and drawing. Introduce explicit entry/initialization calls and redraw calls while retaining the existing named functions where practical.

- `showDeveloperPinPage()`: clear input only at new PIN-session entry, not on each key. A redraw shows `developerPinInput.length()` mask characters and a short error after an incorrect Enter. Keep the present `"0000"` comparison in this scoped repair; security policy is outside this review.
- `showKeyboard()`: should rebuild key geometry only on entry/layer/layout change. `drawPasswordField()` updates content, Show/Hide updates its key and field, and a key press redraws just that key.
- `showClassroomSetup()` / `showSyncSetup()`: use an explicit wizard substate rather than relying solely on `S_SETTINGS` plus independent flags. A storage refresh redraws that step, never normal Settings. Retain the no-OTA-mid-wizard policy.
- `showSettingsPage()` and `drawAlarmEditor()`: redrawing the same route must not create a navigation transition or discard a draft.
- `showHome()` is the committed Home renderer; redraw helpers must not be used as substitutes for route changes.

**Observation — version taps:** The current count increments on accepted held samples and resets after a gap over 2,200 ms (`1954–1958`), not necessarily five released taps within one overall 2,200 ms window. Keep hidden five-tap entry but require distinct release-completed taps on the version row. If retaining the rolling gap policy, document it; if changing to an overall 2,200 ms window, make that a named, explicit choice rather than accidental timing behavior.

### 7.2 Wi-Fi success must know why the editor was opened

Add editor purpose/return context when `openWifiScanner()` opens a flow:

- **Onboarding success:** Continue to `showClassroomSetup()` then `showSyncSetup()`. Save `setupComplete` after the final selection; do not move automatic OTA before that commit.
- **Configured-device network maintenance:** Save successful credentials, configure time, refresh weather as today, and return to the caller's Wi-Fi/Settings route. Do not replay Classroom/sync setup or overwrite those choices.
- **Explicit online transition after offline setup:** A successful user-requested connection should set a documented coherent state (`wifiEnabled = true`, `offline = false`) before weather/time work. Preserve offline onboarding itself. Do not silently normalize these flags merely because a background scan started.
- **Failure:** Stay in the password editor with its draft and visible error; provide Retry and Back through existing controls.

`tryConnect(15000)` blocks for up to 15 seconds per network attempt; `trySavedNetworks(15000)` can attempt five networks during startup. Do not present an apparently actionable Back control while the routine cannot service it. First make the busy state honest; then split connect/scan into start/poll/finish phases if supported by the selected ESP32 library. Retain saved-network order and timeouts unless intentionally changed.

### 7.3 City drafts cannot be Wi-Fi credentials

**Observation:** `showCityKeyboard()` assigns `password = manualCity` and `selSsid = "Manual city"`. `prepareSdCard()` subsequently calls `saveCreds()`, whose source of values is that same pair. The risk is caused by shared interaction state even though persistence implementation is otherwise out of scope.

**Recommendation:** Keep the keyboard renderer reusable, but give it an editor-purpose enum and independent text draft. City entry must not mutate `selSsid`, Wi-Fi `password`, or committed `manualCity/manualLat/manualLon` before validation/commit. Preserve the prior `settings.manualWeather` on Cancel.

For Enter, validate trimmed text, show a bounded lookup busy state, and commit name/coordinates/mode together on successful geocoding. Preserve offline/manual-entry utility with an explicit “location not verified; using previous coordinates” or pending-location outcome, not the current automatic success caused by `|| settings.manualWeather`. Retry must retain the draft. Do not claim successful geocoding when `geocodeManualCity()` returned false.

### 7.4 Alarm and clock controls

- Preserve `beginAlarmEdit()` copying the saved alarm into `alarmDraft`, and Save writing it once. Cancel/Back discards the draft. List switch changes remain immediate.
- `alarmRepeatText()` and `drawAlarmEditor()` currently print **MM/DD/YYYY**, while Home and `drawClockEditor()` print **DD/MM/YYYY**. Normalize displayed alarm dates to DD/MM/YYYY; retain the actual `year/month/day` fields and scheduling logic. This is a label-order correction, not a data migration.
- Distinguish duplicate day initials (`S`, `T`) with short unambiguous labels or a legend, preserving Sunday-through-Saturday bit order. Seven 50 px controls can accommodate compact `Su/Mo/Tu/We/Th/Fr/Sa` labels.
- Time/date stepping should redraw the changed value plus any date clamped by `adjustAlarmDate()` / `adjustClockDate()`. Keep their wrap/clamp semantics and 2024–2099 year range.
- No hidden Delete action should be introduced; the implemented maximum-four list remains intact.

### 7.5 Factory Reset remains two deliberate taps

Use a small reset-confirmation modal state, not just an unrestricted persistent boolean:

1. The first released tap inside the reset target sets `factoryResetArmed`, records its contact ID, and starts a 5-second suggested arm timer.
2. Redraw only the button/confirmation copy. The second action requires a later contact ID, a stable release, the same target, and an unexpired timer.
3. Back, route change, expired timer, or explicit Cancel disarms. A tap outside may disarm without navigating; decide and encode this consistently.
4. Only then call the existing `factoryResetClock()` once. Block further input while it deletes/restarts.
5. Keep the displayed scope tied to ClockOS local data. Do not convert this into SD erase/format. When `sdOk` is false, explain that the action still restarts the device but no ready SD data is available to erase; do not remove the existing restart path.

A confirmation timeout alone does **not** solve the current hold-to-reset defect. Distinct contact/release gating is mandatory.

### 7.6 Other modal flows and honest busy feedback

- `showSdSaveWarning()` currently overlays a card and blocks for 1,600 ms. Preserve the warning on all Settings-root exit methods, but schedule its dwell with a timestamp and an explicit continuation to Home. Optionally allow a released acknowledgement. Do not let a touch dismiss it and then activate Home.
- `showProfilePage()` renders “Sign in with Clock Setup” as a button, but has no action handler. Since authorization occurs in Windows Setup, use clearly instructional copy or an informational explanation; do not imply that tapping it signs in on the device.
- `showDeveloperModePage()` calls `Serial.setDebugOutput(true)` and text says logging is enabled while the page is open. There is no matching exit reset. If that page-specific lifetime is intended, central route exit should call `Serial.setDebugOutput(false)`; do not silently make a page-visit side effect permanent.
- Recovery/rollback messages should be dismissible information, **not** `S_UPDATE`. Retain their existing no-change meaning.
- `runUpdate()` currently owns input synchronously. Keep it exclusive during actual firmware installation. In adjacent `bootloader.h`, `BL::installCandidate()` calls `Update.writeStream()` before reporting progress (`128–130`); the current callback does not provide continuously sampled download progress. Show an indeterminate checking/downloading state until meaningful progress is available rather than fabricating percentages.
- Do not promise Cancel/Back during `Update.writeStream()` without a specifically implemented safe phase boundary and abort policy. Safe-failure behavior in `BL::installCandidate()` is not the same as an implemented user-cancel contract. Successful installation still restarts; unsuccessful/up-to-date completion still returns safely to Home via the current callers.

## 8. Redraw ownership and responsiveness

### 8.1 Never draw another route's regions into the current route

**Immediate repair:** Remove the cross-route display effect of these sequences, while retaining the underlying setting changes:

- General settings: `saveSettings(); showSettingsPage(); drawStatusBlock();` at `1930`.
- Calendar settings: `saveSettings(); showSettingsPage(); drawRightPanel();` at `1946`.

Mark a **proposed** Home dirty bit instead. Render Home status/right panel when Home is next visible. A model update is not permission to paint that model's Home representation on every screen.

`refreshSdState()` should update storage state and invalidate the active route's storage indicator/row. If it needs a larger redraw, route it through the exact active substate. While `alarmRinging` is visible, its modal owns the display: a Home `drawStatusBlock()` due to SD change must not paint over the alarm card/Dismiss button.

### 8.2 Dirty-region recommendations grounded in existing drawing functions

| Existing function | Recommended redraw boundary/policy |
| --- | --- |
| `drawPasswordField()` | Keep field-only updates. Cache the last shown text/masking state; clearing/drawing the same field on every irrelevant event is unnecessary. |
| `drawAnyKey()` / `drawKbKey()` / `drawKey()` | Redraw the captured key on Down, loss of ownership, and Up. No `delay(60)` press animation. |
| `showKeyboard()` | Full page only on entry, layer/layout change, or genuinely changed error layout. Show/Hide updates field + that key, not the whole page. |
| `drawSettingsToggle()` / `showSettingsPage()` | Redraw the affected row/switch for a toggle. Whole Settings page on route/category change; full theme re-render is legitimate because palette changes globally. |
| `drawAlarmList()` | Toggle just the selected row switch/value; rebuild list on Add/Save or entry. |
| `drawAlarmEditor()` / `drawClockEditor()` | Redraw time, date, repeat cell, or enabled footer after draft edits, not the whole screen for every step. |
| `drawWifiPageStatus()` | Cache connection state, SSID, RSSI level, and connecting animation frame. Continue 180 ms glyph animation only while connecting; no 448 × 38 row repaint every 180 ms when stable. |
| `drawStatusBlock()` | Cache battery/SD/Wi-Fi states and only draw changed fields. A 30-second battery sample can remain, but Home connecting feedback should not wait for the entire 30-second block cadence. |
| `drawTimeBlock()` | Preserve minute/format/date updates. Avoid clearing the full 240 × 224 region if only one digit or AM/PM changes, if partial rendering is reliable. |
| `drawRightPanel()` | Split weather and calendar invalidation; avoid re-decoding SD PNGs merely to change an assignment label. |
| `drawUpdatePage()` / `updateProgress()` | Draw the update shell once per operation, then status/progress changes. Reset progress cache at operation start; its current `static last` spans operations. |
| `showSdSaveWarning()` | Modal rectangle + timed continuation, with no blocking dwell delay. |
| `showHome()` | Full Home render on committed entry/wake; then incremental fields. |

Update scheduling and renderer ownership should live in one UI loop. If network work is moved to another task, it may update result data/flags but must not call TFT functions concurrently. Do not assume SD/TFT/network threading is safe without library and board validation.

HTTP calls in `fetchWeather()` and `geocodeManualCity()` have no explicit interaction deadline in this source. This does not establish the libraries' default timeout values. Set documented bounded operation deadlines and show the busy state **before** starting work. A spinner cannot remain responsive if the same UI thread is blocked inside HTTP; use staged/cooperative calls or a worker plus result handoff if necessary. Input, alarms, and display state must have predictable service opportunities; animation cannot compensate for a blocked state machine.

## 9. Sleep/wake and alarm priority

### 9.1 Observed display lifecycle

- Sleep is **display sleep**, not MCU deep sleep: `sleepDisplay()` fills black, sends display-off/sleep-in, and drives `TFT_BACKLIGHT_PIN` low.
- It occurs only for `S_HOME` after `60000UL` since accepted touch activity.
- `screenBeforeSleep` is recorded but not used to restore a screen. `wakeDisplay()` deliberately calls `showHome()`.
- The first wake reading is consumed by the sleeping branch, but the same held finger can be accepted again after the old 220 ms gate and navigate/toggle Home.
- `wakeDisplay()` keeps the backlight off during panel wake commands, then enables it **before** `showHome()` redraws. The code intends no white flash, but actual flash behavior has not been measured.
- While sleeping, `loop()` returns before time/alarm/weather/update work. While ringing, sleep and `refreshSdState()` still run before the alarm's input branch.

### 9.2 Recommended lifecycle, preserving Home-only sleep

1. Sample/update activity from stable valid contact; do not refresh it from animation, redraw, SD refresh, or periodic weather work.
2. Before processing Home idle sleep, service due alarms and modal state. If `alarmRinging`, suppress idle sleep. No blanket sleep policy is added to editors/setup.
3. Sleep entry cancels any pressed target/swipe, disarms transient destructive confirmation, clears pending input, and executes the existing black/panel/backlight commands with their required delays. Sleep is a state transition, not merely `screenSleeping = true` layered over a live contact.
4. Keep lightweight time/alarm evaluation active while the display sleeps and on other pages. Display sleep should not disable the alarm feature. Due alarm sets ringing state and wakes the panel without simulating a user tap; preserve one-time disabling on dismissal and repeat-alarm enablement.
5. User wake gives the contact `WakeContact` ownership. It has no navigation/dismiss/reset action. Require stable release before any new Home action. `lastActivity` starts at wake completion; it is not carried from the preceding Home session.
6. Wake sequence: backlight remains low → sleep-out `0x11` → required panel delay → display-on `0x29` → required delay → render Home or the priority ringing modal while dark → enable GPIO27 backlight. Preserve the current panel commands/delays until controller-specific validation supports changes. Rendering while dark reduces partial-frame exposure; it does not constitute verified no-flash behavior.
7. A ringing modal consumes all UI events. Preserve the currently implemented **tap anywhere** dismissal policy, explicitly communicate it, and commit it on a distinct released tap. The contact that caused wake must not immediately dismiss an alarm. `dismissAlarm()` continues to return Home and turn the LED off; no snooze/audio feature is added.
8. On wake/entry, invalidate time, status, and right panel so the display cannot retain pre-sleep values. `screenBeforeSleep` should either be clearly documented as diagnostic only or replaced by the explicit route/sleep state; do not use it to silently change the specified Home wake policy.

## 10. Acceptance scenarios for later implementation

These are **proposed checks**, not tests executed in this review. Start with simulated contact/state sequences where possible, then validate mapping, noise, feel, timing, and panel behavior on the actual Hosyond device.

| Scenario | Required outcome |
| --- | --- |
| Hold Factory Reset for 2 seconds | Arms at most once; does not reset. A second separate, in-target released tap within the arm window is required. |
| Press Reset then wait past expiry / Back / change route | Arm clears; no delayed destructive action. |
| Hold a Settings toggle, theme row, scan row, or Enter | At most one action. No action repeats after navigation. |
| Rapidly tap two password letters with distinct releases | Two intended characters, no duplicates, no global 220 ms lockout. |
| Brief missing touch sample during a hold | No new contact ID; no second tap/reset confirmation. |
| Edge swipe across a Settings toggle or alarm row | No row action before gesture recognition; exactly one Back on eligible release. |
| Short/reversed/vertical/over-time edge gesture | Cancels cleanly; route and drafts unchanged. |
| Rightward drag starting in the middle of a page | No unintended Back or committed row toggle after tap cancellation. |
| Settings-root Back button vs committed swipe | Same Home destination and same no-SD warning behavior. |
| Home SD entry vs Settings Storage entry | Back returns to the actual caller, not a stale category. |
| Direct Browse Files vs Storage-page Browse Files | Each returns to its recorded parent. |
| General → Appearance vs root → Appearance | Back returns to General or root respectively; all five themes remain selectable. |
| Tap displayed Home date, battery, icons, separators, and blank gaps | Only the documented corresponding targets act; date still provides 12/24-hour shortcut. |
| Tap stepper gaps / Repeat label / PIN margins | No numeric/day/PIN action. All intended targets stay in screen bounds. |
| Enter developer PIN one digit at a time | Draft accumulates, masked length updates, Clear works, and current accepted PIN can reach Developer Mode. |
| Tap developer recovery/rollback information | No firmware change; acknowledgement returns to Developer Mode rather than stranding `S_UPDATE`. |
| Change a General or Calendar toggle | Only the active Settings row changes; no Home status/right-panel content appears on Settings. |
| Insert/remove a card during Classroom/sync setup or ringing | Active wizard/modal remains correct; only relevant storage status updates. No invisible wizard hit zones behind normal Settings. |
| Configure another network after setup is complete | Connection returns to its caller, retains Classroom/sync choices, and uses coherent online flags. |
| Cancel city edit / fail geocoding / prepare SD afterward | Prior committed location remains consistent; no city value becomes a Wi-Fi credential. An intentionally retained offline city is explicitly unverified. |
| Change alarm date / leap-year month / repeat days | Existing wrapping/clamping and Sunday-first repeat bits remain; displayed date order is DD/MM/YYYY. |
| Save alarm or clock once | One commit only; transition contact cannot activate the destination list/Home. Cancel preserves saved data/time. |
| Reach `MAX_ALARMS` | No invisible Add action; all four alarms remain editable/toggleable. |
| Leave Settings without a ready SD via button or swipe | Same temporary-settings warning, predictable dwell/acknowledgement, no touch-through to Home. |
| Leave Home untouched for one minute | Existing Home-only black/panel/backlight sleep occurs; controls on other pages do not gain a new sleep policy. |
| Hold a finger on the sleeping screen | Wakes to Home but does not immediately open a page or toggle format. |
| Alarm due while sleeping / in an editor | Alarm service remains active, modal wakes/overlays correctly, and the user's draft is not committed or overwritten. Dismiss returns Home as specified. |
| Alarm visible beyond Home idle timeout / SD state changes | No sleep or underlying Home redraw obscures the modal. |
| Touch wake when an alarm is due | Wake contact cannot also dismiss the alarm; a new released tap can dismiss it. |
| Update check fails / returns up to date / installs successfully | Current failure/up-to-date Home return and success-only reboot remain. No unsupported user cancellation during flash write. |
| Repeated stable Wi-Fi/status frames | No unnecessary row/full-page repaint; connecting glyph still animates when appropriate. |
| Orientation, corner/center taps, light pressure, edge swipes, wake flash | Verify physically with the unchanged supplied calibration first; record actual observations and tune only proposed interaction thresholds where justified. |

### Recommended sequencing and boundaries

Fix ownership and stranded/corrupted state before animation. A polished slide does not make hold-to-reset or cross-route rendering safe. Implement in the raw same-name sketch only through a separately authorized change task; this document does not authorize publishing a binary, changing release identity, updating manifests/TODO, or marking hardware checks complete.

For pressed states, use existing semantic colors (`UI_SELECTION`, `UI_ACTION`, `UI_ON_ACTION`, `UI_DISABLED`) through `drawKey()`, `drawAppleButton()`, and row renderers. Make disabled/read-only rows unmistakably noninteractive. Give Factory Reset an explicit destructive role using `UI_RED` rather than relying on its current primary-button blue state; visual emphasis supplements, but never replaces, distinct-contact confirmation. Add no gratuitous bounce, haptic promises, sounds, or features unsupported by this hardware/source.

## 11. Ordered implementation checklist

1. **Freeze the baseline and interaction invariants.** Retain `TFT_W/TFT_H`, `TFT_ROTATION`, `TOUCH_CAL_*`, pressure `350`, `MAX_ALARMS`, all setup choices/themes, SD-only persistence, safe OTA behavior, Home-only one-minute sleep, wake-to-Home, and current alarm dismissal/reset scope. Establish the later build/hardware validation plan without marking it complete.
2. **Replace level-triggered dispatch around `readTouch()`.** Add one-read sampling, stabilized Down/Up, contact IDs, tap slop, route-generation ownership, half-open coordinate validation, and cancellation. Remove ordinary 220 ms hold repetition and the keyboard's blocking `delay(60)` press flash.
3. **Close the destructive-contact defect first.** Bound the reset button in both axes; require two distinct released contact IDs, an explicit arm timer, disarming on exit/expiry, and a one-shot `factoryResetClock()` call. Verify that no long hold can execute reset.
4. **Create shared render/hit descriptors.** Normalize Back, Settings rows, grouped rows, Home object partitions, scanner/footer targets, alarm list/switch/footer targets, repeat cells, numeric steppers, and PIN keys. Check bounds before row/column indexing; make margins/read-only rows inert. Reflow overlapping controls rather than expanding hit rectangles blindly.
5. **Separate route entry, initialization, and redraw.** Preserve PIN/text/alarm/clock drafts across redraws; make wizard steps explicit; ensure `showDeveloperPinPage()` clears only on entry and shows actual masked length. Keep current PIN acceptance and five distinct version taps.
6. **Fix display ownership defects.** Replace `drawStatusBlock()`/`drawRightPanel()` calls inside Settings handlers with Home invalidation. Make `refreshSdState()` redraw the exact active wizard/route and respect the ringing modal. Confirm that visible controls always match the dispatched route.
7. **Centralize Back and parent context.** Record Settings substates, caller-specific Storage/files paths, scanner/editor purpose, and General → Appearance context. Fresh Home Settings entry opens root; root button and swipe both execute `leaveSettingsPage()` and its warning. Clear page-scoped developer logging on exit if its stated lifetime is retained.
8. **Implement edge swipe-back as a cancellable gesture.** Use the proposed 24 px edge, intent/vertical limits, existing 90 px distance and 900 ms initial bounds; cancel the captured tap before it commits. One release pops one parent. Add resource-appropriate finger-following feedback only after this contract works.
9. **Separate onboarding from network maintenance.** Preserve first-run Classroom/sync and setup-before-OTA sequencing; normal connection success returns to its caller without replaying the wizard. Define online/offline flags at explicit successful connection, not merely scan start. Retain saved-network order/capacity and honest busy/failure states.
10. **Isolate city draft state from Wi-Fi credentials.** Do not change committed mode/name/coordinates before Save/Enter success or explicit unverified offline acceptance. Cancel restores the prior location mode; failed lookup retains draft/error; `prepareSdCard()` must not save city text as network data.
11. **Repair modal escape and transient flows.** Replace developer informational `drawUpdatePage()` calls with dismissible no-change information; make `showSdSaveWarning()` a timed nonblocking continuation; keep Profile authorization instructional. Give actual OTA exclusive phase-aware ownership with no invented flash-write cancellation or progress.
12. **Make controls contact-driven and efficient.** Restrict repeat to steppers/backspace; update pressed keys, switches, selected rows, values, and error fields only when changed. Normalize alarm display dates to DD/MM/YYYY and clarify repeat-day labels without changing stored fields or schedule semantics.
13. **Decouple alarm service from rendering and navigation.** Evaluate due alarms while sleeping and on other pages, route ringing as the highest-priority modal, preserve drafts without committing them, inhibit Home idle sleep while ringing, and retain one-time disabling/repeat enablement and tap-anywhere dismissal with explicit copy.
14. **Harden sleep/wake contact and frame ordering.** Preserve panel commands/delays/GPIO27 control and wake-to-Home; ignore the wake contact until stable release; render the final Home/alarm frame while the backlight is still low, then enable it. Do not claim no-flash behavior without physical observation.
15. **Optimize redraw and busy scheduling after correctness.** Add changed-field caches to `drawWifiPageStatus()`, `drawStatusBlock()`, time/weather/calendar, and update status; render page shells only on entry or necessary theme/layout changes. Bound network operation deadlines and use cooperative phases or a safe UI-thread result handoff where needed.
16. **Validate and document honestly in the implementation task.** Run deterministic contact/state cases from Section 10, compile with the documented board/library configuration, and then physically check corner/center mapping, target spacing, hold/noise behavior, edge cancellation, alarm wake, backlight ordering, and SD/modal interactions. Record exactly which checks ran; leave unavailable hardware validation explicitly open.
