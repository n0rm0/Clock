# Clock Agent Guide

This directory holds lightweight project-operating notes for agents working on **Clock**. It is documentation only; it does not replace source code, a build, or hardware validation.

## Current baseline

- Default branch: `main`
- The repository now contains the cleaned archive under `.source/`.
- `.source/uncompiled/clock/` is the editable Arduino source; `.source/uncompiled/install/` contains the installer; `.source/compiled/` is reserved for flat `.bin` files.
- The imported source is not yet compile-ready because the archive did not include `bootloader.h`, weather icons, or compiled binaries.

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
├── compiled/                 # flat .bin files only; no version subfolders
└── uncompiled/
    ├── clock/                # active Arduino source
    └── install/              # Windows launcher and Python installer
```

Guidelines:

1. Name the primary application sketch `V<version>.ino` when it is the active build, or `updateV<version>.ino` when stored as an SD update artifact. Do not leave an extra `clock.ino` copy unless it is intentionally a separate sketch.
2. Name support modules for their responsibility, for example `weather.h`, `display.h`, or `bootloader.h`. Include them from the versioned `.ino` entry point.
3. Keep SD-card assets under `.source/`; do not mix installed SD payloads with source-only development files without documenting the reason.
4. Keep compiled firmware as plain `.bin` files directly under `.source/compiled/`. Do not create `BootloaderV...` or `UpdateV...` folders there. Any fallback/version policy must be encoded in the filename and update logic. This is application-level update logic, not a replacement for the ESP32 ROM bootloader.

## SD-card setup tool

The planned Windows setup tool is a `.bat` launcher backed by Python. Its responsibilities are to:

- Prompt the user to select the target drive using Tkinter.
- Create the `.source/uncompiled/` and `.source/compiled/` directory tree.
- Keep compiled firmware as flat `.bin` files and never fabricate a binary when a build has not passed.
- Create or copy required placeholder bootloader files only when appropriate.
- Download or copy weather icon assets into `.source/icons`.
- Report clear errors for missing Python, inaccessible drive letters, failed downloads, or failed file copies.

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
