# Clock source payload

The repository keeps raw editable files in `uncompiled/` and the firmware update tree in `compiled/`.

```text
.source/
├── compiled/                         # contains only .bin files when builds exist
│   ├── bootloader/fallback/bootloaderV1/
│   └── updates/updateV1/
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
```

Each Arduino sketch is in a same-name folder so Arduino IDE can open it directly. The compiled tree is intentionally empty until real `.bin` files are built; Git cannot preserve empty directories without adding a non-binary placeholder, so the directory layout is documented here and created by the installer when needed.
