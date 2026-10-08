# Compiled firmware update tree

This mirrors the versioned source tree. The clock checks `.source/compiled/updates/` for newer firmware, while the fallback bootloader slot is kept separately.

The slots are intentionally empty of firmware until a build succeeds:

```text
compiled/
├── bootloader/fallback/bootloader/bootloaderV0.00/
└── updates/updateV1/
```

The README files inside the slots are Git placeholders, not firmware. Put only the resulting `.bin` files in the corresponding compiled slot; do not add another version-folder layer.
