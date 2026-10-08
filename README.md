# Clock

ESP32 clock project for the Hosyond 4-inch ESP32-32E display.

## Repository layout

The source and firmware trees intentionally have the same versioned folder structure:

```text
.source/
├── compiled/                 # clock update tree; compiled slots are currently empty
│   ├── bootloader/fallback/bootloader/bootloaderV0.00/
│   └── updates/updateV1/
└── uncompiled/               # raw files for download and editing
    ├── bootloader/fallback/bootloader/bootloaderV0.00/
    ├── updates/updateV1/
    │   ├── clock.ino
    │   └── config.h
    └── install/
```

The clock is configured to fetch the newest update from `.source/compiled/updates/`. The compiled slots contain only placeholder README files until real `.bin` files are produced by a successful Arduino build. The uploaded archive did not include compiled binaries, the bootloader module, or weather icons.

See [.agent/todo.md](.agent/todo.md) for the implementation checklist and known blockers.
