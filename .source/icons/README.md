# Weather icon assets

These transparent 96 × 96 PNGs are the exact SD-card assets used by ClockOSv3.9. The installer copies them to `/.source/icons/`; the ESP32 firmware decodes the matching PNG for the current Open-Meteo condition. If a PNG is absent or corrupt, firmware reports that state and draws a built-in fallback instead.

Artwork: [Bas Milius / weather-icons](https://github.com/basmilius/weather-icons), MIT License. The license text is in `LICENSE.txt`.

Included firmware mappings: `clear-day`, `partly-cloudy-day`, `fog`, `drizzle`, `rain`, `snow`, `thunderstorms`, and `cloudy`.
