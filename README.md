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
└── uncompiled/               # raw files for download and editing
    ├── bootloader/fallback/bootloaderV1/
    │   └── bootloaderV1.ino
    └── updates/updateV1/
        └── updateV1.ino
```

The clock fetches the newest update from `.source/compiled/updates/`. The `.bat` launcher downloads `.source/install/install.py` from GitHub and installs user-scoped Python with `winget` when Python is missing. The Python installer fetches the raw `updateV1` source and downloads/converts Meteocons weather icons.

The compiled tree must contain only real `.bin` files. No compiled binaries were included yet, so its empty directories are created when the installer prepares the SD card. See [.agent/todo.md](.agent/todo.md) for validation items.
