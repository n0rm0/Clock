# Clock source payload

The repository keeps two copies of the same versioned layout:

- `uncompiled/` contains raw editable `.ino`, `.h`, and configuration files.
- `compiled/` is the firmware tree checked by the clock for newer update binaries.
- `install/` contains the GitHub-downloaded Windows launcher and Python installer.

```text
.source/
├── compiled/
│   ├── bootloader/fallback/bootloaderV0.00/
│   └── updates/updateV1/
├── install/
│   ├── install.py
│   └── setup_sd.bat
└── uncompiled/
    ├── bootloader/fallback/bootloaderV0.00/
    │   ├── bootloaderV0.00.ino
    │   ├── bootloader.h
    │   └── config.h
    └── updates/updateV1/
        ├── updateV1.ino
        ├── bootloader.h
        └── config.h
```

Each Arduino sketch is in a same-name folder so Arduino IDE can open it directly. The compiled slots currently contain no firmware binaries; their README files only keep the empty slots visible in Git. Add a real `.bin` after a successful build. The original archive did not include weather icons or compiled binaries.
