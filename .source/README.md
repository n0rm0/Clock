# Clock source payload

The repository intentionally keeps **two copies of the same versioned layout**:

- `uncompiled/` contains raw editable `.ino` and supporting files for download and modification.
- `compiled/` is the update tree the clock checks for newer firmware binaries.

```text
.source/
├── compiled/
│   ├── bootloader/fallback/bootloader/bootloaderV0.00/
│   └── updates/updateV1/
└── uncompiled/
    ├── bootloader/fallback/bootloader/bootloaderV0.00/
    ├── updates/updateV1/
    │   ├── clock.ino
    │   └── config.h
    └── install/
```

The compiled version slots currently contain no firmware binaries. Their small README files only keep the empty slots visible in Git; replace them with real `.bin` files after a successful build. The uploaded archive did not contain `bootloader.h`, weather icons, or compiled binaries.
