# Clock / ClockOS

ESP32 clock project for the Hosyond 4-inch ESP32-32E display.

The current firmware iteration is **ClockOS 3.1**. The Arduino compatibility
folder remains `updateV1` so existing SD-card and installer workflows continue
to work; release binaries are versioned as ClockOS artifacts inside that slot.

## Repository layout

Raw source and compiled firmware use matching versioned paths. Each Arduino sketch folder has the same name as its `.ino` file:

```text
.source/
├── compiled/                 # only .bin files; currently no binaries supplied
│   ├── bootloader/fallback/bootloaderV1/
│   └── updates/updateV1/
├── install/                  # GitHub-downloaded launcher and installer
├── touch_test/               # standalone raw-touch coordinate sketch
│   └── touch_test.ino
└── uncompiled/               # raw files for download and editing
    ├── bootloader/fallback/bootloaderV1/
    │   └── bootloaderV1.ino
    └── updates/updateV1/
        └── updateV1.ino
```

The clock fetches the newest update from `.source/compiled/updates/`. ClockOS
shows an Apple-style Software Update card while checking and installing, with a
rounded progress bar, release identity, status text, and safe failure fallback.
The `.bat` launcher downloads `.source/install/install.py` from GitHub and
installs user-scoped Python with `winget` when Python is missing. The Python
installer fetches the raw `updateV1` source and downloads/converts Meteocons
weather icons.

The installer has three modes. **Auto (recommended)** downloads and flashes only the newest compiled clock application `.bin` from `.source/compiled/updates/`; it never downloads the fallback bootloader. **Beta (unstable)** downloads the newest raw update sketch and compiles only that clock application, rather than stale `clock` or bootloader folders left on the computer. **Manual** lets you select a folder containing the `.ino` and `.h` files to compile, including a bootloader sketch only when explicitly selected. Successful compile-only clock binaries are saved as `Downloads/ClockBuilds/<sketch>.bin`.

When flashing a compiled sketch, the installer shows a visible **Flash device** selector with refresh, then asks which firmware target to upload when multiple sketches are available. The port list excludes Windows drive letters and shows detected board names such as `ESP32 Dev Module`. Build errors display cleaned compiler output instead of only an abbreviated ANSI log.

Raw sketches installed to an SD card are placed under `.source/uncompiled/updates/<version>/` or `.source/uncompiled/bootloader/fallback/<version>/`; no root-level `updateV1` folder or `extras` folder is created.

The compiled tree must contain only real `.bin` files. The ClockOS 2.1 binary is
added only after a successful Arduino CLI build. See [.agent/todo.md](.agent/todo.md)
for validation items.

The display is now set to rotation `3` (180°). The latest official TFT_eSPI calibration output `{365, 3431, 321, 3368, 7}` is hardwired into the clock firmware, so it uses that mapping directly and does not launch the calibration screen or overwrite it from the SD card. The standalone touch test remains available at [.source/touch_test/touch_test.ino](.source/touch_test/touch_test.ino) if the panel is replaced.

After Wi-Fi connects, the clock obtains an approximate location from the network's public IP and requests current conditions from Open-Meteo. Weather icons are loaded from `/.source/icons/`; if an icon is missing, the firmware draws a visible fallback icon instead of showing a blank panel. Tapping the clock/time panel opens the alarm editor, where you can set any time, date, one-time alarm, or repeating days, then save or disable it. Active alarms can be dismissed by tapping the screen.

The home screen is now interactive: tap the weather panel for automatic/manual location selection and city entry, the Wi-Fi icon for network management/offline mode, the calendar panel for the extended calendar view, and the battery/settings area for preferences. The date is shown as `DD/MM/YYYY`; tapping the date toggles 12/24-hour time. Offline startup opens a manual date/time editor because the ESP32 cannot recover the clock after a power loss without network time or an RTC.

Preferences are stored as JSON only when an SD card is detected, under
`/data/preferences/` (`settings.json`, `alarm.json`, `wifi.json`, and
`touch.json`). If no SD card is inserted, ClockOS does not save preferences;
the current session continues with in-memory defaults. Classroom secrets and
the read-only cache remain under the protected `.source/data/secrets/` area.
The SD-card icon is filled for a valid ClockOS card, outline-only when no card
is present, and marked with an X when a card is inserted but not prepared.
Settings > SD Card can prepare the ClockOS folder layout, preserve current
settings, show capacity/used space, and open a read-only root file viewer.
The prepare action does not erase unrelated files; weather icons are installed
by the Windows setup tool.

ClockOS 3.1 also includes an **Appearance** tab in Settings. Built-in theme
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
its hidden path and shows only the ClockOS Setup dialog.

ClockOS 3.1 uses a one-time Apple-style setup flow after a fresh install or
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
