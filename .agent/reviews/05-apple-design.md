# ClockOS Apple Settings Visual Design Specification and Implementation Audit

**Audit ID:** `05-apple-design`
**Scope:** The 480 × 320 ClockOS device UI and Windows installer visual system only. Reviewed: all five `.source/themes/appearance/*.json` files, `.source/uncompiled/updates/ClockOSv2.7/ClockOSv2.7.ino`, `config.h`, and `.source/install/install.py`.
**Method:** Static implementation audit. This specifies an **Apple Settings-inspired** system—calm grouped hierarchy, clear semantic controls, and predictable navigation—not a branded copy of Apple software.
**Change status:** **No ClockOS source, theme, installer, launcher, or firmware files were edited.**

---

## 1. Design decision

ClockOS should present Settings as a **single-pane grouped list on the 480 × 320 device** and as a **responsive Settings-style inspector on Windows**. It should not use a persistent, compressed device sidebar or a stack of independent rounded buttons.

> **One visual language, two layouts:** device Settings is a tap-first grouped list; the Windows installer is a resizable, keyboard-accessible settings panel with a protected action footer. Both use the same semantic colors, typography roles, row anatomy, selected state, control behavior, and hierarchy.

### Non-negotiable device constraints

| Constraint | Required design response |
|---|---|
| `TFT_W × TFT_H = 480 × 320`, landscape rotation 3 | Keep all proposed device coordinates in the current logical coordinate space. Do not redesign by rotating the UI. |
| Current calibrated touch transform and pressure threshold | Preserve `TOUCH_CAL_*`, `tft.setTouch(calData)`, and the initial `getTouch(..., 350)` threshold. Geometry must not be “fixed” by changing calibration. |
| ESP32/TFT RGB565 rendering | Store semantic colors as hex in themes/design tokens, then convert to RGB565 at draw time. Do not depend on alpha compositing, blur, or platform font rendering. |
| Small, arm’s-length display | Use one-line labels and concise values in rows; keep help text out of primary rows. |

---

## 2. Compact design-system contract

### 2.1 Semantic color tokens

The existing theme schema is a useful starting point, but colors must be consumed by role, not sprinkled as RGB literals. Every draw helper receives roles below; helpers must never choose an unrelated literal blue or gray.

| Token | Purpose | Light-system baseline | Dark-system baseline | Rule |
|---|---|---:|---:|---|
| `pageBackground` | Whole page / grouped-list canvas | `#F2F2F7` | `#000000` | Use the selected theme `background`; do not use black for some screens and themed backgrounds for others. |
| `groupSurface` | Group card and standard row fill | `#FFFFFF` | `#1C1C1E` | Map to the selected theme `surface`. |
| `elevatedSurface` | Dialog, keyboard tray, installer footer | `#FFFFFF` | `#2C2C2E` | One step above `groupSurface`; not a gradient. |
| `label` | Titles and enabled primary text | `#1C1C1E` | `#F5F5F7` | Map to the selected theme `text`. |
| `secondaryLabel` | Values, explanatory text, inactive icon | `#6D6D72` | `#A1A1A6` | Map to a contrast-qualified form of theme `muted`; body-size text must meet 4.5:1 against its surface. |
| `tertiaryLabel` | Disabled/metadata only | `#8E8E93` | `#636366` | Never use for enabled body text or essential state. |
| `separator` | Inset dividers | `#D1D1D6` | `#38383A` | One physical pixel; no detached-card outlines between every row. |
| `tint` | Links, disclosure values, selected checkmark, focus | `#007AFF` | `#0A84FF` | Map to theme `primary` only when its on-surface and on-action contrast passes; otherwise use the qualified action tint below. |
| `selectionFill` | Selected category/row background | `#DCEBFF` | `#173A63` | Opaque pre-composited tint, not a saturated full-row blue. |
| `actionFill` | Filled primary action | `#0066D6` | `#0066D6` | White text on this blue is 5.42:1. Theme-specific action exceptions are listed below. |
| `onAction` | Text/icon on a filled primary action | `#FFFFFF` | `#FFFFFF` | Never derive this from ordinary body `label` without contrast validation. |
| `switchOn` | Enabled switch track | `#34C759` | `#30D158` | Status is also expressed in the label/value; color alone is not the sole signal. |
| `danger` | Destructive label, icon, and confirmation affordance | `#FF3B30` | `#FF453A` | Prefer red text/outline in a separated danger group; reserve filled red actions for confirmed destructive steps. |
| `success` | Positive storage/connection state | `#248A3D` | `#30D158` | Pair with an explicit word such as **Ready**. |
| `focusRing` | Keyboard focus on Windows / optional active feedback | `#005FCC` | `#64D2FF` | 2 px outer ring; do not indicate focus by color-only label changes. |

### 2.2 Required mapping of the shipped themes

The five JSON packages remain the source of theme identity. Add/derive the roles in the table below when themes are loaded; the JSON fields must not become documentation-only data. `actionFill` is intentionally distinct from decorative `accent`: it is chosen for readable action text.

| Theme | `pageBackground` | `groupSurface` | `label` | `secondaryLabel` | `tint` / `selectionFill` | `actionFill` / `onAction` |
|---|---|---|---|---|---|---|
| Crystal | `#F2F4F8` | `#FFFFFF` | `#14151A` | `#6B7280` | `#007AFF` / `#DCEBFF` | `#0066D6` / `#FFFFFF` |
| Ocean | `#EAF7FF` | `#FFFFFF` | `#08243D` | `#54738C` | `#0066CC` / `#D7ECFB` | `#0066CC` / `#FFFFFF` |
| Sunrise | `#FFF6EE` | `#FFFFFF` | `#321B16` | `#7A544A` | `#FF6B35` / `#FFE2D5` | `#C84D22` / `#FFFFFF` |
| Midnight | `#090B12` | `#171B27` | `#F5F7FF` | `#9AA4B8` | `#5E9BFF` / `#183A63` | `#0066D6` / `#FFFFFF` |
| Graphite | `#202225` | `#303238` | `#FFFFFF` | `#A2A8B5` | `#D7DCE5` / `#474B54` | `#D7DCE5` / `#202225` |

**Theme schema requirement.** Retain the existing `background`, `surface`, `primary`, `text`, `muted`, and `accent` fields. Add `separator`, `selectionFill`, `actionFill`, `onAction`, and optionally `secondaryText` only where a derived value cannot meet contrast. Firmware may synthesize the last five values from the table to avoid JSON parsing complexity, but it must use a single `ThemeTokens` structure after selection. `accent` is decorative (illustration/highlight) unless it also passes the explicit interactive-color checks.

**No literal-color rule.** After tokens are introduced, the following must not appear inside Settings, header, row, button, or installer component renderers except in the token definition: `C(10,132,255)`, `C(0,122,255)`, `C(170,175,188)`, `C(58,62,72)`, black, white, or theme-unrelated gray literals.

### 2.3 Typography scale

Use the installed FreeSans faces on the device and Segoe UI / Segoe UI Semibold on Windows. Do not introduce a downloadable font dependency.

| Role | Device source face | Device use | Windows use | Rules |
|---|---|---|---|---|
| Display clock | `F24B` (`FreeSansBold18pt7b`) | Home time only | Not used | The seven-segment clock remains a deliberate exception. |
| Page/nav title | `F18B` (`FreeSansBold12pt7b`) | One line, centered or left-aligned in 44 px nav bar | 20 px Semibold | Never pair this with a second title in the same header. |
| Section label | `F12B` (`FreeSansBold9pt7b`) | Group caption or selected row label | 13 px Semibold | Uppercase only for small metadata, never for the main information hierarchy. |
| Row title | `F12B` | One line per row | 12–13 px Semibold | Minimum device row label; truncate with ellipsis only after preserving the state/value. |
| Row value / body | `F12` (`FreeSans9pt7b`) | One right-aligned concise value | 11–12 px regular | Enabled text uses `label` or `secondaryLabel`, never `tertiaryLabel`. |
| Metadata / hint | `F12` | Only in a footer or a distinct informational group | 10–11 px regular | Do not create two-line explanatory device rows at the 42–50 px density. |

### 2.4 Shape, spacing, and icon rules

| Element | Device specification | Windows installer specification |
|---|---|---|
| Outer page inset | 12 px left/right | 20–24 px content inset |
| Navigation/header height | 44 px | 56–64 px toolbar/header |
| Group/card radius | 12 px | 12 px (native control corners may remain OS-native) |
| Group gap | 12 px | 12 px between groups; 16 px between major sections |
| Row divider | 1 px inset from content start, never under leading icon | 1 px inset divider or native grouped-row border |
| Leading icon | 26 × 26 px colored rounded square, 5 px inner radius; 14–16 px monochrome glyph centered | 24 × 24 px, decorative; label always carries the meaning |
| Icon palette | One semantic tint per category; icon glyph is `#FFFFFF` only on sufficiently dark icon tile | Same category icon set; no emoji, gradients, or mismatched line weights |
| Chevron | Two 1–2 px stroked lines / compact filled glyph, 12 × 12 px visual | Native disclosure indicator or a consistent 12 px chevron; never text `>` |
| Row pressed state | 50–70 ms `selectionFill`, then route/toggle update | Native pressed state plus focus ring; no silent action |

Icons use a single 1.5–2 px stroke weight and simple, filled system-symbol-like silhouettes. A category icon establishes recognition; it does not reduce the row’s touch target and is not a separate button.

---

## 3. Device Settings architecture and exact geometry

### 3.1 Category hierarchy—no persistent 143 px sidebar

A persistent sidebar is **not approved** for a 480 px-wide device. It wastes 30% of the canvas, leaves only 318 px for two-line detail rows, and reads as a compact desktop control panel rather than a Settings app.

**Root hierarchy:**

1. **Settings root** — six full-width category rows: General, Wi-Fi, Appearance, Calendar, Storage, System.
2. **Category detail** — grouped preference rows and toggles for one category.
3. **Child flow** — scanner, storage setup/files, theme picker, factory-reset confirmation. Each child returns to the category detail that launched it.
4. **Home** — only the root Back control returns to Home.

At a future width large enough for an actual split view, a sidebar may return only if it is **at least 180 logical px wide**, uses the same 44–50 px rows, and keeps a detail column of at least 420 px. That condition cannot be met on this panel.

### 3.2 Settings root: exact 480 × 320 layout

Coordinates use left/top inclusive, right/bottom exclusive bounds.

```cpp
constexpr Rect kNavBack     = {12, 0, 84, 44};
constexpr Rect kRootGroup   = {12, 52, 456, 252};
constexpr int  kRootRowH    = 42;
constexpr Rect kRootRows[6] = {
  {12,  52, 456, 42}, {12,  94, 456, 42},
  {12, 136, 456, 42}, {12, 178, 456, 42},
  {12, 220, 456, 42}, {12, 262, 456, 42}
};
```

| Root element | Position and behavior |
|---|---|
| Navigation bar | `y=[0,44)`, `pageBackground`; title **Settings** centered at x=240. The Back/Home visual may be 70 × 30, but its hit rect is `kNavBack` (84 × 44). |
| Group surface | One `groupSurface` rounded rect `kRootGroup`; not six separate pills. |
| Category row | Exactly the matching `kRootRows[i]` rectangle is drawn, highlighted, and hit-tested. No y-gap and no hit outside x=`[12,468)`. |
| Category icon | x=26, 26 × 26, vertically centered in row. |
| Label / value | Label left x=64; concise state/value right-aligned to x=430; chevron visual centered around x=450. |
| Separators | x=`[64,456)`, y at 94, 136, 178, 220, and 262. Do not draw through the icon tile. |

Root values are concise only: **24-hour**, SSID or **Not Connected**, selected theme, **On/Off**, **Ready/No card**, and **v2.7**. Explanations such as “Use 24-hour clock format” do not appear on root rows.

### 3.3 Detail screens, row density, and controls

```cpp
constexpr Rect kDetailBack = {12, 0, 84, 44};
constexpr int kDetailX = 12, kDetailW = 456;
constexpr int kDetailTop = 56;
constexpr int kDetailRowH = 50;
constexpr int kGroupGap = 12;
```

| Detail element | Required behavior |
|---|---|
| Header | Same 44 px bar. Back returns to Settings root—not Home. Category title centered; do not show profile/SD global icons in this navigation bar. |
| Preference group | One rounded `groupSurface`; group rows are 50 px high. Up to three common preferences fit at y=56–206; a separate second group begins after a 12 px gap. |
| Standard row | Full row is one tap target. One title left; one concise value, switch, checkmark, or chevron right. Use an inset separator between rows. |
| Toggle | Visual track is 46 × 28 with a 24 px knob, right-aligned at x=410–456; row target remains 456 × 50. On uses `switchOn`, off uses `tertiaryLabel`; knob is `#FFFFFF`. |
| Disclosure | Right-side value is `secondaryLabel` or `tint`; custom-drawn chevron is the only disclosure affordance. |
| Theme selection | Five 42 px theme rows in one group starting y=56. The selected row gets `selectionFill` plus a `tint` checkmark; no full-blue row and no “Selected” text needed. |
| Destructive setting | Separate danger group. Use `danger` label/icon; factory reset continues to require a second confirmation. |
| Help text | One `secondaryLabel` line below a group or in an informational card—not a second line squeezed into ordinary rows. |

**Category contents:**

| Category | Grouped rows |
|---|---|
| General | 24-hour time (toggle); Battery percentage (toggle); Appearance (disclosure). |
| Wi-Fi | Network (status/detail); Choose network (disclosure); Saved networks (disclosure only when a management route exists). |
| Appearance | Crystal, Midnight, Ocean, Sunrise, Graphite—one selected checkmark. |
| Calendar | Calendar (toggle); Classroom (toggle); Notifications (toggle). |
| Storage | ClockOS storage (status); Prepare storage (disclosure/action); Browse files (disclosure). |
| System | ClockOS version/About; separately grouped Factory Reset. The Developer shortcut remains hidden and scoped to the version row. |

### 3.4 Touch, accessibility, and navigation policy

1. **Shared rectangles:** Rendering and hit testing use the same `Rect` table. Blank margins, dividers, and inter-group gaps must not trigger a setting.
2. **Target size:** Every ordinary detail row is **456 × 50 logical px**; controls are actionable through the whole row. Root category rows are **456 × 42 logical px**, the maximum six-row no-scroll compromise at this resolution. Validate on hardware that this is usable at the actual panel size; if it is below the team’s physical 9 mm target, make the root vertically scrollable rather than shrinking rows further.
3. **Critical action targets:** Back hit targets are 84 × 44; primary confirmation actions are at least 160 × 44; on-screen keyboard keys remain at least 36 px high and should move toward 42 px where the layout permits.
4. **Tap-first:** No task requires a swipe. Remove Settings’ global right-swipe-to-Home. If retained, an optional back gesture must start at `x <= 24`, travel right >=80 px with vertical movement <=40 px, and call local `popSettings()`.
5. **Navigation state:** Use a small Settings-local route (`ROOT`, `GENERAL`, `WIFI`, `APPEARANCE`, `CALENDAR`, `STORAGE`, `SYSTEM`) plus an explicit child return destination. Do not hard-code each child page to Home/Settings based on a global screen guess.
6. **Contrast:** Enabled body text must reach 4.5:1 against its actual surface; large display text at least 3:1. Status has both color and text (**Ready**, **Needs setup**, **Not connected**). Never rely on red/green or a checkmark alone.
7. **State feedback:** A 50–70 ms pressed row fill is sufficient; update a switch/check locally where possible. Full redraws are acceptable for route changes, not required for every toggle.

---

## 4. Windows installer specification

### 4.1 Layout and category hierarchy

The installer is a desktop Settings surface, not a small fixed-height wizard. It must preserve Windows-native widgets, keyboard focus, and standard dialogs while applying the same grouping and semantic roles.

| Area | Specification |
|---|---|
| Window | Initial size `min(1024, workAreaWidth-64) × min(660, workAreaHeight-120)`, minimum safe size 840 × 540 where the work area allows; centered; **resizable**. Do not force `-topmost`. |
| Header | 56–64 px. 24–28 px clock mark, **ClockOS vX.Y** title, **Setup** subtitle, quiet release identity. |
| Sidebar at >=900 px content width | 184–196 px `pageBackground` sidebar, normal-case labels, 44 px selected category rows: Overview, Storage, Firmware & Device, Services. Selected row uses `selectionFill`, `label`, and the same 24 px icon treatment. |
| Compact width / high scale | Replace the sidebar with a horizontal segmented category control; content becomes one scrollable column. Do not compress type or hide the action footer. |
| Main content | Grouped list/cards for the selected category. Put the common Overview flow first; place high-risk storage details in Storage, firmware mode/device selection in Firmware & Device, and Classroom in the optional Services section. |
| Common path | **Auto firmware** is a segmented control choice, not a sidebar radio button. The SD install and Flash switches reveal their dependent selector rows only when enabled. |
| Footer | Fixed 76–80 px footer outside the scrollable content: left status + 4–6 px progress bar; right quiet **Close** and 172–184 px primary **Start Setup**/**Try Again**. Footer is visible at every size and state. |

### 4.2 Installer token usage and controls

Use `System Light` by default when Windows is light and `System Dark` when Windows is dark; if reliable OS appearance detection is not implemented, use the dark baseline consistently and retain system high-contrast/native focus behavior. Do not mix an ad hoc black sidebar with unrelated dark cards.

| Installer component | Exact specification |
|---|---|
| Canvas and surfaces | `pageBackground`, `groupSurface`, `separator`, `label`, and `secondaryLabel` tokens. The current `#0F0F10/#1C1C1E/#303033/#0A84FF` palette can map to dark-system roles, not to one-off widget values. |
| Group heading | 13 px Semibold; 8 px spacing to first row. Avoid tiny all-caps headings such as **FIRMWARE MODE** as the primary hierarchy. |
| Settings row | 48–52 px high; title left, value/control right, 12–16 px horizontal inset. Use a 1 px inset separator; a row has one action/meaning. |
| Selection | Segmented Auto/Beta/Manual: 36–40 px high, `selectionFill` selected state, `tint` check/icon, visible keyboard focus ring. Do not use text-only radio buttons painted as sidebar items. |
| Toggle/checkbox | Prefer a labelled native checkbutton or accessible switch with a 44 × 26 visual. Its row is the pointer target; Space toggles when focused. |
| Combobox | 36–40 px visual height, full available width; external label rather than placeholder-only meaning; Refresh is a 36 px quiet button. |
| Primary action | `actionFill`/`onAction`, 36–40 px visual height within a >=44 px footer hit region; disabled has a visibly muted fill plus explanatory status. |
| Secondary action | `groupSurface`/`label` with border or native quiet button; does not compete with the primary action. |
| Focus/error | 2 px `focusRing`; an inline error/status row that persists after a modal is closed. Failure/cancel restores editable controls and changes primary to **Try Again**. |

### 4.3 Desktop accessibility behavior

- Keep every input usable by Tab/Shift+Tab, Space, Enter, arrow keys, and screen-reader labels. Native `ttk` widgets are preferred where they provide accessible semantics.
- Support 125%, 150%, and 200% Windows text scaling without clipping; only the content pane scrolls, never the footer action.
- Use 11 px minimum supporting text and 12–13 px standard labels; the current 7–9 px desktop labels are too small.
- Do not disable only some job-affecting controls while work runs. Freeze every choice that is part of the job snapshot, expose **Setting up…**, and restore all controls on an error/cancel.
- Preserve destructive SD warnings before Start and the two existing confirmations, but place the preflight warning inline beneath the enabled Storage switch rather than making it a surprise at commit time.

---

## 5. Current implementation conflicts

### 5.1 Theme and token implementation

| Current implementation | Conflict with this system | Required visual remediation |
|---|---|---|
| The five JSON files define `background/surface/primary/text/muted/accent`, but firmware does not load or parse them. `applyAppearanceTheme()` is a separate hard-coded switch. | Theme files are not the source of rendered truth. `muted` and `accent` package values are effectively unused by v2.7 UI. | Create/derive one `ThemeTokens` object from the selected theme package/table and pass it to every drawing primitive. |
| `appearanceTheme` defaults to **midnight** (ino:47); theme README says **Crystal** is the default visual family. | Conflicting default identity. | Decide one documented default and use it in README, first-run setting, JSON mapping, and firmware. |
| Crystal falls through the `else` branch to `UI_BACK #C3C8DE`, `UI_TEXT #414856`, `UI_KEY #E8ECF5`, and `UI_BLUE #62ABFF` (ino:62–64), none of which match `crystal.json`. | Crystal is materially different from its shipped package and is not a grouped light system. | Map Crystal exactly to the required table above. |
| Graphite uses `#303238` as page background and `#41444C` as key surface (ino:59–61), reversing/ignoring its JSON `background #202225` and `surface #303238`. | Surface hierarchy is inverted. | Use JSON background for page and surface for grouped rows. |
| Settings and subpages hard-code several blues and grays—e.g. `C(0,122,255)`, `C(10,132,255)`, `C(170,175,188)`, `C(58,62,72)`—after theme selection. | Light themes retain dark-style dividers/muted text; selection and action tint are inconsistent. | Replace each with semantic tokens; prohibit literals outside token creation. |
| `drawAppleButton()` paints `primary` with hard-coded `#0A84FF` and labels it with variable `UI_TEXT` (ino:1564–1568). | The label contrast changes with the selected theme. Measured examples: Crystal fallback text on `#0A84FF` is **2.52:1**; Graphite white is **3.65:1**; Midnight text is **3.35:1**. | Use `actionFill` plus an explicit `onAction`. Apply the qualified theme exceptions in section 2.2. |
| Home clock/status/right panel force black, white, and `COL_DIM` (ino:988–1112; 1682–1687). | Changing Appearance does not create a coherent system canvas. | Preserve the intentional high-contrast clock if desired, but define it as a named `clockSurface/clockLabel` variant and make all other home surfaces token-based. |

### 5.2 Device hierarchy, density, controls, and touch

| Current implementation | Conflict with this system | Required visual remediation |
|---|---|---|
| `drawSettingsSidebar()` uses six 132 × 34 px entries in a 143 px rail; the remaining detail pane is 318 px wide (ino:1477–1496). | Persistent micro-sidebar is too dense for the panel and does not resemble a calm grouped device Settings hierarchy. | Replace the main shell with the section 3 root list and category detail route. |
| Main content rows are 54 px pitch with title + detail text in a 318 px pane; Appearance rows are 43 px pitch (ino:1498–1541). | Two competing densities; primary labels and explanatory text compete in small space. | Root 42 px, detail 50 px, one-line title/value anatomy, help outside the row. |
| Settings visual rows and hit bands disagree. General/Calendar visual rows at 54 px cadence accept only 49 px; Appearance rows at 43 px cadence accept 37 px. Detail actions often test y only (ino:1912–1954). | Dead zones occur between visible rows and a matching y can activate a row from unrelated x coordinates. | Use the same `Rect` for rendering and touch; require `x=[12,468)` for a list row. |
| Switch art is 38 × 22 px (ino:1431–1435); alarm switch is 50 × 24 px. | Undersized/inconsistent visual controls; no common component contract. | One settings switch: 46 × 28 px, 24 px knob, whole 456 × 50 row toggles. |
| `settingsRow`, `settingsGroupRow`, main card options, Appearance page, and alarm buttons are five competing row/button patterns. | Product feels assembled screen-by-screen, not like a single system UI. | Consolidate into `drawGroupedRow`, `drawToggleRow`, `drawDisclosureRow`, and one action-button component. |
| Disclosure affordance is a font `>` in several places (ino:1448, 1520) and strings such as `" >"` elsewhere. | Text glyph varies by font and reads as content rather than navigation. | Draw/use one 12 px chevron component. |
| Selected sidebar is opaque saturated blue (`C(39,91,179)`) with white text (ino:1481–1484); theme selection uses blue circular checkmarks. | Selection is not tokenized and changes semantics/component style across pages. | Use a low-saturation `selectionFill` plus `tint` checkmark for all selected rows. |
| Page back patterns differ: main Settings has a small top-right Back, generic detail pages have a top-left 78 × 30 Back, and a right swipe in Settings jumps to Home (ino:1407–1415; 1491–1494; 1892–1908). | Navigation is not hierarchical and a gesture can dismiss Settings unexpectedly. | One 44 px nav bar, local Back, explicit `popSettings()`, optional left-edge-only gesture. |
| Existing `F12/F12B` body faces are used for most UI and two-line explanatory text (ino:138–143; 1444–1448). | Actual reading burden is too high at arm’s length; labels have no clear scale. | Apply the typography roles in section 2.3 and remove row-level help copy. |

### 5.3 Windows installer hierarchy, density, and accessibility

| Current implementation | Conflict with this system | Required visual remediation |
|---|---|---|
| Fixed `640x460`, non-resizable, topmost root (install.py:1160–1163). | Cannot adapt to Windows scaling or content height; topmost is intrusive. | Use section 4 responsive geometry, resizable root, no unconditional topmost. |
| 145 px dark sidebar contains branding, tiny all-caps **FIRMWARE MODE**, and 9 px radio-button mode controls (1173–1198). | It is a control shelf rather than a Settings sidebar; compact type and selection do not match device hierarchy. | Use an >=184 px sidebar only at wide size, normal-case category rows, and a segmented Auto/Beta/Manual control in content. |
| Three vertically stacked cards plus action area must fit before the footer; `action.pack(fill="both", expand=True)` is last (1216–1334). | Primary action can be clipped under DPI/text scale; primary action is not protected. | Fixed 76–80 px footer outside scrollable responsive body. |
| Desktop fonts include 7 px and 8 px labels/hints and a 16 px title (1098–1116). | Supporting text is below the specified accessible desktop scale. | 20 px title, 13 px group/standard label, 11–12 px body/hint; use contrast-qualified `secondaryLabel`. |
| Palette is locally hard-coded to `#0F0F10/#1C1C1E/#F5F5F7/#A1A1A6/#303033/#0A84FF` (1098–1118). | No shared semantic token contract, selected-row treatment, focus specification, or light/high-contrast plan. | Map style configuration to the section 2 role names, with system light/dark modes and visible focus ring. |
| During work, Start, two comboboxes, and mode radios are disabled, but job-affecting checkboxes/Choose/Refresh controls stay live (1439–1449). Failures leave Start disabled (the existing installer review documents this). | Displayed choices can diverge from the running job; recovery is not visually/interaction accessible. | A single `IDLE/RUNNING/FAILED/SUCCEEDED` UI state updates every job input, footer status, and **Try Again** recovery. |

### 5.4 Complete Apple Settings pattern inventory: apply, adapt, or omit deliberately

The following inventory makes the reference model explicit. It distinguishes a useful **system-settings pattern** from proprietary Apple branding/artwork and prevents feature-for-feature imitation that would be unusable on a 480 × 320 clock.

| Pattern | Familiar system-settings purpose | Device decision and exact compact specification | Desktop installer decision | Current gap / source evidence |
|---|---|---|---|---|
| **Navigation bar and hierarchy** | Establish location and a predictable back path. | **Required.** One 44 px bar on every Settings route. Root Back/Home rect is `[12,0,96,44)`; detail Back returns one level to root; only root exits to Home. Center one page title. | **Required.** A 56–64 px app header plus responsive sidebar/category selection. Browser-style Back is not needed because installer categories are peer views. | Main Settings has a top-right 66 × 27 Back while detail screens use a different top-left 78 × 30 pattern; right swipe exits directly to Home. |
| **Sidebar / category list** | On a wide desktop/tablet, exposes peer settings areas and selected state. | **Do not use persistently.** The 143 px rail in a 480 px screen is inappropriate. Use six full-width root category rows instead. A future split view needs >=180 px rail and >=420 px detail column. | **Use only when wide.** 184–196 px sidebar at >=900 px content width; collapse to a horizontal category segment and one-column scroll view otherwise. | `drawSettingsSidebar()` reserves 143 px yet creates 34 px rows; installer hard-codes a 145 px mode shelf. |
| **Search** | Finds a preference by its name/synonym and returns a route/result, not an action shortcut. | **Optional, not root-visible in v1.** Six categories and fewer than roughly 24 settings do not justify consuming 42 px of root space. If setting count grows, add a dedicated full-screen search route with 44 px field, explicit clear action, 42 px result rows, title/value match, and a result that opens—not toggles—the preference. Never put a fake inactive search box above the six rows. | **Appropriate.** Toolbar search at widths >=840 px, `Ctrl+F` focus, filters category rows/options, exposes no hidden action, and provides “No settings found.” | No firmware or installer search exists. Adding an always-visible device search field now would crowd the root and be visually imitative without user value. |
| **Account / service header** | Gives a real signed-in person/account a clear entry point and status. | **Conditional only.** Do not invent an Apple-ID-like header. Current `profileName = "Not signed in"` and Classroom integration justify a compact **Profile & Services** detail row only when a real provider/action exists; otherwise omit it. Keep it under System/Services or a dedicated route, not as a 60–90 px root banner that displaces categories. | **Conditional only.** A small provider/status row in Services may show selected Classroom OAuth file or “Not configured”; it is not product identity. | Firmware has a separate Profile page and top-right profile icon, but Settings root has no coherent service entry; a generic “Not signed in” hero would waste scarce height. |
| **Category icons and semantic colors** | Enables rapid scan of peer categories while text remains primary. | **Required.** Use 26 px colored tiles with self-drawn generic glyphs: General `#8E8E93`, Wi-Fi `#0A84FF`, Appearance `#AF52DE`, Calendar `#FF453A`, Storage `#30D158`, System `#FF9F0A`; apply dark/light token adjustments as needed. The full row remains the target. | **Required.** Reuse 24 px tiles/glyphs in the wide sidebar and overview cards; do not rely on icons alone. | Current bespoke 20 px icons appear only in the tiny device rail and are not shared with rows/installer. |
| **Grouped sections / cards** | Organizes related rows using surface, insets, and separators rather than an outline around every control. | **Required.** Root uses one compact category group because two visibly separated groups would force a scroll or sub-42 px rows. Details use one or more 12 px radius groups, 12 px inter-group gap, and 50 px rows. | **Required.** One card/group per concern; no unconstrained vertical pile. The scrollable body may contain groups; footer is outside it. | Firmware alternates among individual pills, 318 px cards, 448 px rows, and a two-column Appearance grid. Installer cards are visually consistent but vertically overcommitted. |
| **Row title, subtitle, and right-side value** | Separates a preference name, limited explanation/current state, and action/status. | **Required, constrained.** Root: title + concise right value only. Detail: one title and one right value/control. A subtitle is allowed only in a dedicated 64 px informational/status row or below a group, never in a normal 50 px control row. | **Required.** 48–52 px rows can use title + one 11–12 px subtitle only where it prevents a destructive error; otherwise value/action is right-aligned. | Main device rows pack two lines into 54 px at 9 pt and truncate in a 318 px pane; installer uses small hints that can be easy to miss. |
| **Disclosure chevron** | Differentiates a navigable row from an immediate toggle/action. | **Required.** Draw one 12 × 12 px chevron at x≈448 with two stroked lines. Tapping anywhere in the row opens the child route. | **Required.** Use native or matching chevron; tab/Enter opens the row. | Firmware uses literal `>`/`" >"`, which is text, not a stable affordance. |
| **Switch / toggle** | Represents an immediate independent binary state. | **Required only for independent settings** such as 24-hour time, battery percentage, Calendar, Classroom, and Notifications. Track 46 × 28, knob 24, full 456 × 50 row target; immediate change and a brief pressed state. Do **not** use a switch when a confirmation, picker, or navigation is required. | **Required.** Accessible native checkbutton/switch in a 48–52 px row; Space toggles, state/value is announced, and job controls cannot mutate while RUNNING. | Firmware uses 38 × 22 switches and visually different alarm switches; installer uses small checkboxes without shared row semantics. |
| **Slider** | Adjusts a continuous, reversible quantity with immediate feedback. | **Not currently appropriate.** No existing compact Settings value needs a slider. If hardware brightness is later exposed, use a dedicated Appearance row opening a 56 px panel: 280–360 px track, 4 px rail, 28 px thumb, +/- buttons or numeric value, and one-step tap feedback. Never use a slider for date/time, Wi-Fi choice, theme, or a destructive threshold. | **Appropriate only for a genuinely continuous installer preference** such as log verbosity/retention if added; progress is an indicator, never an interactive slider. | No firmware sliders. Do not add a decorative slider merely to resemble a mobile OS. |
| **Segmented control** | Chooses one of 2–3 mutually exclusive, immediately comparable modes. | **Use sparingly.** In a dedicated detail panel, a 2-way segment is 212 × 36 per option (or 3-way 140 × 36) inside a >=56 px control area. Suitable for Automatic/Manual weather location or 12/24-hour **only if** it replaces, rather than duplicates, a switch. Not for category navigation. | **Required for Auto/Beta/Manual firmware mode**: 3 equal segments, 36–40 px high, selected `selectionFill`, visible focus; no sidebar radio buttons. | Current firmware uses separate rows/pills for mutually exclusive setup choices; installer represents mode as 9 px painted radio buttons in the sidebar. |
| **Picker / list selection** | Chooses one item from a finite list without pretending it is a binary setting. | **Required for theme/network/date components, with route-specific forms.** Theme uses a five-row checkmark list. Wi-Fi opens scanner list. For time/date, do not copy a scrolling wheel: panel height and 220 ms input cadence make it inappropriate. Use a dedicated editor with 44 px stepper controls and an explicit Save/Cancel action, or a paged date list/calendar. | **Required for removable drive, serial device, and OAuth file.** Native labelled combobox/file picker is preferred; Refresh is adjacent and remains keyboard reachable. | Firmware Appearance is both a sidebar list and a separate two-column page; alarm/clock editor uses 30–34 px buttons. Installer has correctly native comboboxes but their labels/density need the specified row framing. |
| **Destructive actions and confirmation** | Makes irreversible/local-data actions visually distinct and difficult to trigger by accident without hiding their consequences. | **Required.** Factory Reset is the only red row, isolated in a separate group after an explanatory status row. First tap opens a confirmation page naming affected data and stating firmware/unrelated files are retained; second tap is a 44 px filled red confirm. Back/Cancel is equally reachable. Do not rely on text changing from “Factory Reset” to “Tap again” in the same location alone. | **Required.** SD erase is a danger group with preflight consequences, selected drive name, and two existing confirmations. Primary footer must state the pending action; failed/cancelled confirmation restores Idle. | Firmware visually has a reset card and two-tap arm, but it uses the general primary button styling; installer warnings occur late and the primary can remain disabled after failures. |
| **Privacy, permission, and status rows** | Communicates what is connected, what data is used, and whether a system state is actionable. | **Required where relevant, not as a fake Privacy category.** Wi-Fi row shows SSID/**Not connected**; Storage shows **Ready/Needs setup/No card**; Classroom shows **Enabled/Disabled** and provider state; location mode states Automatic/Manual. Detail/status rows are read-only unless they have chevrons/actions. Use text plus color/status icon. | **Required.** Services identifies OAuth selected/not selected; Storage names removable media; Device says detected/none. Do not expose tokens/secrets or use ambiguous green-only status. | Status appears in several styles and some rows look actionable but are no-ops (e.g., ClockOS storage in current main Settings). |
| **Spacing, alignment, and separators** | Creates a calm scan rhythm and visually defines a group. | **Required.** Section 2/3 measurements are authoritative: 12 px outer inset, 44 px nav, 42 px root, 50 px detail, 12 px group gap, label x=64, values to x=430, divider x=64–456. | **Required.** 20–24 px content inset, 12 px group gap, 48–52 px rows, 12–16 px row inset; no fixed-height clipping. | Current dimensions are page-specific, with 5–7 px untouchable gaps between drawn and active rows. |
| **Typography hierarchy** | Makes current location, preference title, state, and explanation readable at a glance. | **Required.** Use the section 2.3 role scale; one nav title, bold row title, regular concise value. Avoid all-caps navigation/category labels and long explanatory text in every row. | **Required.** 20 px title, 13 px label, 11–12 px body/hint; respect Windows text scale. | Device uses small FreeSans across hierarchy; installer includes 7–9 px labels and all-caps sidebar metadata. |
| **Selected, pressed, disabled, and focus states** | Makes state and causality visible for touch, pointer, keyboard, and assistive technology. | **Required.** Selected list option = `selectionFill` + `tint` check. Press = 50–70 ms fill. Disabled = `tertiaryLabel` plus disabled surface and a reason in status text. No persistent keyboard focus is needed on the touch-only device, but active action feedback is required. | **Required.** Add 2 px focus ring, native hover/pressed state, selected category/segment, explicit disabled fill/text, and RUNNING/FAILED/SUCCEEDED footer states. | Device selection mixes solid-blue sidebar, circular check, and selected pill; installer styles active/disabled buttons but does not define focus or recovery state. |
| **Light/dark/high-contrast behavior** | Preserves hierarchy when the base appearance changes. | **Required.** Every shipped theme maps all tokens, including separator, selection, action, on-action, and disabled state. Theme change redraws the full tokenized page. No literal black/white exceptions outside named clock-surface roles. | **Required.** Follow reliable Windows light/dark mode where feasible; otherwise use one dark system consistently. Retain native high-contrast/focus behavior rather than forcing colors over it. | Firmware has theme-specific `UI_*` values but also many literal colors; installer has one forced dark palette. |
| **Accessibility and touch/pointer sizes** | Keeps controls readable and reliably activatable. | **Required.** Full detail rows 456 × 50; root rows 456 × 42 as the documented space compromise; Back 84 × 44; primary confirmations >=160 × 44. Verify physical size/touch on hardware and make the root scroll before shrinking. | **Required.** 36–40 px visual desktop inputs within >=44 px pointer regions, complete keyboard operation, visible focus, scalable text, and no action clipped by a scroll viewport. | Existing device visual controls reach 22–34 px heights; installer’s 460 px fixed root clips at high scale. |

### 5.5 Information architecture to implement—not merely visual dressing

#### Device route map

```text
Home
└── Settings root
    ├── General ── 24-hour time / Battery percentage / Appearance → Theme list
    ├── Wi-Fi ── Network status / Choose network → Scanner / Saved networks → manager when implemented
    ├── Appearance ── Theme list
    ├── Calendar ── Calendar / Classroom / Notifications / Profile & Services only when configured
    ├── Storage ── Status / Prepare storage → confirmation / Browse files → viewer
    └── System ── About/version / Profile & Services if not under Calendar / Factory Reset → confirmation
```

The root has exactly six current categories and one compact category group. Adding a persistent account banner, search field, or seventh category is **not approved** until a scroll model or a different root information architecture is intentionally introduced. The optional Profile & Services route is contextual; it must not masquerade as a device account if no identity is configured.

#### Windows installer route map

```text
Setup
├── Overview                Auto mode summary; the most common safe path
├── Storage                 SD install toggle, removable media selector, erase disclosure
├── Firmware & Device       Auto/Beta/Manual segment, Flash toggle, serial-device selector
├── Services                Optional Classroom consent, selected OAuth source/status
└── Fixed footer            Status/progress + Close + Start Setup / Try Again
```

At wide size those four peers appear in the sidebar. At narrow/high-scale size, the same peers appear in a segmented/tab control or compact category selector above a one-column scroll view. A search result, if implemented, opens the relevant category and focuses the exact setting; it never applies an install/erase/flash action directly.

### 5.6 Reproduce the useful patterns without copying Apple artwork or branding

- Use the generic word **Settings**, ordinary system concepts, semantic colors, and original ClockOS copy. Do not use Apple logos, Apple-ID wording, iOS/iPadOS screenshots, proprietary SF Symbols files, or Apple-specific illustration assets.
- Draw ClockOS’s simple outline glyphs directly with TFT primitives or use independently licensed/open-source icon geometry after license review. Keep the documented 1.5–2 px stroke and colored-tile system consistent rather than reproducing a proprietary symbol silhouette.
- The desired result comes from **information hierarchy, grouped rows, restrained color, state feedback, and accessible controls**—not from copying an exact pixel layout, font, or branded behavior.

---

## 6. Implementation order and visual acceptance criteria

### Implementation order

1. **Token layer first:** create a device `ThemeTokens` structure plus matching installer role names; settle the Crystal/default conflict; replace literals before changing page structure.
2. **Reusable components:** implement grouped surface, one row renderer, chevron, switch, icon tile, selected checkmark, header, and action button. Components receive tokens and a `Rect`.
3. **Settings-local route + rect tables:** render and hit-test from one source of truth. Keep persistence, calibration, Wi-Fi, SD, and other data behaviors unchanged.
4. **Device root and details:** ship General/Appearance/Calendar/Storage/System/Wi-Fi against the geometry above; normalize child-page returns.
5. **Installer shell:** responsive header/sidebar/content/footer; move common controls into setting rows; retain native widget semantics.
6. **Contrast and hardware pass:** test each shipped theme on panel and installer light/dark modes before release.

### Required visual/interaction acceptance tests

| Test | Pass condition |
|---|---|
| Device Settings root | Six categories fit with no clipping, each one uses its exact 456 × 42 rect, and blank margins/dividers do nothing. |
| Device detail | Standard rows are 456 × 50; Back returns to root; scanner/storage/theme/reset child routes return to their originating detail. |
| Theme parity | Crystal, Midnight, Ocean, Sunrise, and Graphite all use package-aligned page/surface/text colors; title, secondary text, separator, chevron, selected row, switch, and primary action remain legible. |
| Contrast | Enabled body text meets 4.5:1; action text uses the table’s approved pair; Ready/Needs setup/Not connected remains intelligible without color. |
| Touch | Test centers and boundaries of every row, Back, toggle, and action on calibrated hardware; 20 rapid taps and an intentional drag never produce an unintended Home navigation or double toggle. |
| Installer at 100–200% scale | Header, content and fixed footer remain visible; content may scroll but Start/Retry never clips. |
| Installer keyboard/failure state | Every input is focusable; focus ring is visible; failure/cancel restores all inputs and exposes **Try Again**. |
| Common workflow | Auto + optional Flash/Storage intent is intelligible before Start; destructive SD outcome is visible inline before confirmation. |

---

## 7. Audit conclusion

ClockOS v2.7 has Apple-inspired pieces—rounded controls, theme names, colored category icons, and a dark installer palette—but not yet a cohesive Apple Settings-like system. The principal visual blockers are **non-authoritative theme JSONs, hard-coded color exceptions, an undersized device sidebar, inconsistent row/control patterns, mismatched visual/touch rectangles, and an inflexible installer layout with an unprotected primary action**.

The compact specification above resolves those blockers without changing the panel orientation, touch calibration, settings data model, firmware feature set, or installer technology. The first release gate should be token parity plus the shared-row geometry; without those two foundations, rounded rectangles alone will continue to look like a custom control panel rather than ClockOS Settings.


---

## Implementation follow-up — ClockOSv2.8

The v2.8 implementation replaced the device sidebar with the six-category root and one-pane detail routes. It uses per-theme semantic page/surface/text/tint/selection/action/muted/separator tokens, contrast-qualified primary-action text, a subtle selected-theme fill, a separated destructive System row, non-disclosure status rows, and larger 46 × 28 switches. Wi-Fi and Storage hit handling was tightened to activate only the row that has a real destination; Settings remains locally navigable. The Windows installer is resizable with an outside-content action/progress footer, error retry, and a visible erase choice that defaults on when SD installation is enabled, as explicitly requested; the user can uncheck it and two volume-specific prompts remain.

The installer has **not** been fully refactored into the proposed four-category sidebar or segmented mode control, and OS light/high-contrast themes, keyboard focus rings, DPI testing, and hardware validation remain follow-up items. This is an implementation record, not a claim that every optional desktop pattern in this static design inventory is shipped. The final v2.8 ESP32 image passes the Arduino build, all 7 automated tests pass, and `esptool.py` reports a valid checksum/validation hash.
