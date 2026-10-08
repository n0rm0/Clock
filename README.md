# Clock

ESP32 clock project for the Hosyond 4-inch ESP32-32E display.

## Repository layout

```text
.source/
├── compiled/                 # flat, built .bin files only
├── uncompiled/
│   ├── clock/                # active Arduino source
│   └── install/              # Windows launcher and Python installer
└── README.md
```

The uploaded source archive contained duplicate preview files and two implementation variants. The published layout keeps the active TFT_eSPI implementation and removes duplicate ` (1)` files, header copies of sketches, the unused incomplete `main.ino`, and the alternate LovyanGFX preview files.

Compiled firmware is not included yet because the archive did not contain `.bin` files. Add only binaries produced by a successful, reproducible Arduino build directly under `.source/compiled/`; do not create per-version `BootloaderV...` or `UpdateV...` folders.

See [.agent/todo.md](.agent/todo.md) for the implementation checklist and known blockers.
