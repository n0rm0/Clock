# Clock source payload

The repository keeps raw editable files in `uncompiled/` and verified application firmware in `compiled/`. Each Arduino sketch is in a same-name folder so Arduino IDE can open it directly.

```text
.source/
├── compiled/
│   ├── bootloader/fallback/bootloaderV1/
│   └── updates/
│       ├── updateV1/ClockOSV1.bin
│       ├── ClockOSV1/ClockOSV1.bin
│       ├── ClockOSv2.5/ClockOSv2.5.bin
│       ├── ClockOSv2.6/ClockOSv2.6.bin
│       ├── ClockOSv2.7/ClockOSv2.7.bin
│       ├── ClockOSv2.8/ClockOSv2.8.bin
│       ├── ClockOSv3.8/ClockOSv3.8.bin
│       └── ClockOSv3.9/ClockOSv3.9.bin
├── icons/                       # transparent PNG weather art and its license
├── install/
│   ├── install.py
│   └── setup_sd.bat
└── uncompiled/
    ├── bootloader/fallback/bootloaderV1/
    └── updates/
        ├── updateV1/
        ├── ClockOSV1/
        ├── ClockOSv2.5/
        ├── ClockOSv2.6/
        ├── ClockOSv2.7/
        ├── ClockOSv2.8/
        ├── ClockOSv3.8/
        └── ClockOSv3.9/
```

The installer recognizes legacy `updateV*` and current `ClockOSv*` application folders while excluding bootloader sketches from automatic selection. The compiled tree contains application `.bin` files only. Release identities and paths are defined in `.source/releases/current.json`; the v3.9 build record is `.source/releases/ClockOSv3.9.md`. Eight transparent 96 × 96 PNGs in `.source/icons/` are copied to the SD card's `/.source/icons/` folder by the installer.
