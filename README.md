# Clock / ClockOS

ESP32 clock project for the Hosyond 4-inch ESP32-32E display.

**ClockOSV1** remains the stable legacy release. The canonical same-name
`ClockOSV1/` source/binary alias is published alongside the preserved `updateV1`
compatibility slot, so older SD-card and OTA paths keep working. **ClockOSv2.6** is the current compiled release candidate: it has a
same-name Arduino sketch folder, matching raw-source tree, verified application
binary, and release manifest. See [the release policy](.source/releases/README.md)
before changing a version.

## Repository layout

Raw source and compiled firmware use matching versioned paths. Each Arduino sketch folder has the same name as its `.ino` file:

```text
.source/
├── compiled/                 # only verified .bin files
│   ├── bootloader/fallback/bootloaderV1/
│   └── updates/
│       ├── updateV1/ClockOSV1.bin  # legacy slot
│       ├── ClockOSV1/ClockOSV1.bin
│       ├── ClockOSv2.5/ClockOSv2.5.bin
│       └── ClockOSv2.6/ClockOSv2.6.bin
├── install/                  # GitHub-downloaded launcher and installer
├── touch_test/               # standalone raw-touch coordinate sketch
│   └── touch_test.ino
└── uncompiled/               # raw files for download and editing
    ├── bootloader/fallback/bootloaderV1/
    │   └── bootloaderV1.ino
    └── updates/
        ├── updateV1/updateV1.ino  # legacy-compatible sketch
        ├── ClockOSV1/ClockOSV1.ino
        ├── ClockOSv2.5/ClockOSv2.5.ino
        └── ClockOSv2.6/ClockOSv2.6.ino
```

The clock fetches the highest-version update from `.source/compiled/updates/`. Wi-Fi onboarding completes and saves settings before any automatic OTA check, so an update reboot cannot interrupt the first-run wizard. If an earlier firmware interrupted setup after Wi-Fi was saved, V2.6 resumes using those credentials. ClockOS
shows an Apple-style Software Update card while checking and installing, with a
rounded progress bar, release identity, status text, and safe failure fallback.
The `.bat` launcher downloads `.source/install/install.py` from GitHub and
installs user-scoped Python with `winget` when Python is missing. The Python
installer scans the repository and selects the highest-version same-name raw clock
source (`ClockOSv*` or legacy `updateV*`), then downloads/converts Meteocons
weather icons.

The installer has three modes. **Auto (recommended)** downloads and flashes only the newest compiled clock application `.bin` from `.source/compiled/updates/`; it never downloads the fallback bootloader. **Beta (unstable)** downloads the newest raw update sketch and compiles only that clock application, rather than stale `clock` or bootloader folders left on the computer. **Manual** lets you select a folder containing the `.ino` and `.h` files to compile, including a bootloader sketch only when explicitly selected. Successful compile-only clock binaries are saved as `Downloads/ClockBuilds/<sketch>.bin`.

When flashing a compiled sketch, the installer shows a visible **Flash device** selector with refresh, then asks which firmware target to upload when multiple sketches are available. The port list excludes Windows drive letters and shows detected board names such as `ESP32 Dev Module`. Build errors display cleaned compiler output instead of only an abbreviated ANSI log.

Raw sketches installed to an SD card are placed under `.source/uncompiled/updates/<version>/` or `.source/uncompiled/bootloader/fallback/<version>/`; no root-level `updateV1` folder or `extras` folder is created.

The compiled tree contains only real `.bin` files. `ClockOSv2.6.bin` was built
with Arduino CLI, ESP32 core 2.0.17, TFT_eSPI, PNGdec, and ArduinoJson; its
matching raw source is published under the same-name `ClockOSv2.6/` folder. The installer and OTA updater choose the highest semantic-version valid application source or binary (so v2.10 outranks v2.9); neither automatically selects a bootloader or pins to an older manifest. See
[`.source/releases/ClockOSv2.6.md`](.source/releases/ClockOSv2.6.md) and
[`.agent/todo.md`](.agent/todo.md) for the validation record and remaining
physical-device checks.

ClockOSv2.6 uses display rotation `3` (180°) with the supplied TFT_eSPI touch
calibration `{343, 3436, 266, 3381, 1}`. It does not overwrite that mapping from
the SD card. Physical corner/center calibration and final orientation checks
remain open; the standalone touch test is available at
[`.source/touch_test/touch_test.ino`](.source/touch_test/touch_test.ino).

After Wi-Fi connects, the clock requests current conditions from Open-Meteo using Philadelphia by default; manual city selection remains available. It does not use ISP IP geolocation, which can incorrectly place a clock in a distant city. Weather is laid out as icon, temperature, and city, without a long condition label. Meteocons PNGs are loaded from `/.source/icons/`, with a built-in vector symbol drawn underneath so the icon remains visible if assets or PNG decoding fail.

The home screen is interactive: tap the weather panel for location settings and city entry, the Wi-Fi icon for network management, the calendar panel for the extended calendar, and the battery/settings area for preferences. Settings now use an Apple/iPad-style sidebar with colored category icons, grouped rows, and switch controls. The display sleep routine explicitly drives the active-high GPIO27 backlight low and restores it on wake. System labels use a smaller FreeSans scale while the seven-segment clock remains large. A newly computer-formatted SD card is accepted and ClockOS creates its own required folders automatically without erasing unrelated files; the status indicator distinguishes card presence from readiness. The date is shown as `DD/MM/YYYY`; tapping it toggles 12/24-hour time. Offline startup opens a manual date/time editor because the ESP32 cannot recover the clock after a power loss without network time or an RTC.

Preferences are stored as JSON only when an SD card is detected, under
`/data/preferences/` (`settings.json`, `alarm.json`, `wifi.json`, and
`touch.json`). If no SD card is inserted, ClockOS does not save preferences;
the current session continues with in-memory defaults. Classroom secrets and
the read-only cache remain under the protected `/data/secrets/` area.
The SD-card icon is filled for a valid ClockOS card, outline-only when no card
is present, and marked with an X when a card is inserted but not prepared.
Settings > SD Card can prepare the ClockOS folder layout, preserve current
settings, show capacity/used space, and open a read-only root file viewer.
The prepare action does not erase unrelated files; weather icons are installed
by the Windows setup tool.

ClockOSV1 also includes an **Appearance** tab in Settings. Built-in theme
packages live under `.source/themes/appearance/` and are installed to the same
location on the SD card. The initial catalog includes **Crystal**, **Midnight**,
**Ocean**, **Sunrise**, and **Graphite**. Each theme is a small versioned JSON
package, and the installer downloads the catalog so additional themes can be
added in future releases without changing the SD-card layout.

The Windows installer opens with a small branded **ClockOS Setup — Loading,
please wait…** dialog and an animated progress bar while the main setup window
is initialized. This makes the hidden batch launcher feel intentional rather
than appearing to run an unknown background process.

For a completely console-free launch, double-click `setup_sd.vbs` instead of
the `.bat` file. Windows necessarily creates a console host when Explorer
directly opens a `.bat`, so a tiny black flash cannot be eliminated from the
`.bat` entry point itself. The VBScript launcher starts the same batch through
its hidden path and shows only the ClockOS Setup dialog. It works both beside
`setup_sd.bat` and when downloaded by itself: if the batch is not beside it,
the VBScript launcher quietly downloads the signed repository copy first.

The Windows bootstrap supports **Windows 10 and Windows 11** on x64, x86, and
ARM64. It uses `winget` when available, but Windows 10 installations without
the App Installer/`winget` package automatically download the matching official
Python 3.12 installer from python.org instead. No Windows 11-only API is
required.

ClockOSV1 uses a one-time Apple-style setup flow after a fresh install or
Factory Reset. It asks whether Wi-Fi should be enabled, whether Google
Classroom should be enabled, and whether the display is a **Main** or **Side**
device for display synchronization. Up to five Wi-Fi networks are stored in
`wifi.json` and tried in order. The sync compatibility rule accepts the same
major version with a minor difference of at most `0.1`; incompatible displays
are warned and must not be paired.

The profile control uses the supplied default Apple-style silhouette. Google
authorization remains performed by the Windows Clock Setup flow; the OAuth
client is stored on the SD card as `/data/secrets/classroomsecret.json`, with
the token and cache alongside it. Before a destructive SD wipe, the installer
copies all existing `.json` files aside and restores them afterward. Google may
still display an organization-review or administrator approval message; that
restriction must be resolved in Google Cloud/Workspace and cannot be bypassed
by ClockOS.

Enable **Google Classroom notifications** in the installer and choose the Desktop OAuth JSON from Google Cloud. The installer also checks common user folders (`Downloads`, `Documents`, `Desktop`, and the installer folder) for OAuth-shaped `.json` files and asks **“Is this the Google OAuth secrets file for Clock?”** before selecting one. It opens Google sign-in **before erasing or installing the SD card**; choose the school account, approve read-only Classroom/Calendar access, and the installer saves the private client/token plus `classroom_cache.json` under `.source/data/secrets/`. Do not commit those files to GitHub. The cache contains active courses, published assignments/projects, announcements, submission status data, and upcoming Google Calendar events. The firmware reads the cache at startup and refreshes the displayed assignment summary every second; run the installer again to perform a new Google sync.

The home screen shows the nearest cached assignment or project and due date. The extended calendar lists the next cached items. The display sleeps after one minute without a touch and wakes on touch. Settings includes a two-tap **Factory reset** button that removes saved settings, Wi-Fi credentials, Classroom tokens, and the cached Classroom data before restarting.
