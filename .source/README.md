# Clock source payload

The repository keeps raw editable files in `uncompiled/` and the firmware update tree in `compiled/`.

```text
.source/
├── compiled/                         # contains only .bin files when builds exist
│   ├── bootloader/fallback/bootloaderV1/
│   └── updates/
│       ├── updateV1/ClockOSV1.bin
│       └── ClockOSv2.5/ClockOSv2.5.bin
├── install/
│   ├── install.py
│   └── setup_sd.bat
└── uncompiled/
    ├── bootloader/fallback/bootloaderV1/
    │   ├── bootloaderV1.ino
    │   ├── bootloader.h
    │   └── config.h
    └── updates/updateV1/
        ├── updateV1.ino
        ├── bootloader.h
        └── config.h
    └── updates/ClockOSv2.5/
        ├── ClockOSv2.5.ino
        ├── bootloader.h
        └── config.h
```

Each Arduino sketch is in a same-name folder so Arduino IDE can open it directly. The installer recognizes both legacy `updateV*` and current `ClockOSv*` application folders, while excluding bootloader sketches from automatic selection. The compiled tree contains only verified application `.bin` files; release identities and paths are defined in `.source/releases/current.json`.
