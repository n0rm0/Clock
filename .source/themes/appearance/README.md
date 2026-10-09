# ClockOS Appearance Themes

Built-in appearance packages for ClockOSV1. Each theme is a small JSON file so future releases can add themes without changing the SD-card layout.

Install location on the SD card:

```text
.source/themes/appearance/
```

Theme fields:

- `id`: stable identifier
- `name`: display name
- `version`: theme package version
- `background`, `surface`, `primary`, `text`, `muted`, `accent`: RGB hex colors
- `style`: visual family name

The firmware currently ships with Crystal as the default visual family. The other packages are ready for the Appearance selector and future downloadable theme updates.
