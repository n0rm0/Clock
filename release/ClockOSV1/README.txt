ClockOSV1 release assets
========================

Windows:
- ClockOSV1-setup-windows-x64.vbs
- ClockOSV1-setup-windows-x86.vbs
- ClockOSV1-setup-windows-arm64.vbs

The Windows VBS launchers are architecture-independent and use the installed
Windows Python/runtime automatically. They download the current hidden batch
launcher when run by themselves and show only the ClockOSV1 setup dialog.

Portable:
- ClockOSV1-portable.zip

macOS:
- ClockOSV1-setup-macos.command

The macOS command launcher requires Python 3 and Tkinter. The current SD-card
selection and ESP32 flashing workflow is Windows-first; macOS users can use
the runner for source/setup workflows and should use a Windows machine for
removable-drive flashing if the macOS environment does not expose the device.
