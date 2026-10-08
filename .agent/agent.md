# Clock Agent Guide

This directory holds lightweight project-operating notes for agents working on **Clock**. It is documentation only; it does not replace source code, a build, or hardware validation.

## Current baseline

- Default branch: `main`
- The repository now contains the cleaned archive under `.source/`.
- `.source/uncompiled/` contains raw editable files; `.source/compiled/` mirrors its versioned update and bootloader slots for firmware fetched by the clock.
- `.source/install/install.py` and `.source/install/setup_sd.bat` are the GitHub installer tools.
- The bootloader module is now present, but weather icons, compiled binaries, and a hardware-verified build are still pending.

## Intended project direction

The target hardware is a Hosyond 4-inch ESP32-32E display board with an ST7796S panel. The planned clock home screen is landscape (480 × 320) and includes:

- A small current-weather icon and outdoor temperature in the upper-right area.
- A large seven-segment clock and abbreviated weekday in the center.
- Date, indoor temperature, and humidity along the bottom.

The planned software uses Wi-Fi for weather, SD-card assets for weather icons, and a versioned update/bootloader layout. Pin assignments, display configuration, library choices, and board behavior must be verified against the actual board documentation and a physical build before being treated as final.

## Naming and layout conventions

When firmware is added, keep one Arduino entry sketch per version and put supporting modules in clearly named headers or implementation files. Avoid duplicate sketches that contain identical clock logic.

Use the version placeholder consistently:

```text
.source/
├── compiled/                 # mirrored update tree; slots await .bin files
│   ├── bootloader/fallback/bootloaderV<version>/
│   └── updates/updateV<version>/
└── uncompiled/               # raw files for download and editing
    ├── bootloader/fallback/bootloaderV<version>/
    ├── updates/updateV<version>/
    └── install/              # Windows launcher and Python installer
```

Guidelines:

1. Put `updateV<version>.ino` inside a same-name `updateV<version>/` folder, and put `bootloaderV<version>.ino` inside a same-name `bootloaderV<version>/` folder. Do not add a second `bootloader/` folder inside `fallback/`.
2. Name support modules for their responsibility, for example `weather.h`, `display.h`, or `bootloader.h`. Include them from the versioned `.ino` entry point.
3. Keep SD-card assets under `.source/`; do not mix installed SD payloads with source-only development files without documenting the reason.
4. Keep `compiled/` and `uncompiled/` as matching trees. Raw `.ino` files belong in `uncompiled`; compiled firmware belongs in the matching `compiled` slot. Do not add an extra version-folder layer. This is application-level update logic, not a replacement for the ESP32 ROM bootloader.

## SD-card setup tool

The planned Windows setup tool is a `.bat` launcher backed by Python. Its responsibilities are to:

- Prompt the user to select the target drive using Tkinter.
- Create matching `.source/uncompiled/` and `.source/compiled/` directory trees, plus `.source/install/` for the launcher.
- Leave compiled version slots without firmware until a build passes; never fabricate a binary.
- Create or copy required placeholder bootloader files only when appropriate.
- Download or copy weather icon assets into `.source/icons`.
- Install user-scoped Python with `winget` when possible and report clear errors for missing Python, inaccessible drives, failed downloads, or failed file copies.

Do not delete or overwrite a user's existing SD-card files without an explicit, documented opt-in.

## Implementation and validation rules

Before claiming firmware work is complete:

1. Inspect the existing project files rather than replacing them wholesale.
2. Build with the matching ESP32 board package, selected board, and required libraries.
3. Resolve compile errors from actual compiler output rather than guessing.
4. Test the SD-card setup tool on a disposable directory or removable test drive.
5. Keep configuration secrets (Wi-Fi credentials, API keys, location data) out of version control; use ignored local configuration or documented placeholders.
6. Update `.agent/todo.md` only when a task is genuinely complete and state what validation was performed.

## Agent workflow

- Read this file and `.agent/todo.md` before making scoped project changes.
- Keep commits focused and describe both the change and validation.
- Do not mark unimplemented design items as complete merely because they appeared in earlier chat history.
- If board pin mappings, library APIs, or OTA partition behavior are uncertain, flag the uncertainty and seek authoritative documentation or a build/test result.
