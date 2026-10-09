# Clock

ESP32 clock project for the Hosyond 4-inch ESP32-32E display.

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

The clock fetches the newest update from `.source/compiled/updates/`. The `.bat` launcher downloads `.source/install/install.py` from GitHub and installs user-scoped Python with `winget` when Python is missing. The Python installer fetches the raw `updateV1` source and downloads/converts Meteocons weather icons.

The installer has three modes. **Auto (recommended)** downloads and flashes only the newest compiled clock application `.bin` from `.source/compiled/updates/`; it never downloads the fallback bootloader. **Beta (unstable)** downloads the newest raw update sketch and compiles only that clock application, rather than stale `clock` or bootloader folders left on the computer. **Manual** lets you select a folder containing the `.ino` and `.h` files to compile, including a bootloader sketch only when explicitly selected. Successful compile-only clock binaries are saved as `Downloads/ClockBuilds/<sketch>.bin`.

When flashing a compiled sketch, the installer shows a visible **Flash device** selector with refresh, then asks which firmware target to upload when multiple sketches are available. The port list excludes Windows drive letters and shows detected board names such as `ESP32 Dev Module`. Build errors display cleaned compiler output instead of only an abbreviated ANSI log.

Raw sketches installed to an SD card are placed under `.source/uncompiled/updates/<version>/` or `.source/uncompiled/bootloader/fallback/<version>/`; no root-level `updateV1` folder or `extras` folder is created.

The compiled tree must contain only real `.bin` files. No compiled binaries were included yet, so its empty directories are created when the installer prepares the SD card. See [.agent/todo.md](.agent/todo.md) for validation items.

The display is now set to rotation `3` (180°). The latest official TFT_eSPI calibration output `{365, 3431, 321, 3368, 7}` is hardwired into the clock firmware, so it uses that mapping directly and does not launch the calibration screen or overwrite it from the SD card. The standalone touch test remains available at [.source/touch_test/touch_test.ino](.source/touch_test/touch_test.ino) if the panel is replaced.
