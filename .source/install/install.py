"""
Clock installer (lives in the GitHub repo at .source/install/install.py)
Launched by setup_sd.bat, which downloads this file fresh, runs it, then deletes it.

Rufus-style window: pick the SD card, then three checkboxes (all ON by default):
  [x] Download source code  - wipe the SD card (2 warnings), build .source/, icons,
                              and put every sketch in its own folder on the card
  [x] Compile source code   - arduino-cli (the engine inside Arduino IDE 2) builds the sketches
  [x] Flash ESP32           - uploads the compiled clock sketch over USB
When it finishes the window closes by itself.

Source code is kept in  <Documents>\\ClockSource\\<name>\\<name>.ino  so you can edit it.
Files in your Downloads (.ino / .h) are MOVED there (never copied).
"python install.py update" = refresh the SD card from GitHub only (no wipe, no icons).
"""
import os, sys, re, json, shutil, stat, threading, ctypes, subprocess, urllib.request, zipfile, tempfile

import tkinter as tk
from tkinter import ttk, messagebox, simpledialog, filedialog

OWNER, REPO, BRANCH = "n0rm0", "Clock", "main"
FILES_PATH = ".source/uncompiled/updates/updateV1"
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
    url = "https://api.github.com/repos/%s/%s/contents/%s?ref=%s" % (OWNER, REPO, path, BRANCH)
    try:
        items = json.loads(http_get(url))
    except Exception:
        return []
    out = []
    for it in items:
        if it["type"] == "file":
            out.append((it["path"][len(FILES_PATH) + 1:], it["download_url"]))
        elif it["type"] == "dir":
            out += list_repo(it["path"])
    return out


def fetch_repo(stage):
    files = list_repo(FILES_PATH)
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
        folder = os.path.join(dest, stems[stem]) if stem in stems else os.path.join(dest, "extras", os.path.dirname(rel))
        put(rel, folder)
    return inos


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
    for d in ("compiled/updates", "compiled/bootloader/fallback", "data", "icons"):
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
    """Copy every sketch folder from the workspace onto the (already wiped) SD card."""
    for e in os.scandir(ws):
        if e.is_dir() and e.name not in ("build", ".build"):
            shutil.copytree(e.path, os.path.join(drive, e.name), dirs_exist_ok=True)


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
        line = line.strip()
        if not line:
            continue
        tail = (tail + [line])[-15:]
        cur = min(hi - 1, cur + 0.2)
        prog(int(cur), "%s  %s" % (label, line[:48]))
    p.wait()
    if p.returncode != 0:
        raise RuntimeError("%s failed:\n%s" % (label, "\n".join(tail)))
    return "\n".join(tail)


def serial_ports(cli):
    try:
        out = subprocess.run([cli, "board", "list", "--format", "json"], capture_output=True, text=True,
                             creationflags=NO_WINDOW, timeout=30).stdout
        data = json.loads(out)
        data = data.get("detected_ports", data) if isinstance(data, dict) else data
        return [d["port"]["address"] for d in data if d.get("port", {}).get("protocol") == "serial"]
    except Exception:
        return []


def build_and_flash(do_compile, do_flash, ask_port):
    ws = workspace()
    sketches = sorted(e.name for e in os.scandir(ws) if e.is_dir() and os.path.isfile(os.path.join(e.path, e.name + ".ino")))
    if not sketches:
        raise RuntimeError("No sketches found in " + ws)
    cli = find_cli()
    prog(55, "Preparing Arduino tools...")
    run_cli(cli, ["core", "update-index", "--additional-urls", ESP_INDEX], 55, 58, "Updating board index")
    run_cli(cli, ["core", "install", CORE, "--additional-urls", ESP_INDEX], 58, 72, "Installing ESP32 board support")
    run_cli(cli, ["lib", "install"] + LIBS, 72, 76, "Installing libraries")
    build = os.path.join(ws, ".build")
    span = 14.0 / len(sketches)
    for i, name in enumerate(sketches):
        lo = int(76 + i * span)
        run_cli(cli, ["compile", "--fqbn", FQBN, "--build-property", "compiler.cpp.extra_flags=" + TFT_FLAGS,
                      "--output-dir", os.path.join(build, name), os.path.join(ws, name)],
                lo, int(lo + span), "Compiling %s" % name)
    if do_flash:
        ports = serial_ports(cli)
        if not ports:
            raise RuntimeError("No ESP32 found. Plug it in with a data USB cable (hold BOOT while plugging in if needed) and run again.")
        port = ports[0] if len(ports) == 1 else ask_port(ports)
        if not port:
            raise Cancelled()
        sk = FLASH_SKETCH if FLASH_SKETCH in sketches else sketches[0]
        run_cli(cli, ["upload", "--fqbn", FQBN, "-p", port, "--input-dir", os.path.join(build, sk),
                      os.path.join(ws, sk)], 91, 99, "Flashing %s to %s" % (sk, port))
        return "Flashed '%s' on %s" % (sk, port), sketches
    return "Compiled: " + ", ".join(sketches), sketches


# ------------------------------------------------------------------ the whole job
def work(opts, ask_port):
    try:
        drive, do_dl, do_cc, do_fl, update_only, manual = opts
        n = m = 0
        inos = []
        if do_dl or do_cc or do_fl:
            prog(3, "Getting your files..." if manual else "Getting latest files from GitHub...")
            n, m, inos = get_source(manual)
            S.summary += ["Source folder: " + workspace(),
                          "Files from GitHub: %d" % n,
                          "Moved from %s: %d" % ("your selection" if manual else "Downloads", m)]
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
        if do_cc or do_fl:
            msg, _ = build_and_flash(do_cc or do_fl, do_fl, ask_port)
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
    root.geometry("440x330")
    root.resizable(False, False)
    root.attributes("-topmost", True)

    pad = {"padx": 16}
    ttk.Label(root, text="Device").pack(anchor="w", pady=(14, 2), **pad)
    drives = removable_drives()
    labels = [d[1] for d in drives]
    combo = ttk.Combobox(root, values=labels, state="readonly", width=58)
    if labels:
        combo.current(0)
    combo.pack(anchor="w", **pad)
    ttk.Label(root, text="Choose your SD card (removable drives only)" if labels
              else "No SD card found - insert it and press Refresh", foreground="#666").pack(anchor="w", **pad)

    def refresh():
        nonlocal drives
        drives = removable_drives()
        combo["values"] = [d[1] for d in drives]
        if drives:
            combo.current(0)
        else:
            combo.set("")

    ttk.Button(root, text="Refresh", command=refresh).pack(anchor="e", padx=16, pady=(2, 6))

    v_dl, v_cc, v_fl = tk.BooleanVar(value=True), tk.BooleanVar(value=True), tk.BooleanVar(value=True)

    def sync(changed):
        if changed == "fl" and v_fl.get():
            v_cc.set(True)          # flashing needs a build
        if changed == "cc" and not v_cc.get():
            v_fl.set(False)

    ttk.Checkbutton(root, text="Download source code  (erases the SD card, then installs)", variable=v_dl).pack(anchor="w", **pad)
    ttk.Checkbutton(root, text="Compile source code", variable=v_cc, command=lambda: sync("cc")).pack(anchor="w", **pad)
    ttk.Checkbutton(root, text="Flash ESP32", variable=v_fl, command=lambda: sync("fl")).pack(anchor="w", **pad)

    status = ttk.Label(root, text="READY", anchor="center", relief="sunken")
    status.pack(fill="x", padx=16, pady=(14, 4))
    bar = ttk.Progressbar(root, length=408, maximum=100)
    bar.pack(**pad)
    start = ttk.Button(root, text="START")
    start.pack(pady=10)

    def ask_port(ports):
        box = {}
        ev = threading.Event()

        def ask():
            box["p"] = simpledialog.askstring("ESP32 port", "More than one port found:\n" + "\n".join(ports) +
                                              "\n\nType the one to use:", initialvalue=ports[0], parent=root)
            ev.set()
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
                root.update()
                root.after(1500, root.destroy)
                status["text"] = "DONE"
                return
            root.destroy()
        else:
            root.after(150, poll)

    def go():
        do_dl, do_cc, do_fl = v_dl.get(), v_cc.get(), v_fl.get()
        if not (do_dl or do_cc or do_fl):
            return
        drive = None
        if do_dl:
            i = combo.current()
            if i < 0 or i >= len(drives):
                messagebox.showwarning("Clock Setup", "Pick the SD card first.", parent=root)
                return
            drive = drives[i][0]
            if not update_only:
                if not safe_to_wipe(drive):
                    messagebox.showerror("Clock Setup", "%s is not a removable drive (or it is your system drive).\nNothing was changed." % drive, parent=root)
                    return
                if not messagebox.askokcancel("WARNING 1 of 2", "%s will be COMPLETELY ERASED before the files are written.\n\nEverything on it will be lost." % drives[i][1],
                                              icon="warning", parent=root):
                    return
                if not messagebox.askyesno("WARNING 2 of 2 - last chance", "This permanently deletes ALL data on %s and cannot be undone.\n\nErase %s and continue?" % (drive, drive),
                                           icon="warning", default="no", parent=root):
                    return
        manual = None
        if not do_dl and (do_cc or do_fl):
            # Download is off: you pick the source files yourself and we check nothing is missing
            picked = filedialog.askopenfilenames(parent=root, title="Select your .ino and .h files (Ctrl+click for several)",
                                                 filetypes=[("Arduino files", "*.ino *.h"), ("All files", "*.*")])
            if not picked:
                return
            problems = check_files(list(picked), do_fl)
            if problems:
                messagebox.showerror("Missing files", "\n".join(problems) + "\n\nSelect all the files together and try again.", parent=root)
                return
            manual = list(picked)
        start.state(["disabled"])
        combo.state(["disabled"])
        threading.Thread(target=work, args=((drive, do_dl, do_cc, do_fl, update_only, manual), ask_port), daemon=True).start()
        poll()

    start.configure(command=go)
    if update_only:
        v_cc.set(False)
        v_fl.set(False)
    root.mainloop()


if __name__ == "__main__":
    main()
