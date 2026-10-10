"""
Clock installer (lives in the GitHub repo at .source/install/install.py)
Launched by setup_sd.bat, which downloads this file fresh, runs it, then deletes it.

Compact setup window: choose an SD card, firmware mode, and optional actions:
  Auto (recommended)        - download and flash the newest compiled .bin from GitHub
  Beta (unstable)            - download the newest raw sketches and compile them
  Manual                     - choose a folder of .ino/.h files and compile those sketches
  SD install                 - optionally wipe/install source and icons on a selected SD card
  Classroom notifications    - authorize before the wipe, then save read-only cache/tokens under /data/secrets
When it finishes the window closes by itself.

Source code is kept in  <Documents>\\ClockOSV1\\<name>\\<name>.ino  so you can edit it.
Files in your Downloads (.ino / .h) are MOVED there (never copied).
"python install.py update" = refresh the SD card from GitHub only (no wipe, no icons).
"""
import os, sys, re, json, shutil, stat, threading, ctypes, subprocess, urllib.request, zipfile, tempfile, glob

import tkinter as tk
from tkinter import ttk, messagebox, simpledialog, filedialog

OWNER, REPO, BRANCH = "n0rm0", "Clock", "main"
SOURCE_ROOT = ".source/uncompiled/updates"
# The installer reads the release manifest on every launch.  These values keep
# setup useful and predictable if GitHub is unavailable or the manifest is bad.
DEFAULT_RELEASE = {
    "productName": "ClockOS",
    "displayVersion": "v2.6",
    "firmwareIdentity": "ClockOSv2.6",
    "sketch": "ClockOSv2.6",
    "rawSourcePath": ".source/uncompiled/updates/ClockOSv2.6",
    "binaryPath": ".source/compiled/updates/ClockOSv2.6/ClockOSv2.6.bin",
}
RELEASE = dict(DEFAULT_RELEASE)
PRODUCT_NAME = DEFAULT_RELEASE["productName"]
PRODUCT_VERSION = DEFAULT_RELEASE["displayVersion"]
FIRMWARE_IDENTITY = DEFAULT_RELEASE["firmwareIdentity"]
FLASH_SKETCH = DEFAULT_RELEASE["sketch"]
_RELEASE_LOADED = False
INSTALLER_ICON_NAME = "clockos.ico"
UA = {"User-Agent": "clock-installer"}
ICON_BASE = "https://raw.githubusercontent.com/basmilius/weather-icons/dev/production/fill"
ICONS = ["clear-day", "clear-night", "partly-cloudy-day", "partly-cloudy-night", "cloudy",
         "overcast", "overcast-day", "overcast-night", "rain", "drizzle", "partly-cloudy-day-rain",
         "thunderstorms", "thunderstorms-rain", "thunderstorms-day", "snow", "sleet", "hail",
         "fog", "mist", "haze", "wind", "not-available"]
ICON_SIZE = 96
THEME_IDS = ["crystal", "midnight", "ocean", "sunrise", "graphite"]
THEME_BASE = "https://raw.githubusercontent.com/%s/%s/%s/.source/themes/appearance" % (OWNER, REPO, BRANCH)

# Hosyond 4" ESP32-32E = ESP32 Dev Module, 4 MB flash.
# PartitionScheme=min_spiffs ("Minimal SPIFFS: 1.9 MB APP with OTA") = the biggest app size that
# still has two update slots, which GitHub self-update needs. Every downloaded .bin is also kept
# on the SD card as its own file.
FQBN = "esp32:esp32:esp32:PartitionScheme=min_spiffs"
CORE = "esp32:esp32@2.0.17"        # version used by the Hosyond/LCDWIKI demos
ESP_INDEX = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"
LIBS = ["TFT_eSPI", "PNGdec", "ArduinoJson"]
CLASSROOM_SCOPES = [
    "https://www.googleapis.com/auth/classroom.courses.readonly",
    # Classroom exposes coursework.me as a non-readonly scope; the API calls
    # below remain read-only, but the invalid *.me.readonly variant is rejected
    # by Google Auth Platform.
    "https://www.googleapis.com/auth/classroom.coursework.me",
    "https://www.googleapis.com/auth/classroom.courseworkmaterials.readonly",
    "https://www.googleapis.com/auth/classroom.student-submissions.me.readonly",
    "https://www.googleapis.com/auth/classroom.announcements.readonly",
    "https://www.googleapis.com/auth/calendar.readonly",
]
SECRETS_DIR = os.path.join("data", "secrets")
# TFT_eSPI settings passed at compile time (pins from the LCD wiki), so no library file is edited.
TFT_FLAGS = " ".join([
    "-DUSER_SETUP_LOADED=1", "-DST7796_DRIVER=1", "-DTFT_WIDTH=320", "-DTFT_HEIGHT=480",
    "-DTFT_MISO=12", "-DTFT_MOSI=13", "-DTFT_SCLK=14", "-DTFT_CS=15", "-DTFT_DC=2", "-DTFT_RST=-1",
    "-DTFT_BL=27", "-DTFT_BACKLIGHT_ON=HIGH", "-DTOUCH_CS=33", "-DUSE_HSPI_PORT=1",
    "-DLOAD_GLCD=1", "-DLOAD_FONT2=1", "-DLOAD_FONT4=1", "-DLOAD_FONT6=1", "-DLOAD_FONT7=1",
    "-DLOAD_FONT8=1", "-DLOAD_GFXFF=1", "-DSMOOTH_FONT=1",
    "-DSPI_FREQUENCY=40000000", "-DSPI_READ_FREQUENCY=16000000", "-DSPI_TOUCH_FREQUENCY=2500000",
])
NO_WINDOW = 0x08000000 if os.name == "nt" else 0


def python_executable():
    """Return python.exe even when the GUI was launched through pythonw.exe/pyw.exe."""
    exe = sys.executable
    if os.name == "nt" and os.path.basename(exe).lower() in ("pythonw.exe", "pyw.exe"):
        candidate = os.path.join(os.path.dirname(exe), "python.exe")
        if os.path.isfile(candidate):
            return candidate
    return exe


def install_python_packages(packages, lo, hi, label):
    """Install packages into the same user Python environment used by the GUI."""
    run_cli(python_executable(), ["-m", "pip", "install", "--user"] + list(packages),
            lo, hi, label)


class Cancelled(Exception):
    pass


class State:
    pct, text, done, error, summary = 0, "Starting...", False, None, []


S = State()


def prog(p, t=None):
    S.pct = max(S.pct, p) if p < 100 else p
    if t:
        S.text = t


# ------------------------------------------------------------------ GitHub
def http_get(url, timeout=30):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=timeout) as r:
        return r.read()


def is_application_sketch(name):
    """Return whether *name* is a supported same-name application sketch."""
    return bool(re.fullmatch(r"(?:clockos|update)v[0-9]+(?:\.[0-9]+)*", name or "", re.IGNORECASE))


def is_bootloader_sketch(name):
    """Keep bootloader folders out of automatic source and flash choices."""
    return (name or "").lower().startswith("bootloaderv")


def _safe_release_text(value):
    """Accept short manifest labels/names only; never trust a path-like value."""
    return (isinstance(value, str) and 0 < len(value.strip()) <= 80
            and "\x00" not in value and "/" not in value and "\\" not in value)


def _safe_release_path(value, prefix):
    """Accept a repository-relative manifest path under one expected tree."""
    if not isinstance(value, str):
        return None
    path = value.strip().replace("\\", "/")
    if (not path.startswith(prefix.rstrip("/") + "/") or ".." in path.split("/")
            or path.startswith("/") or len(path) > 240):
        return None
    return path


def load_release_manifest():
    """Load GitHub's current release manifest once, falling back safely offline.

    The manifest is deliberately treated as display/default metadata, not as an
    arbitrary URL or executable instruction.  A malformed or partial response
    leaves the complete hard-coded release together, avoiding mixed labels and
    flash targets.
    """
    global _RELEASE_LOADED, RELEASE, PRODUCT_NAME, PRODUCT_VERSION, FIRMWARE_IDENTITY, FLASH_SKETCH
    if _RELEASE_LOADED:
        return dict(RELEASE)
    _RELEASE_LOADED = True
    release = dict(DEFAULT_RELEASE)
    url = "https://raw.githubusercontent.com/%s/%s/%s/.source/releases/current.json" % (OWNER, REPO, BRANCH)
    try:
        candidate = json.loads(http_get(url, timeout=8).decode("utf-8"))
        required = ("productName", "displayVersion", "firmwareIdentity", "sketch")
        if not isinstance(candidate, dict) or not all(_safe_release_text(candidate.get(key)) for key in required):
            raise ValueError("invalid release manifest")
        sketch = candidate["sketch"].strip()
        if not is_application_sketch(sketch):
            raise ValueError("manifest sketch is not an application sketch")
        release.update({key: candidate[key].strip() for key in required})
        raw_source = _safe_release_path(candidate.get("rawSourcePath"), SOURCE_ROOT)
        if raw_source and os.path.basename(raw_source) == sketch:
            release["rawSourcePath"] = raw_source
        else:
            release["rawSourcePath"] = SOURCE_ROOT + "/" + sketch
        binary_root = ".source/compiled/updates"
        binary_path = _safe_release_path(candidate.get("binaryPath"), binary_root)
        if binary_path and binary_path.lower().endswith(".bin"):
            release["binaryPath"] = binary_path
        else:
            release["binaryPath"] = binary_root + "/%s/%s.bin" % (sketch, sketch)
    except Exception:
        # Network failures must not prevent an offline source build or flash.
        pass
    RELEASE = release
    PRODUCT_NAME = release["productName"]
    PRODUCT_VERSION = release["displayVersion"]
    FIRMWARE_IDENTITY = release["firmwareIdentity"]
    FLASH_SKETCH = release["sketch"]
    return dict(RELEASE)


def list_repo(path):
    base = path.rstrip("/") + "/"
    url = "https://api.github.com/repos/%s/%s/contents/%s?ref=%s" % (OWNER, REPO, path, BRANCH)
    try:
        items = json.loads(http_get(url))
    except Exception:
        return []
    out = []
    for it in items:
        if it["type"] == "file":
            out.append((it["path"][len(base):], it["download_url"]))
        elif it["type"] == "dir":
            out += list_repo(it["path"])
    return out


def latest_source_path():
    """Find the highest-version same-name ClockOS/updateV application sketch."""
    url = "https://api.github.com/repos/%s/%s/git/trees/%s?recursive=1" % (OWNER, REPO, BRANCH)
    data = json.loads(http_get(url))
    prefix = SOURCE_ROOT + "/"
    folders = {}
    for item in data.get("tree", []):
        path = item.get("path", "")
        if item.get("type") != "blob" or not path.startswith(prefix) or not path.endswith(".ino"):
            continue
        folder = path.rsplit("/", 1)[0]
        name = os.path.basename(folder)
        if is_application_sketch(name) and os.path.splitext(os.path.basename(path))[0] == name:
            folders[name] = folder
    if not folders:
        raise RuntimeError("No raw ClockOS/updateV application sketch was found under " + SOURCE_ROOT)
    return max(folders.values(), key=lambda x: (version_key(x), os.path.basename(x).lower().startswith("clockos"), x.lower()))


def fetch_repo(stage):
    files = list_repo(latest_source_path())
    for rel, url in files:
        dst = os.path.join(stage, *rel.split("/"))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, "wb") as f:
            f.write(http_get(url))
    return len(files)


# ------------------------------------------------------------------ Downloads (MOVE, never copy)
DUP = re.compile(r"^(.*?)( \(\d+\))?(\.[^.]+)$")


def grab_downloads(stage):
    dl = os.path.join(os.path.expanduser("~"), "Downloads")
    if not os.path.isdir(dl):
        return 0
    best = {}
    for f in os.listdir(dl):
        m = DUP.match(f)
        if not m or m.group(3).lower() not in (".ino", ".h"):
            continue
        name = m.group(1) + m.group(3)          # "clock (1).ino" -> "clock.ino"
        p = os.path.join(dl, f)
        k = name.lower()
        if k not in best or os.path.getmtime(p) > os.path.getmtime(best[k][1]):
            best[k] = (name, p)

    def read(p):
        with open(p, "rb") as fh:
            return fh.read()

    skip = set()                                # preview copies (clock.h identical to clock.ino)
    for k, (name, p) in best.items():
        twin = best.get(k[:-2] + ".ino") if k.endswith(".h") else None
        if twin and read(p) == read(twin[1]):
            skip.add(k)

    moved = 0
    for k, (name, p) in best.items():
        if k in skip:
            continue
        dst = os.path.join(stage, name)
        if os.path.exists(dst):
            os.remove(dst)
        shutil.move(p, dst)
        moved += 1
    return moved


# ------------------------------------------------------------------ one folder per .ino
INC = re.compile(r'#include\s+"([^"]+)"')


def includes(path):
    with open(path, errors="ignore") as f:
        return set(INC.findall(f.read()))


def arrange(stage, dest):
    files = {}
    for root, _, fs in os.walk(stage):
        for f in fs:
            full = os.path.join(root, f)
            files[os.path.relpath(full, stage).replace("\\", "/")] = full
    inos = sorted(r for r in files if "/" not in r and r.lower().endswith(".ino"))

    def needed(ino):
        seen, todo = set(), list(includes(files[ino]))
        while todo:
            h = todo.pop()
            if h in seen or h not in files:
                continue
            seen.add(h)
            todo += list(includes(files[h]))
        return seen

    plan = {ino: needed(ino) for ino in inos}
    placed = {}

    def put(rel, dst_dir):
        os.makedirs(dst_dir, exist_ok=True)
        dst = os.path.join(dst_dir, os.path.basename(rel))
        if os.path.exists(dst):
            os.remove(dst)
        if rel in placed:
            shutil.copy2(placed[rel], dst)      # shared header goes into every sketch folder
        elif os.path.exists(files[rel]):
            shutil.move(files[rel], dst)
            placed[rel] = dst

    for ino in inos:
        folder = os.path.join(dest, ino[:-4])
        put(ino, folder)
        for h in sorted(plan[ino]):
            put(h, folder)

    stems = {i[:-4].lower(): i[:-4] for i in inos}
    for rel in sorted(files):
        if rel in placed or not os.path.exists(files[rel]):
            continue
        stem = os.path.splitext(os.path.basename(rel))[0].lower()
        # Do not create an extras folder. Unreferenced files are not part of a
        # sketch payload and must not be copied into the user's source tree.
        continue
    return inos


def remove_stale_sketches(dest, current_inos):
    """Remove only stale generated ClockOSv/updateV application folders.

    Other user sketches and bootloader folders are intentionally left alone.
    """
    current = {os.path.splitext(os.path.basename(x))[0].lower() for x in current_inos}
    if not os.path.isdir(dest):
        return
    for entry in os.scandir(dest):
        if not entry.is_dir() or entry.name.startswith(".") or not is_application_sketch(entry.name):
            continue
        ino = os.path.join(entry.path, entry.name + ".ino")
        if os.path.isfile(ino) and entry.name.lower() not in current:
            shutil.rmtree(entry.path, ignore_errors=True)


def workspace():
    """Keep the established ClockOSV1 workspace path for existing installations."""
    docs = os.path.join(os.path.expanduser("~"), "Documents")
    return os.path.join(docs if os.path.isdir(docs) else os.path.expanduser("~"), "ClockOSV1")


def oauth_json_candidates():
    """Find likely Google OAuth client JSON files without scanning the whole computer."""
    home = os.path.expanduser("~")
    roots = [os.path.join(home, name) for name in ("Downloads", "Documents", "Desktop")]
    roots += [os.getcwd()]
    found, seen = [], set()
    for root in roots:
        if not os.path.isdir(root):
            continue
        for current, dirs, files in os.walk(root):
            depth = os.path.relpath(current, root).count(os.sep)
            if depth >= 3:
                dirs[:] = []
            for name in files:
                if not name.lower().endswith(".json"):
                    continue
                path = os.path.abspath(os.path.join(current, name))
                if path in seen:
                    continue
                seen.add(path)
                try:
                    if os.path.getsize(path) > 2 * 1024 * 1024:
                        continue
                    with open(path, encoding="utf-8") as f:
                        data = json.load(f)
                    block = data.get("installed") or data.get("web")
                    if isinstance(block, dict) and block.get("client_id") and block.get("client_secret"):
                        found.append(path)
                except (OSError, ValueError, TypeError):
                    continue
    return found


def get_source(manual=None):
    """GitHub files + Downloads -> <Documents>/ClockOSV1/<name>/<name>.ino. Returns (github, moved, inos).
    manual = list of files you picked by hand: GitHub and Downloads are skipped; the picked files
    are MOVED into ClockOSV1 (anything else already there stays)."""
    ws = workspace()
    stage = tempfile.mkdtemp(prefix="clock_stage_")
    try:
        if manual:
            n = 0
            chosen = {os.path.basename(p).lower() for p in manual}
            for p in manual:
                dst = os.path.join(stage, os.path.basename(p))
                if os.path.exists(dst):
                    os.remove(dst)
                shutil.move(p, dst)
            m = len(manual)
            if os.path.isdir(ws):               # keep headers that live in the other sketch folders
                for root, _, fs in os.walk(ws):
                    if ".build" in root:
                        continue
                    for f in fs:
                        if f.lower() not in chosen and f.lower().endswith((".ino", ".h")) \
                                and not os.path.exists(os.path.join(stage, f)):
                            shutil.copy2(os.path.join(root, f), os.path.join(stage, f))
        else:
            n = fetch_repo(stage)
            m = grab_downloads(stage)
        inos = arrange(stage, ws)
        if not manual:
            remove_stale_sketches(ws, inos)
    finally:
        shutil.rmtree(stage, ignore_errors=True)
    return n, m, inos


def check_files(picked, need_update):
    """Check you picked everything the sketches need. Returns a list of problems (empty = OK)."""
    ws = workspace()
    have = {}                                   # lowercase name -> path
    if os.path.isdir(ws):
        for root, _, fs in os.walk(ws):
            if ".build" in root:
                continue
            for f in fs:
                have.setdefault(f.lower(), os.path.join(root, f))
    for p in picked:                            # picked files win over what is already in ClockOSV1
        have[os.path.basename(p).lower()] = p
    inos = sorted(n for n in have if n.endswith(".ino"))
    problems = []
    if not inos:
        return ["No .ino file selected. Pick at least one ClockOSv/updateV application sketch."]
    if need_update and not any(is_application_sketch(os.path.splitext(ino)[0]) for ino in inos):
        problems.append("A ClockOSv/updateV application sketch is missing (needed to flash the ESP32).")
    for ino in inos:
        seen, todo = set(), [ino]
        while todo:
            cur = todo.pop()
            if cur in seen or cur not in have:
                continue
            seen.add(cur)
            for h in includes(have[cur]):
                if h.lower() not in have:
                    problems.append("%s needs %s - not selected." % (ino, h))
                else:
                    todo.append(h.lower())
    return sorted(set(problems))


# ------------------------------------------------------------------ SD card helpers
def drive_type(root):
    return ctypes.windll.kernel32.GetDriveTypeW(ctypes.c_wchar_p(root)) if os.name == "nt" else 0


def drive_root(path):
    return os.path.splitdrive(os.path.abspath(path))[0].upper() + "\\"


def safe_to_wipe(root):
    if os.name != "nt":
        return False
    if root.upper().startswith(os.environ.get("SystemDrive", "C:").upper()):
        return False
    if drive_root(os.path.expanduser("~")) == root or drive_root(sys.executable) == root:
        return False
    return drive_type(root) == 2


def volume_label(root):
    buf = ctypes.create_unicode_buffer(261)
    ctypes.windll.kernel32.GetVolumeInformationW(ctypes.c_wchar_p(root), buf, 261, None, None, None, None, 0)
    return buf.value


def removable_drives():
    out = []
    if os.name != "nt":
        return out
    for c in "DEFGHIJKLMNOPQRSTUVWXYZ":
        r = c + ":\\"
        if os.path.exists(r) and drive_type(r) == 2:
            try:
                gb = shutil.disk_usage(r).total / 1e9
            except Exception:
                continue
            out.append((r, "%s  %s  (%.1f GB)" % (r[:2], volume_label(r) or "No label", gb)))
    return out


def _rm(func, path, _exc):
    os.chmod(path, stat.S_IWRITE)
    func(path)


def preserve_json_files(root):
    """Copy every JSON file aside before a wipe so secrets/configs can return."""
    stage = tempfile.mkdtemp(prefix="clock_sd_json_")
    for current, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if d.lower() not in ("system volume information", "$recycle.bin")]
        for name in files:
            if not name.lower().endswith(".json"):
                continue
            src = os.path.join(current, name)
            rel = os.path.relpath(src, root)
            dst = os.path.join(stage, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            try:
                shutil.copy2(src, dst)
            except OSError:
                pass
    return stage


def restore_json_files(root, stage):
    if not stage or not os.path.isdir(stage):
        return 0
    count = 0
    for current, _, files in os.walk(stage):
        for name in files:
            src = os.path.join(current, name)
            rel = os.path.relpath(src, stage)
            dst = os.path.join(root, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
            count += 1
    return count


def wipe(root):
    for e in os.scandir(root):
        if e.name.lower() in ("system volume information", "$recycle.bin"):
            continue
        try:
            if e.is_dir(follow_symlinks=False):
                shutil.rmtree(e.path, onerror=_rm)
            else:
                os.chmod(e.path, stat.S_IWRITE)
                os.remove(e.path)
        except Exception:
            pass


def structure(drive):
    """Create Clock-owned source/assets plus the root paths the firmware uses."""
    for d in ("compiled", "compiled/updates", "compiled/bootloader/fallback",
              "uncompiled", "uncompiled/updates", "uncompiled/bootloader/fallback",
              "data", "data/preferences", "data/secrets", "icons", "themes", "themes/appearance"):
        os.makedirs(os.path.join(drive, ".source", d), exist_ok=True)
    # Firmware preferences and Classroom cache are deliberately root-level,
    # while installer sources, update files, icons, and themes live in .source.
    for d in ("data", "data/preferences", "data/secrets"):
        os.makedirs(os.path.join(drive, d), exist_ok=True)
    if os.name == "nt":
        try:
            subprocess.run(["attrib", "+h", os.path.join(drive, ".source")], check=False,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=NO_WINDOW)
        except OSError:
            pass


def icons(drive, lo, hi):
    try:
        import fitz
    except ImportError:
        install_python_packages(["pymupdf"], lo, min(hi, lo + 3), "Installing weather-icon support")
        try:
            import fitz
        except ImportError as exc:
            raise RuntimeError("Weather icons need PyMuPDF, but it could not be imported after installation: %s" % exc)
    ok, bad = 0, []
    for i, name in enumerate(ICONS):
        prog(lo + int((hi - lo) * i / len(ICONS)), "Downloading icons... %d/%d" % (i + 1, len(ICONS)))
        data = None
        for folder in ("svg-static", "svg"):
            try:
                data = http_get("%s/%s/%s.svg" % (ICON_BASE, folder, name), 20)
                break
            except Exception:
                continue
        if data is None:
            bad.append(name)
            continue
        try:
            page = fitz.open(stream=data, filetype="svg")[0]
            z = ICON_SIZE / max(page.rect.width, page.rect.height)
            page.get_pixmap(matrix=fitz.Matrix(z, z), alpha=True).save(
                os.path.join(drive, ".source", "icons", name + ".png"))
            ok += 1
        except Exception:
            bad.append(name)
    return ok, bad


def themes(drive, lo, hi):
    target = os.path.join(drive, ".source", "themes", "appearance")
    os.makedirs(target, exist_ok=True)
    ok, bad = 0, []
    for i, theme_id in enumerate(THEME_IDS):
        prog(lo + int((hi - lo) * i / len(THEME_IDS)), "Downloading themes... %d/%d" % (i + 1, len(THEME_IDS)))
        try:
            data = http_get("%s/%s.json" % (THEME_BASE, theme_id), 20)
            with open(os.path.join(target, theme_id + ".json"), "wb") as f:
                f.write(data)
            ok += 1
        except Exception:
            bad.append(theme_id)
    return ok, bad


def classroom_setup(credentials_path, secrets):
    """Authorize the school account and stage private tokens/cache for the SD card."""
    if not credentials_path or not os.path.isfile(credentials_path):
        raise RuntimeError("Classroom notifications require the downloaded OAuth JSON file.")
    os.makedirs(secrets, exist_ok=True)
    prog(53, "Preparing Google Classroom authorization...")
    try:
        from google_auth_oauthlib.flow import InstalledAppFlow
        from googleapiclient.discovery import build
    except ImportError:
        install_python_packages(["google-auth-oauthlib", "google-api-python-client"],
                                53, 58, "Installing Google Classroom support")
        try:
            from google_auth_oauthlib.flow import InstalledAppFlow
            from googleapiclient.discovery import build
        except ImportError as exc:
            raise RuntimeError("Google Classroom support was installed but could not be imported. "
                               "Restart Clock Setup after installing Python 3: %s" % exc)

    client_copy = os.path.join(secrets, "classroomsecret.json")
    token_file = os.path.join(secrets, "classroom_token.json")
    cache_file = os.path.join(secrets, "classroom_cache.json")
    shutil.copy2(credentials_path, client_copy)
    flow = InstalledAppFlow.from_client_secrets_file(credentials_path, CLASSROOM_SCOPES)
    # This opens the browser on the Windows computer. The user must choose the school account.
    creds = flow.run_local_server(port=0, access_type="offline", prompt="consent")
    with open(token_file, "w", encoding="utf-8") as f:
        f.write(creds.to_json())

    classroom = build("classroom", "v1", credentials=creds, cache_discovery=False)
    calendar = build("calendar", "v3", credentials=creds, cache_discovery=False)
    courses = classroom.courses().list(courseStates=["ACTIVE"], pageSize=100).execute().get("courses", [])
    assignments = []
    announcements = []
    for course in courses:
        cid = course.get("id")
        try:
            work = classroom.courses().courseWork().list(
                courseId=cid, courseWorkStates=["PUBLISHED"], orderBy="dueDate asc", pageSize=100
            ).execute().get("courseWork", [])
        except Exception:
            work = []
        for item in work:
            due = item.get("dueDate") or {}
            assignments.append({
                "course": course.get("name", ""), "title": item.get("title", ""),
                "description": item.get("description", ""), "dueDate": due,
                "dueTime": item.get("dueTime") or {}, "link": item.get("alternateLink", ""),
            })
        try:
            posts = classroom.courses().announcements().list(courseId=cid, pageSize=50).execute().get("announcements", [])
        except Exception:
            posts = []
        for post in posts:
            announcements.append({"course": course.get("name", ""), "text": post.get("text", ""),
                                  "creationTime": post.get("creationTime", ""),
                                  "link": post.get("alternateLink", "")})
    from datetime import datetime, timezone
    now = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
    events = calendar.events().list(calendarId="primary", timeMin=now, maxResults=100,
                                    singleEvents=True, orderBy="startTime").execute().get("items", [])
    cache = {"updatedAt": now, "courses": courses, "assignments": assignments,
             "announcements": announcements, "calendar": events}
    with open(cache_file, "w", encoding="utf-8") as f:
        json.dump(cache, f, ensure_ascii=False)
    prog(58, "Google Classroom data saved to SD card")
    return len(assignments), len(events)


def put_on_sd(drive, ws):
    """Copy recognized application sources into .source without touching unrelated files."""
    root = os.path.join(drive, ".source", "uncompiled")
    for e in os.scandir(ws):
        if e.is_dir() and e.name not in ("build", ".build"):
            if is_application_sketch(e.name):
                target = os.path.join(root, "updates", e.name)
            elif is_bootloader_sketch(e.name):
                target = os.path.join(root, "bootloader", "fallback", e.name)
            else:
                continue
            shutil.copytree(e.path, target, dirs_exist_ok=True)


# ------------------------------------------------------------------ arduino-cli (the engine inside Arduino IDE 2)
def find_cli():
    cands = []
    for base in (os.environ.get("LOCALAPPDATA"), os.environ.get("ProgramFiles"), os.environ.get("ProgramFiles(x86)")):
        if base:
            cands.append(os.path.join(base, "Programs", "arduino-ide", "resources", "app", "lib", "backend", "resources", "arduino-cli.exe"))
            cands.append(os.path.join(base, "Arduino IDE", "resources", "app", "lib", "backend", "resources", "arduino-cli.exe"))
    cands.append(os.path.join(os.environ.get("LOCALAPPDATA", ""), "ClockInstaller", "arduino-cli.exe"))
    for c in cands:
        if os.path.isfile(c):
            return c
    found = shutil.which("arduino-cli")
    if found:
        return found
    # not installed: fetch the standalone cli once
    dst = os.path.join(os.environ.get("LOCALAPPDATA", tempfile.gettempdir()), "ClockInstaller")
    os.makedirs(dst, exist_ok=True)
    prog(S.pct, "Downloading arduino-cli...")
    z = os.path.join(dst, "cli.zip")
    with open(z, "wb") as f:
        f.write(http_get("https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip", 120))
    with zipfile.ZipFile(z) as zf:
        zf.extract("arduino-cli.exe", dst)
    os.remove(z)
    return os.path.join(dst, "arduino-cli.exe")


def run_command(command, lo, hi, label):
    """Run a tool hidden; creep the bar from lo to hi while it prints."""
    prog(lo, label)
    p = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         text=True, errors="replace", creationflags=NO_WINDOW)
    tail, cur = [], lo
    for line in p.stdout:
        line = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", line).strip()
        if not line:
            continue
        tail = (tail + [line])[-40:]
        cur = min(hi - 1, cur + 0.2)
        prog(int(cur), "%s  %s" % (label, line[:48]))
    p.wait()
    if p.returncode != 0:
        raise RuntimeError("%s failed:\n%s" % (label, "\n".join(tail)))
    return "\n".join(tail)


def run_cli(cli, args, lo, hi, label):
    return run_command([cli] + args, lo, hi, label)


def esptool_command():
    """Find the esptool bundled with the ESP32 Arduino core, or use Python's module."""
    roots = []
    for base in (os.environ.get("LOCALAPPDATA", ""), os.environ.get("APPDATA", ""),
                 os.path.expanduser("~/.arduino15")):
        if base:
            roots.append(os.path.join(base, "Arduino15", "packages", "esp32", "tools"))
            roots.append(os.path.join(base, "packages", "esp32", "tools"))
    for root in roots:
        for pattern in ("**/esptool.exe", "**/esptool.py", "**/esptool"):
            matches = glob.glob(os.path.join(root, pattern), recursive=True)
            if matches:
                path = matches[-1]
                return [python_executable(), path] if path.lower().endswith(".py") else [path]
    return [python_executable(), "-m", "esptool"]


def serial_ports(cli):
    """Return serial ports only, with the detected board name when available."""
    try:
        out = subprocess.run([cli, "board", "list", "--format", "json"], capture_output=True,
                             text=True, creationflags=NO_WINDOW, timeout=30).stdout
        data = json.loads(out)
        data = data.get("detected_ports", data) if isinstance(data, dict) else data
        result = []
        seen = set()
        for item in data:
            port = item.get("port", {})
            address = port.get("address", "")
            if not address or port.get("protocol") != "serial" or re.fullmatch(r"[A-Za-z]:", address) or address in seen:
                continue
            seen.add(address)
            boards = item.get("matching_boards") or item.get("boards") or []
            board = ""
            if boards and isinstance(boards[0], dict):
                board = boards[0].get("name") or boards[0].get("fqbn") or ""
            result.append((address, board or "Serial device"))
        return result
    except Exception:
        return []


def version_key(path):
    values = re.findall(r"[Vv](\d+(?:\.\d+)*)", path)
    if not values:
        return (0,)
    return tuple(int(x) for x in values[-1].split("."))


def default_flash_target(sketches):
    """Choose the highest-version application sketch, preferring canonical ClockOS names."""
    applications = [name for name in sketches if is_application_sketch(name)]
    if applications:
        return max(applications, key=lambda name: (version_key(name), name.lower().startswith("clockos"), name.lower()))
    return next((name for name in sketches if not is_bootloader_sketch(name)), None)


def latest_binary():
    """Find the highest-version compiled ClockOS/updateV application .bin."""
    url = "https://api.github.com/repos/%s/%s/git/trees/%s?recursive=1" % (OWNER, REPO, BRANCH)
    data = json.loads(http_get(url))
    prefix = ".source/compiled/updates/"
    paths = [item.get("path", "") for item in data.get("tree", [])
             if item.get("type") == "blob" and item.get("path", "").startswith(prefix)
             and item.get("path", "").lower().endswith(".bin")]
    application_paths = []
    for path in paths:
        relative = path[len(prefix):]
        folder = relative.split("/", 1)[0]
        if is_application_sketch(folder):
            application_paths.append(path)
    if not application_paths:
        raise RuntimeError("No compiled ClockOS/updateV application .bin is available in GitHub at " + prefix)
    return max(application_paths, key=lambda x: (version_key(x), os.path.basename(x.rsplit("/", 1)[0]).lower().startswith("clockos"), x))


def download_latest_binary():
    path = latest_binary()
    url = "https://raw.githubusercontent.com/%s/%s/%s/%s" % (OWNER, REPO, BRANCH, path)
    data = http_get(url, 120)
    stem = os.path.splitext(os.path.basename(path))[0]
    folder = os.path.join(workspace(), ".build", "auto")
    os.makedirs(folder, exist_ok=True)
    target = os.path.join(folder, stem + ".bin")
    with open(target, "wb") as f:
        f.write(data)
    return path, target


def flash_binary(cli, binary, port, label):
    """Flash an application-only .bin into the existing ESP32 OTA app slot.

    Arduino CLI upload expects bootloader and partition artifacts beside the
    application. Auto mode intentionally downloads only the application, so
    use esptool directly at the normal ESP32 app offset instead.
    """
    command = esptool_command() + ["--chip", "esp32", "--port", port,
                                   "write_flash", "0x10000", binary]
    run_command(command, 88, 99, "Flashing %s to %s" % (label, port))


def build_sketches(cli, sketches):
    build = os.path.join(workspace(), ".build")
    span = 14.0 / max(1, len(sketches))
    for i, name in enumerate(sketches):
        lo = int(76 + i * span)
        run_cli(cli, ["compile", "--fqbn", FQBN, "--build-property", "compiler.cpp.extra_flags=" + TFT_FLAGS,
                      "--output-dir", os.path.join(build, name), os.path.join(workspace(), name)],
                lo, int(lo + span), "Compiling %s" % name)
        output = os.path.join(build, name, name + ".ino.bin")
        if not os.path.isfile(output):
            output = os.path.join(build, name, name + ".bin")
        if not os.path.isfile(output):
            raise RuntimeError("Compilation finished but no %s.bin was produced for %s" % (name, name))
    return build


def save_named_binaries(build, sketches):
    target_dir = os.path.join(os.path.expanduser("~"), "Downloads", "ClockBuilds")
    os.makedirs(target_dir, exist_ok=True)
    saved = []
    for name in sketches:
        candidates = (os.path.join(build, name, name + ".ino.bin"),
                      os.path.join(build, name, name + ".bin"))
        source = next((p for p in candidates if os.path.isfile(p)), None)
        if source:
            target = os.path.join(target_dir, name + ".bin")
            shutil.copy2(source, target)
            saved.append(target)
    return saved


def ask_sketch(sketches, root, default=None):
    """Ask for a compiled target; a bootloader always needs an explicit choice."""
    if len(sketches) == 1 and default == sketches[0] and not is_bootloader_sketch(sketches[0]):
        return sketches[0]
    box, ev = {}, threading.Event()
    def ask():
        dialog = tk.Toplevel(root)
        dialog.title("Select firmware to flash")
        dialog.transient(root)
        dialog.grab_set()
        ttk.Label(dialog, text="Select the compiled firmware to upload:").pack(padx=16, pady=(14, 6))
        explicit_choice = default not in sketches
        values = (["Choose firmware manually..."] if explicit_choice else []) + sketches
        combo = ttk.Combobox(dialog, values=values, state="readonly", width=42)
        combo.current(values.index(default) if default in values else 0)
        combo.pack(padx=16, pady=4)
        def choose():
            if combo.get() not in sketches:
                return
            box["name"] = combo.get()
            dialog.destroy(); ev.set()
        button = ttk.Button(dialog, text="Use selected firmware", command=choose)
        button.pack(pady=(6, 14))
        if explicit_choice:
            button.state(["disabled"])
            combo.bind("<<ComboboxSelected>>", lambda _event: button.state(["!disabled"])
                       if combo.get() in sketches else button.state(["disabled"]))
        dialog.protocol("WM_DELETE_WINDOW", lambda: (dialog.destroy(), ev.set()))
    root.after(0, ask)
    ev.wait()
    return box.get("name")


def build_and_flash(mode, do_flash, ask_port, ask_target, selected_port=None):
    cli = find_cli()
    if mode == "auto":
        path, binary = download_latest_binary()
        if do_flash:
            ports = serial_ports(cli)
            if not ports:
                raise RuntimeError("No serial ESP32 port found. The SD-card drive is not a flash port.")
            port = selected_port or ask_port(ports)
            if not port:
                raise Cancelled()
            flash_binary(cli, binary, port, os.path.basename(path))
            return "Flashed newest compiled firmware %s to %s" % (path, port), []
        return "Downloaded newest compiled firmware: " + path, []

    ws = workspace()
    sketches = sorted(e.name for e in os.scandir(ws)
                      if e.is_dir() and os.path.isfile(os.path.join(e.path, e.name + ".ino")))
    if mode == "beta":
        sketches = [name for name in sketches if is_application_sketch(name)]
        if not sketches:
            raise RuntimeError("No ClockOSv/updateV application sketch was found in the newest raw source.")
    if not sketches:
        raise RuntimeError("No sketches found in " + ws)
    prog(55, "Preparing Arduino tools...")
    run_cli(cli, ["core", "update-index", "--additional-urls", ESP_INDEX], 55, 58, "Updating board index")
    run_cli(cli, ["core", "install", CORE, "--additional-urls", ESP_INDEX], 58, 72, "Installing ESP32 board support")
    run_cli(cli, ["lib", "install"] + LIBS, 72, 76, "Installing libraries")
    build = build_sketches(cli, sketches)
    saved = save_named_binaries(build, sketches)
    if not do_flash:
        return "Built %s; .bin output: %s" % (", ".join(sketches), ", ".join(saved)), sketches
    ports = serial_ports(cli)
    if not ports:
        raise RuntimeError("No serial ESP32 port found. The SD-card drive is not a flash port.")
    target = ask_target(sketches, default_flash_target(sketches))
    if not target:
        raise Cancelled()
    port = selected_port or ask_port(ports)
    if not port:
        raise Cancelled()
    sk = target
    run_cli(cli, ["upload", "--fqbn", FQBN, "-p", port, "--input-dir", os.path.join(build, sk),
                  os.path.join(ws, sk)], 91, 99, "Flashing %s to %s" % (sk, port))
    return "Flashed %s to %s" % (sk, port), sketches


# ------------------------------------------------------------------ the whole job
def work(opts, ask_port, ask_target):
    classroom_stage = None
    preserved_json = None
    try:
        drive, mode, do_dl, do_fl, update_only, manual, selected_port, classroom_enabled, credentials_path = opts
        n = m = 0
        inos = []
        if mode in ("beta", "manual") or do_dl:
            prog(3, "Getting raw source..." if mode != "manual" else "Reading selected folder...")
            n, m, inos = get_source(manual)
            S.summary += ["Source folder: " + workspace(),
                          "Files from GitHub: %d" % n,
                          "Copied from selection/Downloads: %d" % m]
        if classroom_enabled:
            if not do_dl or not drive:
                raise RuntimeError("Enable SD-card installation when Classroom notifications are enabled.")
            # OAuth happens before wipe/install so the user can sign in and the resulting cache
            # is then copied into the freshly created /data/secrets folder.
            classroom_stage = tempfile.mkdtemp(prefix="clock_classroom_")
            count, events = classroom_setup(credentials_path, classroom_stage)
            S.summary.append("Google Classroom: %d assignments, %d calendar events saved" % (count, events))
        if do_dl:
            if not update_only:
                prog(8, "Erasing SD card...")
                preserved_json = preserve_json_files(drive)
                wipe(drive)
            prog(15, "Creating folders on the SD card...")
            structure(drive)
            if preserved_json:
                restored = restore_json_files(drive, preserved_json)
                S.summary.append("Preserved JSON files: %d" % restored)
            put_on_sd(drive, workspace())
            if classroom_stage:
                target = os.path.join(drive, SECRETS_DIR)
                os.makedirs(target, exist_ok=True)
                for name in os.listdir(classroom_stage):
                    shutil.copy2(os.path.join(classroom_stage, name), os.path.join(target, name))
            S.summary.append("SD card: %s  (sketches: %s)" % (drive, ", ".join(inos) or "none"))
            if not update_only:
                ok, bad = icons(drive, 20, 52)
                S.summary.append("Icons: %d/%d%s" % (ok, len(ICONS), "  (failed: " + ", ".join(bad) + ")" if bad else ""))
                tok, tbad = themes(drive, 52, 58)
                S.summary.append("Themes: %d/%d%s" % (tok, len(THEME_IDS), "  (failed: " + ", ".join(tbad) + ")" if tbad else ""))
        if mode == "auto" and not do_dl:
            prog(20, "Finding newest compiled firmware...")
        if do_fl or mode == "auto":
            msg, _ = build_and_flash(mode, do_fl, ask_port, ask_target, selected_port)
            S.summary.append(msg)
        elif mode in ("beta", "manual"):
            msg, _ = build_and_flash(mode, False, ask_port, ask_target, selected_port)
            S.summary.append(msg)
        prog(100, "Done")
    except Cancelled:
        S.error = "Cancelled."
    except Exception as e:
        S.error = str(e)
    finally:
        if classroom_stage:
            shutil.rmtree(classroom_stage, ignore_errors=True)
        if preserved_json:
            shutil.rmtree(preserved_json, ignore_errors=True)
        S.done = True


# ------------------------------------------------------------------ UI (compact setup)
def ensure_windows_icon():
    """Fetch the compact title-bar icon beside a temporary downloaded installer.

    `setup_sd.bat` downloads only this Python file, so a repository checkout is
    not guaranteed to be beside it. A failed/malformed icon download is ignored
    deliberately: setup must remain usable offline and never open a console.
    """
    if os.name != "nt":
        return None
    icon_dir = os.path.dirname(os.path.abspath(__file__))
    icon_path = os.path.join(icon_dir, INSTALLER_ICON_NAME)
    try:
        if os.path.isfile(icon_path) and os.path.getsize(icon_path) >= 128:
            with open(icon_path, "rb") as existing:
                if existing.read(4) == b"\x00\x00\x01\x00":
                    return icon_path
        url = "https://raw.githubusercontent.com/%s/%s/%s/.source/install/%s" % (
            OWNER, REPO, BRANCH, INSTALLER_ICON_NAME)
        data = http_get(url, timeout=12)
        if len(data) < 128 or data[:4] != b"\x00\x00\x01\x00":
            return None
        staged = icon_path + ".tmp"
        with open(staged, "wb") as f:
            f.write(data)
        os.replace(staged, icon_path)
        return icon_path
    except OSError:
        return None
    except Exception:
        return None


def set_windows_icon(root, icon_path=None):
    """Apply the local ClockOS icon without blocking the setup window on error."""
    if os.name != "nt" or not icon_path:
        return
    try:
        root.iconbitmap(icon_path)
    except tk.TclError:
        # A missing/corrupt icon must never keep the hidden launcher from opening setup.
        pass


def configure_setup_style(root):
    """Apply a restrained, Windows-native-friendly visual system to setup."""
    style = ttk.Style(root)
    try:
        style.theme_use("clam")
    except tk.TclError:
        pass
    background, card, text, muted, border, accent = "#0F0F10", "#1C1C1E", "#F5F5F7", "#A1A1A6", "#303033", "#0A84FF"
    style.configure("TFrame", background=background)
    style.configure("Card.TFrame", background=card, relief="solid", borderwidth=1)
    style.configure("Title.TLabel", background=background, foreground=text, font=("Segoe UI", 16, "bold"))
    style.configure("Subtitle.TLabel", background=background, foreground=muted, font=("Segoe UI", 9))
    style.configure("CardTitle.TLabel", background=card, foreground=text, font=("Segoe UI", 10, "bold"))
    style.configure("CardHint.TLabel", background=card, foreground=muted, font=("Segoe UI", 8))
    style.configure("Card.TCheckbutton", background=card, foreground=text, font=("Segoe UI", 9))
    style.map("Card.TCheckbutton", background=[("active", card)], foreground=[("disabled", "#9A9AA0")])
    style.configure("TCombobox", font=("Segoe UI", 9), padding=3, fieldbackground="#2C2C2E",
                    background="#2C2C2E", foreground=text)
    style.map("TCombobox", fieldbackground=[("readonly", "#2C2C2E")], foreground=[("readonly", text)])
    style.configure("Quiet.TButton", background="#2C2C2E", foreground=text, borderwidth=0,
                    font=("Segoe UI", 8), padding=(8, 3))
    style.map("Quiet.TButton", background=[("active", "#3A3A3C")])
    style.configure("Accent.TButton", background=accent, foreground="#FFFFFF", borderwidth=0,
                    font=("Segoe UI", 9, "bold"), padding=(15, 6))
    style.map("Accent.TButton", background=[("active", "#409CFF"), ("disabled", "#355A82")],
              foreground=[("disabled", "#C8D8E8")])
    style.configure("Setup.Horizontal.TProgressbar", troughcolor="#2C2C2E", background=accent,
                    bordercolor="#2C2C2E", lightcolor=accent, darkcolor=accent)


def card(parent, title):
    """Return a consistently spaced card with a small title row."""
    frame = ttk.Frame(parent, style="Card.TFrame", padding=(12, 9))
    frame.pack(fill="x", pady=(0, 7))
    ttk.Label(frame, text=title, style="CardTitle.TLabel").pack(anchor="w")
    return frame


def main():
    update_only = "update" in [a.lower() for a in sys.argv[1:]]
    root = tk.Tk()
    set_windows_icon(root, ensure_windows_icon())
    root.withdraw()
    configure_setup_style(root)

    splash = tk.Toplevel(root)
    splash.title("Clock Setup")
    splash.geometry("330x145")
    splash.resizable(False, False)
    splash.attributes("-topmost", True)
    splash.protocol("WM_DELETE_WINDOW", lambda: None)
    splash.configure(bg="#0F0F10")
    tk.Label(splash, text="Clock Setup", bg="#0F0F10", fg="#F5F5F7",
             font=("Segoe UI", 14, "bold")).pack(pady=(22, 4))
    tk.Label(splash, text="Checking current release information…", bg="#0F0F10", fg="#A1A1A6",
             font=("Segoe UI", 9)).pack()
    splash_bar = ttk.Progressbar(splash, mode="indeterminate", length=246,
                                 style="Setup.Horizontal.TProgressbar")
    splash_bar.pack(pady=(16, 0))
    splash_bar.start(12)
    splash.update_idletasks()
    sx = (splash.winfo_screenwidth() - splash.winfo_width()) // 2
    sy = (splash.winfo_screenheight() - splash.winfo_height()) // 2
    splash.geometry("+%d+%d" % (sx, sy))
    splash.update()
    # A failed request silently retains DEFAULT_RELEASE, so setup remains usable offline.
    load_release_manifest()

    root.title("%s Setup" % PRODUCT_NAME)
    root.geometry("640x460")
    root.resizable(False, False)
    root.attributes("-topmost", True)
    root.configure(bg="#0F0F10")

    shell = tk.Frame(root, bg="#0F0F10")
    shell.pack(fill="both", expand=True)
    sidebar = tk.Frame(shell, bg="#141416", width=145)
    sidebar.pack(side="left", fill="y")
    sidebar.pack_propagate(False)
    content = tk.Frame(shell, bg="#0F0F10")
    content.pack(side="left", fill="both", expand=True)

    tk.Label(sidebar, text="CLOCK", bg="#141416", fg="#F5F5F7",
             font=("Segoe UI", 12, "bold")).pack(anchor="w", padx=16, pady=(20, 1))
    tk.Label(sidebar, text="SETUP", bg="#141416", fg="#A1A1A6",
             font=("Segoe UI", 8, "bold")).pack(anchor="w", padx=16, pady=(0, 20))
    tk.Label(sidebar, text="FIRMWARE MODE", bg="#141416", fg="#8E8E93",
             font=("Segoe UI", 7, "bold")).pack(anchor="w", padx=16, pady=(0, 5))

    mode = tk.StringVar(value="auto")
    mode_buttons = {}
    for value, label in (("auto", "Auto · Recommended"), ("beta", "Beta · Source"), ("manual", "Manual · Files")):
        button = tk.Radiobutton(sidebar, text=label, variable=mode, value=value, indicatoron=0,
                                anchor="w", relief="flat", bd=0, padx=12, pady=8,
                                bg="#141416", fg="#E5E5EA", activebackground="#2C2C2E",
                                activeforeground="#FFFFFF", selectcolor="#2C2C2E",
                                font=("Segoe UI", 9))
        button.pack(fill="x", padx=9, pady=1)
        mode_buttons[value] = button

    def paint_mode(*_unused):
        selected = mode.get()
        for value, button in mode_buttons.items():
            button.configure(bg="#2C2C2E" if value == selected else "#141416",
                             fg="#FFFFFF" if value == selected else "#E5E5EA")

    mode.trace_add("write", paint_mode)
    paint_mode()

    def info():
        messagebox.showinfo(
            "Clock setup modes",
            "Auto (recommended): downloads the current %s application .bin from GitHub and never chooses a bootloader.\n\n"
            "Beta (source): downloads the current raw ClockOSv/updateV application source, compiles it, and can flash it.\n\n"
            "Manual: choose a folder of .ino and .h files to compile. A bootloader can only be chosen explicitly in the target dialog."
            % FIRMWARE_IDENTITY,
            parent=root)

    tk.Button(sidebar, text="Mode help", command=info, anchor="w", relief="flat", bd=0,
              bg="#141416", fg="#64B5FF", activebackground="#141416", activeforeground="#A8D6FF",
              font=("Segoe UI", 8)).pack(anchor="w", padx=14, pady=(7, 0))
    if update_only:
        tk.Label(sidebar, text="UPDATE ONLY\nNo SD erase or flash", justify="left", bg="#141416", fg="#A1A1A6",
                 font=("Segoe UI", 8)).pack(anchor="sw", side="bottom", padx=16, pady=18)

    inner = tk.Frame(content, bg="#0F0F10")
    inner.pack(fill="both", expand=True, padx=16, pady=(13, 10))
    ttk.Label(inner, text="%s %s" % (PRODUCT_NAME, PRODUCT_VERSION), style="Title.TLabel").pack(anchor="w")
    ttk.Label(inner, text="Release %s  •  install source, assets, and firmware in one place" % FIRMWARE_IDENTITY,
              style="Subtitle.TLabel").pack(anchor="w", pady=(1, 9))

    sd_card = card(inner, "SD card installation")
    drives = removable_drives()
    labels = [d[1] for d in drives]
    sd_row = ttk.Frame(sd_card, style="Card.TFrame")
    sd_row.pack(fill="x", pady=(5, 2))
    combo = ttk.Combobox(sd_row, values=labels, state="readonly", width=41)
    if labels:
        combo.current(0)
    combo.pack(side="left", fill="x", expand=True)

    def refresh():
        nonlocal drives
        drives = removable_drives()
        values = [d[1] for d in drives]
        combo["values"] = values
        combo.current(0) if values else combo.set("")
        sd_hint.configure(text="Select a removable SD card" if values else "No removable SD card found")

    ttk.Button(sd_row, text="Refresh", style="Quiet.TButton", command=refresh).pack(side="left", padx=(7, 0))
    sd_hint = ttk.Label(sd_card, text="Select a removable SD card" if labels else "No removable SD card found",
                        style="CardHint.TLabel")
    sd_hint.pack(anchor="w")
    v_dl = tk.BooleanVar(value=False)
    ttk.Checkbutton(sd_card, text="Install source, icons, and themes", variable=v_dl,
                    style="Card.TCheckbutton").pack(anchor="w", pady=(2, 0))

    classroom_card = card(inner, "Classroom notifications")
    classroom_var = tk.BooleanVar(value=False)
    classroom_path = {"value": ""}
    classroom_row = ttk.Frame(classroom_card, style="Card.TFrame")
    classroom_row.pack(fill="x", pady=(4, 1))
    ttk.Checkbutton(classroom_row, text="Enable Google Classroom", variable=classroom_var,
                    style="Card.TCheckbutton").pack(side="left")
    classroom_label = ttk.Label(classroom_row, text="No OAuth JSON selected", style="CardHint.TLabel")
    classroom_label.pack(side="left", padx=(8, 0), fill="x", expand=True)

    def choose_classroom_json():
        selected = filedialog.askopenfilename(parent=root, title="Choose Google OAuth client JSON",
                                              filetypes=[("JSON files", "*.json"), ("All files", "*.*")])
        if selected:
            classroom_path["value"] = selected
            classroom_label.configure(text=os.path.basename(selected)[:40])
            classroom_var.set(True)

    ttk.Button(classroom_row, text="Choose…", style="Quiet.TButton", command=choose_classroom_json).pack(side="right")

    def suggest_oauth_json():
        if classroom_path["value"]:
            return
        for candidate in oauth_json_candidates():
            if messagebox.askyesno("Google Classroom OAuth",
                                   "Is this the Google OAuth secrets file for Clock?\n\n%s\n\n"
                                   "Choose Yes only if you recognize this file." % candidate,
                                   parent=root):
                classroom_path["value"] = candidate
                classroom_label.configure(text=os.path.basename(candidate)[:40])
                classroom_var.set(True)
                break

    ttk.Label(classroom_card, text="Sign-in happens before any confirmed SD-card erase.",
              style="CardHint.TLabel").pack(anchor="w")

    flash_card = card(inner, "Flash device")
    v_fl = tk.BooleanVar(value=True)
    flash_title = ttk.Frame(flash_card, style="Card.TFrame")
    flash_title.pack(fill="x", pady=(4, 2))
    ttk.Checkbutton(flash_title, text="Flash ESP32 after download/build", variable=v_fl,
                    style="Card.TCheckbutton").pack(side="left")
    device_hint = ttk.Label(flash_title, text="SD drives are excluded", style="CardHint.TLabel")
    device_hint.pack(side="right")
    device_var = tk.StringVar(value="Auto-detect at Start")
    device_row = ttk.Frame(flash_card, style="Card.TFrame")
    device_row.pack(fill="x")
    device_combo = ttk.Combobox(device_row, textvariable=device_var, values=["Auto-detect at Start"],
                                state="readonly", width=41)
    device_combo.pack(side="left", fill="x", expand=True)
    device_ports = {}

    def refresh_devices():
        device_hint.configure(text="Detecting serial devices…")

        def detect():
            try:
                found = serial_ports(find_cli())

                def update():
                    device_ports.clear()
                    values = ["Auto-detect at Start"]
                    for address, board in found:
                        label = "%s — %s" % (address, board)
                        values.append(label)
                        device_ports[label] = address
                    device_combo["values"] = values
                    device_var.set(values[0])
                    device_hint.configure(text="%d serial device(s) found" % len(found))

                root.after(0, update)
            except Exception as exc:
                error_text = str(exc)
                root.after(0, lambda: device_hint.configure(text="Detection failed: %s" % error_text[:42]))

        threading.Thread(target=detect, daemon=True).start()

    ttk.Button(device_row, text="Refresh", style="Quiet.TButton", command=refresh_devices).pack(side="left", padx=(7, 0))

    action = ttk.Frame(inner, style="Card.TFrame", padding=(12, 8))
    action.pack(fill="both", expand=True)
    status = ttk.Label(action, text="READY", style="CardHint.TLabel")
    status.pack(anchor="w")
    bar = ttk.Progressbar(action, maximum=100, style="Setup.Horizontal.TProgressbar")
    bar.pack(fill="x", pady=(4, 6))
    start = ttk.Button(action, text="START SETUP", style="Accent.TButton")
    start.pack(anchor="e")

    def ask_port(ports):
        box = {}
        ev = threading.Event()

        def ask():
            dialog = tk.Toplevel(root)
            dialog.title("Select ESP32 port")
            dialog.transient(root)
            dialog.grab_set()
            ttk.Label(dialog, text="Select the ESP32 serial port (SD drives are excluded):").pack(padx=16, pady=(14, 6))
            values = ["%s — %s" % (address, board) for address, board in ports]
            port_box = ttk.Combobox(dialog, values=values, state="readonly", width=48)
            port_box.current(0)
            port_box.pack(padx=16, pady=4)

            def choose():
                box["p"] = ports[port_box.current()][0] if port_box.current() >= 0 else None
                dialog.destroy()
                ev.set()

            ttk.Button(dialog, text="Use selected port", style="Accent.TButton", command=choose).pack(pady=(6, 14))
            dialog.protocol("WM_DELETE_WINDOW", lambda: (dialog.destroy(), ev.set()))

        root.after(0, ask)
        ev.wait()
        return box.get("p")

    def poll():
        bar["value"] = S.pct
        status["text"] = S.text
        if S.done:
            if S.error:
                messagebox.showerror("Clock Setup", S.error, parent=root)
            else:
                status["text"] = "DONE"
                root.after(1500, root.destroy)
            return
        root.after(150, poll)

    def go():
        selected_mode = mode.get()
        do_dl, do_fl = v_dl.get(), v_fl.get()
        classroom_enabled = classroom_var.get()
        credentials_path = classroom_path["value"]
        if classroom_enabled:
            if not do_dl:
                messagebox.showwarning("Google Classroom",
                                       "Select SD-card installation so authorized data can be saved to /data/secrets.",
                                       parent=root)
                return
            if not credentials_path or not os.path.isfile(credentials_path):
                choose_classroom_json()
                credentials_path = classroom_path["value"]
                if not credentials_path:
                    return
        selected_port = device_ports.get(device_var.get())
        drive = None
        if do_dl:
            i = combo.current()
            if i < 0 or i >= len(drives):
                messagebox.showwarning("Clock Setup", "Pick the SD card first.", parent=root)
                return
            drive = drives[i][0]
            if not update_only and not safe_to_wipe(drive):
                messagebox.showerror("Clock Setup", "%s is not a removable drive. Nothing was changed." % drive,
                                     parent=root)
                return
            if not update_only:
                if not messagebox.askokcancel("WARNING 1 of 2",
                                               "%s will be erased before files are written. Continue?" % drives[i][1],
                                               parent=root):
                    return
                if not messagebox.askyesno("WARNING 2 of 2",
                                            "Erase %s and continue? This cannot be undone." % drive,
                                            default="no", parent=root):
                    return

        manual = None
        if selected_mode == "manual":
            messagebox.showwarning("Manual mode",
                                   "Select a folder containing the .ino and .h files you want to compile.\n\n"
                                   "Selected files are moved into the Clock source workspace.", parent=root)
            folder = filedialog.askdirectory(parent=root, title="Select folder containing Arduino .ino and .h files")
            if not folder:
                return
            manual = [os.path.join(root_dir, name)
                      for root_dir, _, names in os.walk(folder)
                      for name in names if name.lower().endswith((".ino", ".h"))]
            if not manual:
                messagebox.showerror("Manual mode", "The selected folder contains no .ino or .h files.", parent=root)
                return
            problems = check_files(manual, False)
            if problems:
                messagebox.showerror("Missing files", "\n".join(problems), parent=root)
                return

        if selected_mode == "beta" and not messagebox.askyesno(
                "Beta warning — unstable",
                "Beta compiles the newest raw source and may fail or damage a test device. Continue?",
                default="no", parent=root):
            return
        if update_only:
            do_fl = False
        S.pct, S.text, S.done, S.error, S.summary = 0, "Starting...", False, None, []
        start.state(["disabled"])
        combo.state(["disabled"])
        device_combo.state(["disabled"])
        for button in mode_buttons.values():
            button.configure(state="disabled")
        threading.Thread(target=work,
                         args=((drive, selected_mode, do_dl, do_fl, update_only, manual, selected_port,
                                classroom_enabled, credentials_path),
                               ask_port,
                               lambda sketches, default: ask_sketch(sketches, root, default)),
                         daemon=True).start()
        poll()

    start.configure(command=go)
    if update_only:
        v_fl.set(False)
    root.after(300, refresh_devices)
    root.after(600, suggest_oauth_json)
    splash_bar.stop()
    splash.destroy()
    root.deiconify()
    root.mainloop()


if __name__ == "__main__":
    main()
