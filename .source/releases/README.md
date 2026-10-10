# ClockOS release policy

`current.json` is the repository's release manifest. The firmware sketch, its same-name Arduino folder, the matching raw-source folder, the compiled binary location, and the installer label must use the same release identity.

| Change type | Increment | Example |
| --- | --- | --- |
| Small fix | `+0.1` | `v2.7` → `v2.8` |
| Major feature release | `+1.0` | `v2.7` → `v3.7` |

Before publishing a release:

1. Update `current.json` and add a versioned copy such as `ClockOSv2.7.json`.
2. Create `.source/uncompiled/updates/<firmwareIdentity>/<firmwareIdentity>.ino` with its supporting headers.
3. Build that same sketch and place only the verified `.bin` at the manifest's `binaryPath`.
4. Keep the previous stable release intact so the installer and OTA updater have a known-good fallback.
5. Verify the installer selects the maximum semantic version from all same-name application sources and binaries, regardless of the current manifest target; bootloader folders are excluded.

The application updater treats this as a ClockOS application release. It does not replace the ESP32 ROM bootloader.
