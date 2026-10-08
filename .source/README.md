# Clock source payload

The repository keeps editable source and compiled firmware separate:

- `uncompiled/clock/` contains the active Arduino sketch and configuration.
- `uncompiled/install/` contains the Windows launcher and Python installer.
- `compiled/` contains only flat, successfully built `.bin` files. Do not add `BootloaderV...` or `UpdateV...` subfolders there.

The uploaded archive did not contain `bootloader.h`, weather icons, or compiled binaries. The active sketch currently includes `bootloader.h`, so it is not compile-ready until that module is supplied or the include is removed as part of the update implementation.
