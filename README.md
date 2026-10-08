# Clock

ESP32 clock project for the Hosyond 4-inch ESP32-32E display.

## Repository layout

The source and firmware trees use the same versioned folder structure, and each Arduino sketch folder has the same name as its `.ino` file:

```text
.source/
├── compiled/                 # clock update tree; slots are empty until builds pass
│   ├── bootloader/fallback/bootloaderV0.00/
│   └── updates/updateV1/
├── install/                  # GitHub-downloaded launcher and installer
└── uncompiled/               # raw files for download and editing
    ├── bootloader/fallback/bootloaderV0.00/
    │   └── bootloaderV0.00.ino
    └── updates/updateV1/
        └── updateV1.ino
```

The clock fetches the newest update from `.source/compiled/updates/`. The `.bat` launcher downloads `.source/install/install.py` from GitHub and installs user-scoped Python with `winget` when Python is missing. The Python installer fetches the raw `updateV1` source and downloads/converts Meteocons weather icons.

Compiled slots contain only README placeholders until real `.bin` files are produced by a successful Arduino build. See [.agent/todo.md](.agent/todo.md) for known validation items.
