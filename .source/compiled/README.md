# Compiled firmware update tree

The clock checks `.source/compiled/updates/` for newer firmware. This tree mirrors the raw source tree without adding another nested version folder:

```text
compiled/
├── bootloader/fallback/bootloaderV0.00/
└── updates/updateV1/
```

The slots are intentionally empty of firmware until a build succeeds. README files inside the slots are Git placeholders, not firmware. Put the resulting `.bin` file in the matching slot after compilation.
