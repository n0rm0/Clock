# Compiled firmware

Compiled firmware belongs directly in this directory as plain binary files. Do not create per-version `BootloaderV...` or `UpdateV...` folders here.

Expected examples are:

```text
.source/compiled/clockV1.00.bin
.source/compiled/bootloaderV1.00.bin
```

No compiled binaries were included in the uploaded archive, so none are fabricated or marked as ready. Add a `.bin` only after a successful Arduino build and record the board package, partition scheme, and build command.
