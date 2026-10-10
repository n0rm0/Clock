import ast
import os
import re
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INSTALLER_PATH = ROOT / ".source" / "install" / "install.py"
FIRMWARE_PATH = ROOT / ".source" / "uncompiled" / "updates" / "ClockOSv2.7" / "ClockOSv2.7.ino"
CONFIG_PATH = ROOT / ".source" / "uncompiled" / "updates" / "ClockOSv2.7" / "config.h"


def load_installer_layout():
    """Load only the pure SD-layout constant and function, without importing Tk."""
    tree = ast.parse(INSTALLER_PATH.read_text(encoding="utf-8"))
    selected = []
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
            isinstance(target, ast.Name) and target.id == "CLOCKOS_SD_DIRECTORIES"
            for target in node.targets
        ):
            selected.append(node)
        elif isinstance(node, ast.FunctionDef) and node.name == "structure":
            selected.append(node)
    namespace = {"os": os}
    exec(compile(ast.Module(body=selected, type_ignores=[]), str(INSTALLER_PATH), "exec"), namespace)
    return namespace


def firmware_sd_directories():
    source = FIRMWARE_PATH.read_text(encoding="utf-8")
    config = CONFIG_PATH.read_text(encoding="utf-8")
    macros = {
        name: value
        for name, value in re.findall(r'^\s*#define\s+(\w+)\s+"([^"]+)"', config, re.MULTILINE)
    }
    match = re.search(
        r"CLOCKOS_SD_DIRECTORIES\[\]\s*=\s*\{(.*?)\};",
        source,
        re.DOTALL,
    )
    if not match:
        raise AssertionError("Firmware SD directory list was not found")
    entries = []
    for token in match.group(1).split(","):
        token = token.strip()
        if not token:
            continue
        if token.startswith('"') and token.endswith('"'):
            entries.append(token[1:-1].lstrip("/"))
        else:
            if token not in macros:
                raise AssertionError(f"Unknown firmware SD path macro: {token}")
            entries.append(macros[token].lstrip("/"))
    return tuple(entries)


class SdLayoutParityTests(unittest.TestCase):
    def test_firmware_and_windows_installer_share_sd_layout(self):
        installer = load_installer_layout()
        self.assertEqual(tuple(installer["CLOCKOS_SD_DIRECTORIES"]), firmware_sd_directories())

    def test_windows_setup_creates_layout_without_erasing_existing_files(self):
        installer = load_installer_layout()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            unrelated = root / "keep-this.txt"
            unrelated.write_text("user data", encoding="utf-8")
            installer["structure"](str(root))
            expected = set(installer["CLOCKOS_SD_DIRECTORIES"])
            actual = {
                path.relative_to(root).as_posix()
                for path in root.rglob("*")
                if path.is_dir()
            }
            self.assertEqual(actual, expected)
            self.assertEqual(unrelated.read_text(encoding="utf-8"), "user data")


if __name__ == "__main__":
    unittest.main()
