"""
Clock installer (lives in the GitHub repo at .source/install/install.py)
Launched by setup_sd.bat, which downloads this file fresh, runs it, then deletes it.

Rufus-style window: pick the SD card, then three checkboxes (all ON by default):
  Auto (recommended)        - download and flash the newest compiled .bin from GitHub
  Beta (unstable)            - download the newest raw sketches and compile them
  Manual                     - choose a folder of .ino/.h files and compile those sketches
  SD install                 - optionally wipe/install source and icons on a selected SD card
When it finishes the window closes by itself.

Source code is kept in  <Documents>\\ClockSource\\<name>\\<name>.ino  so you can edit it.
Files in your Downloads (.ino / .h) are MOVED there (never copied).
"python install.py update" = refresh the SD card from GitHub only (no wipe, no icons).
"""
import os, sys, re, json, shutil, stat, threading, ctypes, subprocess, urllib.request, zipfile, tempfile

import tkinter as tk
from tkinter import ttk, messagebox, simpledialog, filedialog

OWNER, REPO, BRANCH = "n0rm0", "Clock", "main"
SOURCE_ROOT = ".source/uncompiled/updates"
UA = {"User-Agent": "clock-installer"}
ICON_BASE = "https://raw.githubusercontent.com/basmilius/weather-icons/dev/production/fill"
ICONS = ["clear-day", "clear-night", "partly-cloudy-day", "partly-cloudy-night", "cloudy",
         "overcast", "overcast-day", "overcast-night", "rain", "drizzle", "partly-cloudy-day-rain",
         "thunderstorms", "thunderstorms-rain", "thunderstorms-day", "snow", "sleet", "hail",
         "fog", "mist", "haze", "wind", "not-available"]
ICON_SIZE = 96

# Hosyond 4" ESP32-32E = ESP32 Dev Module, 4 MB flash.
# PartitionScheme=min_spiffs ("Minimal SPIFFS: 1.9 MB APP with OTA") = the biggest app size that
# still has two update slots, which GitHub self-update needs. Every downloaded .bin is also kept
# on the SD card as its own file.
FQBN = "esp32:esp32:esp32:PartitionScheme=min_spiffs"
CORE = "esp32:esp32@2.0.17"        # version used by the Hosyond/LCDWIKI demos
ESP_INDEX = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"
LIBS = ["TFT_eSPI", "PNGdec", "ArduinoJson"]
# TFT_eSPI settings passed at compile time (pins from the LCD wiki), so no library file is edited.
TFT_FLAGS = " ".join([
    "-DUSER_SETUP_LOADED=1", "-DST7796_DRIVER=1", "-DTFT_WIDTH=320", "-DTFT_HEIGHT=480",
    "-DTFT_MISO=12", "-DTFT_MOSI=13", "-DTFT_SCLK=14", "-DTFT_CS=15", "-DTFT_DC=2", "-DTFT_RST=-1",
    "-DTFT_BL=27", "-DTFT_BACKLIGHT_ON=HIGH", "-DTOUCH_CS=33", "-DUSE_HSPI_PORT=1",
    "-DLOAD_GLCD=1", "-DLOAD_FONT2=1", "-DLOAD_FONT4=1", "-DLOAD_FONT6=1", "-DLOAD_FONT7=1",
    "-DLOAD_FONT8=1", "-DLOAD_GFXFF=1", "-DSMOOTH_FONT=1",
    "-DSPI_FREQUENCY=40000000", "-DSPI_READ_FREQUENCY=16000000", "-DSPI_TOUCH_FREQUENCY=2500000",
])
FLASH_SKETCH = "updateV1"     # flashed over USB; it updates itself from GitHub afterwards
NO_WINDOW = 0x08000000 if os.name == "nt" else 0


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
    """Find the newest same-name updateV... sketch folder in GitHub."""
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
        if os.path.splitext(os.path.basename(path))[0] == name:
            folders[name] = folder
    if not folders:
        raise RuntimeError("No raw update sketch was found under " + SOURCE_ROOT)
    return max(folders.values(), key=lambda x: (version_key(x), x))


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
    """Remove old generated sketch folders so Beta cannot compile a stale clock copy."""
    current = {os.path.splitext(os.path.basename(x))[0].lower() for x in current_inos}
    if not os.path.isdir(dest):
        return
    for entry in os.scandir(dest):
        if not entry.is_dir() or entry.name.startswith(".") or entry.name.lower().startswith("bootloaderv"):
            continue
        ino = os.path.join(entry.path, entry.name + ".ino")
        if os.path.isfile(ino) and entry.name.lower() not in current:
            shutil.rmtree(entry.path, ignore_errors=True)


def workspace():
    docs = os.path.join(os.path.expanduser("~"), "Documents")
    return os.path.join(docs if os.path.isdir(docs) else os.path.expanduser("~"), "ClockSource")


def get_source(manual=None):
    """GitHub files + Downloads -> <Documents>/ClockSource/<name>/<name>.ino. Returns (github, moved, inos).
    manual = list of files you picked by hand: GitHub and Downloads are skipped; the picked files
    are MOVED into ClockSource (anything else already there stays)."""
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
    for p in picked:                            # picked files win over what is already in ClockSource
        have[os.path.basename(p).lower()] = p
    inos = sorted(n for n in have if n.endswith(".ino"))
    problems = []
    if not inos:
        return ["No .ino file selected. Pick at least updateV1.ino."]
    if need_update and "updateV1.ino" not in have:
        problems.append("updateV1.ino is missing (needed to flash the ESP32).")
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
    for d in ("compiled/updates/updateV1", "compiled/bootloader/fallback/bootloaderV1",
              "uncompiled/updates", "uncompiled/bootloader/fallback", "data", "icons"):
        os.makedirs(os.path.join(drive, ".source", d), exist_ok=True)
    if os.name == "nt":
        os.system('attrib +h "%s"' % os.path.join(drive, ".source"))


def icons(drive, lo, hi):
    try:
        import fitz
    except ImportError:
        subprocess.run([sys.executable, "-m", "pip", "install", "pymupdf"],
                       capture_output=True, creationflags=NO_WINDOW)
        import fitz
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


def put_on_sd(drive, ws):
    """Copy raw sketch folders into the SD card's .source/uncompiled tree."""
    root = os.path.join(drive, ".source", "uncompiled")
    for e in os.scandir(ws):
        if e.is_dir() and e.name not in ("build", ".build"):
            if e.name.lower().startswith("bootloaderv"):
                target = os.path.join(root, "bootloader", "fallback", e.name)
            elif e.name.lower().startswith("updatev"):
                target = os.path.join(root, "updates", e.name)
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


def run_cli(cli, args, lo, hi, label):
    """Run arduino-cli hidden; creep the bar from lo to hi while it prints."""
    prog(lo, label)
    p = subprocess.Popen([cli] + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
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


def latest_binary():
    """Find the newest compiled .bin in the GitHub update tree."""
    url = "https://api.github.com/repos/%s/%s/git/trees/%s?recursive=1" % (OWNER, REPO, BRANCH)
    data = json.loads(http_get(url))
    prefix = ".source/compiled/updates/"
    paths = [item.get("path", "") for item in data.get("tree", [])
             if item.get("type") == "blob" and item.get("path", "").startswith(prefix)
             and item.get("path", "").lower().endswith(".bin")]
    if not paths:
        raise RuntimeError("No compiled .bin firmware is available in GitHub at " + prefix)
    return max(paths, key=lambda x: (version_key(x), x))


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
    """Upload a downloaded .bin through arduino-cli without compiling it."""
    folder = tempfile.mkdtemp(prefix="clock_flash_")
    try:
        stem = os.path.splitext(os.path.basename(binary))[0]
        cli_binary = os.path.join(folder, stem + ".ino.bin")
        shutil.copy2(binary, cli_binary)
        run_cli(cli, ["upload", "--fqbn", FQBN, "-p", port, "--input-dir", folder],
                88, 99, "Flashing %s to %s" % (label, port))
    finally:
        shutil.rmtree(folder, ignore_errors=True)


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
    """Ask which compiled sketch should be flashed; manual mode can select bootloaderV1."""
    if len(sketches) == 1:
        return sketches[0]
    box, ev = {}, threading.Event()
    def ask():
        dialog = tk.Toplevel(root)
        dialog.title("Select firmware to flash")
        dialog.transient(root)
        dialog.grab_set()
        ttk.Label(dialog, text="Select the compiled firmware to upload:").pack(padx=16, pady=(14, 6))
        combo = ttk.Combobox(dialog, values=sketches, state="readonly", width=42)
        combo.current(sketches.index(default) if default in sketches else 0)
        combo.pack(padx=16, pady=4)
        def choose():
            box["name"] = combo.get()
            dialog.destroy(); ev.set()
        ttk.Button(dialog, text="Use selected firmware", command=choose).pack(pady=(6, 14))
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
        sketches = [name for name in sketches if name.lower().startswith("updatev")]
        if not sketches:
            raise RuntimeError("No updateV sketch was found in the newest raw source.")
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
    target = ask_target(sketches, FLASH_SKETCH if mode == "beta" else sketches[0])
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
    try:
        drive, mode, do_dl, do_fl, update_only, manual, selected_port = opts
        n = m = 0
        inos = []
        if mode in ("beta", "manual") or do_dl:
            prog(3, "Getting raw source..." if mode != "manual" else "Reading selected folder...")
            n, m, inos = get_source(manual)
            S.summary += ["Source folder: " + workspace(),
                          "Files from GitHub: %d" % n,
                          "Copied from selection/Downloads: %d" % m]
        if do_dl:
            if not update_only:
                prog(8, "Erasing SD card...")
                wipe(drive)
            prog(15, "Creating folders on the SD card...")
            structure(drive)
            put_on_sd(drive, workspace())
            S.summary.append("SD card: %s  (sketches: %s)" % (drive, ", ".join(inos) or "none"))
            if not update_only:
                ok, bad = icons(drive, 20, 52)
                S.summary.append("Icons: %d/%d%s" % (ok, len(ICONS), "  (failed: " + ", ".join(bad) + ")" if bad else ""))
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
        S.done = True


# ------------------------------------------------------------------ UI (Rufus style)
def main():
    update_only = "update" in [a.lower() for a in sys.argv[1:]]
    root = tk.Tk()
    root.title("Clock Setup")
    root.geometry("560x500")
    root.resizable(False, False)
    root.attributes("-topmost", True)
    pad = {"padx": 16}

    ttk.Label(root, text="Clock setup", font=("Segoe UI", 13, "bold")).pack(anchor="w", pady=(14, 2), **pad)
    ttk.Label(root, text="Auto flashes the newest clock .bin. Beta compiles the newest clock source.",
              foreground="#555").pack(anchor="w", **pad)

    mode = tk.StringVar(value="auto")
    modes = ttk.Frame(root)
    modes.pack(anchor="w", pady=(10, 2), **pad)
    ttk.Radiobutton(modes, text="Auto (recommended)", variable=mode, value="auto").pack(side="left")
    ttk.Radiobutton(modes, text="Beta (unstable)", variable=mode, value="beta").pack(side="left", padx=(12, 0))
    ttk.Radiobutton(modes, text="Manual", variable=mode, value="manual").pack(side="left", padx=(12, 0))

    def info():
        messagebox.showinfo("Clock setup modes",
            "Auto (recommended): downloads and flashes the newest clock application .bin from GitHub. It never downloads the fallback bootloader.\n\n"
            "Beta (unstable): downloads the newest raw clock update .ino/.h files from GitHub, compiles the application only, and can flash it.\n\n"
            "Manual: choose a folder containing the .ino and .h files you want to compile. It can compile all selected sketches and flash the selected result. Verify the folder before continuing.",
            parent=root)
    ttk.Button(modes, text="Info", command=info).pack(side="left", padx=(12, 0))

    ttk.Label(root, text="SD card (optional)").pack(anchor="w", pady=(12, 2), **pad)
    drives = removable_drives()
    labels = [d[1] for d in drives]
    combo = ttk.Combobox(root, values=labels, state="readonly", width=60)
    if labels:
        combo.current(0)
    combo.pack(anchor="w", **pad)
    ttk.Label(root, text="Install source and icons to a removable SD card" if labels else "No SD card selected",
              foreground="#666").pack(anchor="w", **pad)

    def refresh():
        nonlocal drives
        drives = removable_drives()
        combo["values"] = [d[1] for d in drives]
        combo.current(0) if drives else combo.set("")
    ttk.Button(root, text="Refresh drives", command=refresh).pack(anchor="e", padx=16, pady=(2, 4))

    v_dl = tk.BooleanVar(value=False)
    v_fl = tk.BooleanVar(value=True)
    ttk.Checkbutton(root, text="Install source and weather icons to selected SD card", variable=v_dl).pack(anchor="w", **pad)
    ttk.Checkbutton(root, text="Flash ESP32 after download/build", variable=v_fl).pack(anchor="w", **pad)

    ttk.Label(root, text="Flash device (serial ESP32 port)").pack(anchor="w", pady=(10, 2), **pad)
    device_var = tk.StringVar(value="Auto-detect at Start")
    device_combo = ttk.Combobox(root, textvariable=device_var,
                                values=["Auto-detect at Start"], state="readonly", width=66)
    device_combo.pack(anchor="w", **pad)
    device_hint = ttk.Label(root, text="SD-card drive letters are excluded.", foreground="#666")
    device_hint.pack(anchor="w", **pad)
    device_ports = {}

    def refresh_devices():
        device_hint["text"] = "Detecting ESP32 serial devices..."
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
                    device_hint["text"] = "%d ESP32 serial device(s) found." % len(found)
                root.after(0, update)
            except Exception as exc:
                error_text = str(exc)
                root.after(0, lambda: device_hint.configure(text="Device detection failed: %s" % error_text))
        threading.Thread(target=detect, daemon=True).start()

    ttk.Button(root, text="Refresh flash devices", command=refresh_devices).pack(anchor="e", padx=16, pady=(2, 4))
    root.after(300, refresh_devices)

    status = ttk.Label(root, text="READY", anchor="center", relief="sunken")
    status.pack(fill="x", padx=16, pady=(14, 4))
    bar = ttk.Progressbar(root, length=468, maximum=100)
    bar.pack(**pad)
    start = ttk.Button(root, text="START")
    start.pack(pady=10)

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
            ttk.Button(dialog, text="Use selected port", command=choose).pack(pady=(6, 14))
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
        selected_port = device_ports.get(device_var.get())
        drive = None
        if do_dl:
            i = combo.current()
            if i < 0 or i >= len(drives):
                messagebox.showwarning("Clock Setup", "Pick the SD card first.", parent=root)
                return
            drive = drives[i][0]
            if not update_only and not safe_to_wipe(drive):
                messagebox.showerror("Clock Setup", "%s is not a removable drive. Nothing was changed." % drive, parent=root)
                return
            if not update_only:
                if not messagebox.askokcancel("WARNING 1 of 2", "%s will be erased before files are written. Continue?" % drives[i][1], parent=root):
                    return
                if not messagebox.askyesno("WARNING 2 of 2", "Erase %s and continue? This cannot be undone." % drive, default="no", parent=root):
                    return

        manual = None
        if selected_mode == "manual":
            messagebox.showwarning("Manual mode", "Select a folder containing the .ino and .h files you want to compile.\n\nUse Info for details. Manual files are copied, not deleted.", parent=root)
            folder = filedialog.askdirectory(parent=root, title="Select folder containing Arduino .ino and .h files")
            if not folder:
                return
            manual = [os.path.join(root_dir, name)
                      for root_dir, _, names in os.walk(folder)
                      for name in names if name.lower().endswith((".ino", ".h"))]
            if not manual:
                messagebox.showerror("Manual mode", "The selected folder contains no .ino or .h files.", parent=root)
                return
            problems = check_files(manual, selected_mode == "beta" and do_fl)
            if problems:
                messagebox.showerror("Missing files", "\n".join(problems), parent=root)
                return

        if selected_mode == "beta" and not messagebox.askyesno("Beta warning — unstable", "Beta compiles the newest raw source and may fail or damage a test device. Continue?", default="no", parent=root):
            return
        if update_only:
            do_fl = False
        start.state(["disabled"])
        combo.state(["disabled"])
        device_combo.state(["disabled"])
        threading.Thread(target=work, args=((drive, selected_mode, do_dl, do_fl, update_only, manual, selected_port),
                                            ask_port,
                                            lambda sketches, default: ask_sketch(sketches, root, default)),
                         daemon=True).start()
        poll()

    start.configure(command=go)
    if update_only:
        v_fl.set(False)
    root.mainloop()


if __name__ == "__main__":
    main()
