import ast
import hashlib
import json
import os
import shutil
import stat
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
INSTALLER = ROOT / ".source" / "install" / "install.py"


def load_sd_helpers():
    tree = ast.parse(INSTALLER.read_text(encoding="utf-8"))
    selected = []
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(
            isinstance(target, ast.Name) and target.id == "CLOCKOS_SD_DIRECTORIES"
            for target in node.targets
        ):
            selected.append(node)
        elif isinstance(node, ast.FunctionDef) and node.name in {
            "_rm", "wipe_card_contents", "structure"
        }:
            selected.append(node)
    namespace = {"os": os, "shutil": shutil, "stat": stat}
    exec(compile(ast.Module(body=selected, type_ignores=[]), str(INSTALLER), "exec"), namespace)
    return namespace


class InstallerEraseAndReleaseTests(unittest.TestCase):
    def test_explicit_erase_removes_mixed_files_and_recreates_layout(self):
        helpers = load_sd_helpers()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "old.bin").write_bytes(b"old firmware")
            (root / "random.json").write_text('{"stale":true}', encoding="utf-8")
            (root / ".source" / "data" / "secrets").mkdir(parents=True)
            (root / ".source" / "data" / "secrets" / "legacy.json").write_text("stale", encoding="utf-8")
            (root / "data" / "preferences").mkdir(parents=True)
            (root / "data" / "preferences" / "wifi.json").write_text("old wifi", encoding="utf-8")
            (root / "photos" / "nested").mkdir(parents=True)
            (root / "photos" / "nested" / "image.dat").write_bytes(b"user file")

            helpers["wipe_card_contents"](str(root))
            helpers["structure"](str(root))

            for old_path in ("old.bin", "random.json", ".source/data/secrets/legacy.json",
                             "data/preferences/wifi.json", "photos/nested/image.dat"):
                self.assertFalse((root / old_path).exists(), old_path)
            expected = set(helpers["CLOCKOS_SD_DIRECTORIES"])
            actual = {path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_dir()}
            self.assertEqual(actual, expected)

    def test_windows_metadata_is_the_only_wipe_exemption(self):
        helpers = load_sd_helpers()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "System Volume Information").mkdir()
            (root / "$Recycle.Bin").mkdir()
            (root / "keep.txt").write_text("remove", encoding="utf-8")
            helpers["wipe_card_contents"](str(root))
            self.assertTrue((root / "System Volume Information").is_dir())
            self.assertTrue((root / "$Recycle.Bin").is_dir())
            self.assertFalse((root / "keep.txt").exists())

    def test_delete_failure_is_reported_and_fails_closed(self):
        helpers = load_sd_helpers()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "blocked.txt").write_text("still here", encoding="utf-8")
            with mock.patch.dict(helpers, {"os": mock.Mock(wraps=os)}):
                helpers["os"].remove.side_effect = PermissionError("locked")
                with self.assertRaisesRegex(RuntimeError, "blocked.txt"):
                    helpers["wipe_card_contents"](str(root))

    def test_erase_is_explicit_and_update_only_is_forced_safe(self):
        source = INSTALLER.read_text(encoding="utf-8")
        self.assertIn('erase_var = tk.BooleanVar(value=False)', source)
        self.assertIn('if not erase_var.get():\n                erase_var.set(True)', source)
        self.assertIn('erase_requested = bool(do_dl and erase_var.get() and not update_only)', source)
        self.assertIn('erase_requested = bool(erase_requested and do_dl and not update_only)', source)
        self.assertIn('default="no"', source)
        self.assertIn('if erase_requested:', source)
        self.assertIn('prog(8, "Erasing selected SD-card files...")', source)
        self.assertNotIn("preserve_json_files", source)
        self.assertNotIn("restore_json_files", source)

    def test_current_release_manifest_matches_firmware_image_and_ui(self):
        manifest = json.loads((ROOT / ".source/releases/current.json").read_text(encoding="utf-8"))
        binary_path = ROOT / manifest["binaryPath"]
        image = binary_path.read_bytes()
        self.assertEqual(manifest["displayVersion"], "v2.8")
        self.assertEqual(manifest["binaryBytes"], len(image))
        self.assertEqual(manifest["sha256"], hashlib.sha256(image).hexdigest())
        firmware = (ROOT / manifest["rawSourcePath"] / "ClockOSv2.8.ino").read_text(encoding="utf-8")
        self.assertIn('"v2.8"', firmware)
        self.assertIn('"ClockOSv2.8"', firmware)
        for marker in (b"Settings", b"General", b"Appearance", b"Storage", b"Factory Reset", b"Choose Network"):
            self.assertIn(marker, image)
        self.assertEqual(binary_path.name, "ClockOSv2.8.bin")


if __name__ == "__main__":
    unittest.main()
