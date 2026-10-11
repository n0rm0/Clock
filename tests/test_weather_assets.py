import ast
import os
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INSTALLER = ROOT / ".source" / "install" / "install.py"
ICON_DIR = ROOT / ".source" / "icons"
REQUIRED_ICONS = ("clear-day", "partly-cloudy-day", "fog", "drizzle", "rain", "snow", "thunderstorms", "cloudy")
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def decode_rgba_png(data):
    """Decode the small non-interlaced RGBA PNGs to check their visible pixels."""
    if data[:8] != PNG_SIGNATURE:
        raise ValueError("not a PNG")
    offset = 8
    compressed = bytearray()
    width = height = bit_depth = color_type = interlace = None
    while offset < len(data):
        length = int.from_bytes(data[offset:offset + 4], "big")
        kind = data[offset + 4:offset + 8]
        content = data[offset + 8:offset + 8 + length]
        if len(content) != length:
            raise ValueError("truncated PNG chunk")
        if kind == b"IHDR":
            width, height, bit_depth, color_type, compression, filtering, interlace = struct.unpack(">IIBBBBB", content)
            if bit_depth != 8 or color_type != 6 or compression != 0 or filtering != 0 or interlace != 0:
                raise ValueError("expected non-interlaced 8-bit RGBA PNG")
        elif kind == b"IDAT":
            compressed.extend(content)
        offset += length + 12
        if kind == b"IEND":
            break
    if not width or not height:
        raise ValueError("missing PNG header")
    raw = zlib.decompress(compressed)
    stride = width * 4
    rows = []
    prior = bytearray(stride)
    cursor = 0
    for _ in range(height):
        filter_type = raw[cursor]
        cursor += 1
        encoded = raw[cursor:cursor + stride]
        cursor += stride
        row = bytearray(stride)
        for i, value in enumerate(encoded):
            left = row[i - 4] if i >= 4 else 0
            above = prior[i]
            upper_left = prior[i - 4] if i >= 4 else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = above
            elif filter_type == 3:
                predictor = (left + above) // 2
            elif filter_type == 4:
                p = left + above - upper_left
                pa, pb, pc = abs(p - left), abs(p - above), abs(p - upper_left)
                predictor = left if pa <= pb and pa <= pc else above if pb <= pc else upper_left
            else:
                raise ValueError(f"unsupported PNG filter {filter_type}")
            row[i] = (value + predictor) & 0xFF
        rows.append(bytes(row))
        prior = row
    return width, height, rows


def load_icon_installer(fake_http_get, progress_log):
    tree = ast.parse(INSTALLER.read_text(encoding="utf-8"))
    selected = []
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
            isinstance(target, ast.Name) and target.id in {"WEATHER_ICONS", "WEATHER_ASSET_BASE"}
            for target in node.targets
        ):
            selected.append(node)
        elif isinstance(node, ast.FunctionDef) and node.name == "icons":
            selected.append(node)
    namespace = {
        "OWNER": "n0rm0",
        "REPO": "Clock",
        "BRANCH": "main",
        "os": os,
        "http_get": fake_http_get,
        "prog": lambda value, message: progress_log.append((value, message)),
    }
    exec(compile(ast.Module(body=selected, type_ignores=[]), str(INSTALLER), "exec"), namespace)
    return namespace


class WeatherAssetTests(unittest.TestCase):
    def test_ipados_style_settings_expose_descriptive_controls_and_live_values(self):
        firmware = (ROOT / ".source/uncompiled/updates/ClockOSv3.9/ClockOSv3.9.ino").read_text(encoding="utf-8")
        for text in ("Settings", "General", "Wi-Fi", "Appearance", "Calendar", "Storage", "System",
                     "Battery percentage", "Turn display off", "Weather & location", "Weather PNG",
                     "Current wireless connection.", "Create folders needed for settings and weather icons."):
            self.assertIn(text, firmware)
        self.assertIn('doc["displaySleep"] = settings.displaySleep;', firmware)
        self.assertIn('settings.displaySleep = doc["displaySleep"] | true;', firmware)
        self.assertIn('else if (row == 3) settings.displaySleep = !settings.displaySleep;', firmware)
        self.assertIn("const uint8_t order[] = {0, 2, 3, 1, 4, 5};", firmware)
        self.assertIn("visual = (y - 52) / SETTINGS_ROOT_ROW_H;", firmware)

    def test_idle_backlight_timeout_and_page_restore_are_not_home_only(self):
        firmware = (ROOT / ".source/uncompiled/updates/ClockOSv3.9/ClockOSv3.9.ino").read_text(encoding="utf-8")
        self.assertIn("current == S_HOME || current == S_SETTINGS || current == S_WIFI", firmware)
        self.assertIn("settings.displaySleep && idleSleepEligible(screen) && !alarmRinging", firmware)
        self.assertIn("restoreAfterWake();", firmware)
        self.assertIn("digitalWrite(TFT_BACKLIGHT_PIN, enabled ? HIGH : LOW);", firmware)

    def test_required_weather_pngs_are_transparent_and_visibly_rasterized(self):
        firmware = (ROOT / ".source/uncompiled/updates/ClockOSv3.9/ClockOSv3.9.ino").read_text(encoding="utf-8")
        assets = sorted(ICON_DIR.glob("*.png"))
        self.assertEqual(len(assets), len(REQUIRED_ICONS))
        for name in REQUIRED_ICONS:
            data = (ICON_DIR / f"{name}.png").read_bytes()
            self.assertEqual(data[:8], PNG_SIGNATURE)
            width, height, rows = decode_rgba_png(data)
            self.assertEqual((width, height), (96, 96), name)
            self.assertEqual(rows[0][:4], b"\x00\x00\x00\x00", f"{name} should retain transparency")
            opaque = [(row[i], row[i + 1], row[i + 2], row[i + 3])
                      for row in rows for i in range(0, len(row), 4) if row[i + 3] > 220]
            bright = sum(1 for red, green, blue, _ in opaque if red + green + blue > 360)
            self.assertGreater(len(opaque), 400, name)
            self.assertGreaterEqual(bright / len(opaque), 0.15, f"{name} may have black-filled SVG symbols")
            self.assertIn(f'return "{name}";', firmware)

    def test_installer_copies_exact_validated_pngs_into_sd_layout(self):
        assets = {path.stem: path.read_bytes() for path in ICON_DIR.glob("*.png")}
        requested = []
        progress = []

        def fake_http_get(url, timeout):
            self.assertEqual(timeout, 20)
            name = url.rsplit("/", 1)[-1][:-4]
            requested.append(name)
            return assets[name]

        installer = load_icon_installer(fake_http_get, progress)
        with tempfile.TemporaryDirectory() as temporary:
            count, failed = installer["icons"](temporary, 20, 52)
            installed = Path(temporary) / ".source" / "icons"
            self.assertEqual((count, failed), (len(REQUIRED_ICONS), []))
            self.assertEqual(set(requested), set(assets))
            for name, data in assets.items():
                self.assertEqual((installed / f"{name}.png").read_bytes(), data)
        self.assertEqual(len(progress), len(REQUIRED_ICONS))
        self.assertTrue(all("weather PNGs" in message for _, message in progress))

    def test_installer_rejects_non_png_or_wrong_dimensions(self):
        progress = []
        installer = load_icon_installer(lambda _url, _timeout: b"not png", progress)
        with tempfile.TemporaryDirectory() as temporary:
            count, failed = installer["icons"](temporary, 20, 52)
            self.assertEqual(count, 0)
            self.assertEqual(len(failed), len(REQUIRED_ICONS))
            self.assertEqual(list((Path(temporary) / ".source" / "icons").glob("*.png")), [])


if __name__ == "__main__":
    unittest.main()
