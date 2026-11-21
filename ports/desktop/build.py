"""Build the current firmware UI as a native macOS simulator (no SDL needed)."""
from pathlib import Path
import os
import plistlib
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT / "tools"))
from project_sources import shared_sources
PORT = ROOT / "ports/desktop"
PASSPORT = "--passport" in sys.argv
BOARD = ROOT / "ports/ai-passport"
OUT = ROOT / ("artifacts/desktop-passport" if PASSPORT else "artifacts/desktop")
BUILD = OUT / "build"
INCLUDE = BUILD / "include"
APP = OUT / ("GVIEW Travel Passport Simulator.app" if PASSPORT else "GVIEW Travel Simulator.app")
for folder in (INCLUDE, APP / "Contents/MacOS"):
    folder.mkdir(parents=True, exist_ok=True)

headers = ["sdkconfig.h", "freertos/FreeRTOS.h", "freertos/task.h", "freertos/event_groups.h",
           "freertos/semphr.h", "esp_system.h", "esp_wifi.h", "esp_event.h", "esp_log.h",
           "esp_attr.h", "esp_timer.h", "esp_random.h", "esp_heap_caps.h", "esp_chip_info.h",
           "esp_flash.h", "esp_partition.h", "nvs.h", "nvs_flash.h", "driver/i2c_master.h"]
for name in headers:
    path = INCLUDE / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('#include "compat.h"\n')
for name in ("lcd_init.h", "LCD/lcd_init.h"):
    path = INCLUDE / name
    path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(BOARD / "include/LCD/lcd_init.h" if PASSPORT else PORT / "lcd_init.h", path)

# Preserve firmware bytes and all algorithms. Only the ESP IRAM annotation is
# unsuitable for Mach-O. Native text's local include must use the host header.
engine = BUILD / "gview.c"
engine.write_bytes((ROOT / "components/GVIEW/gview.c").read_bytes().replace(
    b'#define FAST_CODE __attribute__((section(".iram1")))', b'#define FAST_CODE'))
text = BUILD / "native_text.c"
text.write_bytes((BOARD / "main/native_text.c" if PASSPORT else ROOT / "ports/waveshare/components/LCD/native_text.c").read_bytes())

# Compile the production runtime and pages directly, without extracting a
# second alarm implementation or maintaining another application source list.
sources = [engine if path == ROOT / "components/GVIEW/gview.c" else path
           for path in shared_sources("renderer", "viewport", "app")]
sources.extend([text, PORT / "simulator.c"])
if PASSPORT:
    sources.extend([BOARD / "main/overlay.c",BOARD / "main/passport_input.c",BOARD / "main/passport_controls.c"])
compiler = os.environ.get("CC", "clang")
flags = ["-std=gnu11", "-O1", "-g", "-funsigned-char", "-fno-common",
         "-Wno-invalid-source-encoding", "-Wno-incompatible-pointer-types",
         "-Wno-incompatible-pointer-types-discards-qualifiers", "-Wno-pointer-sign",
         "-Wno-int-conversion", "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast",
         "-Wno-format-security", "-Wno-parentheses-equality", "-Wno-constant-conversion",
         "-Wno-deprecated-declarations", "-Wno-nonportable-include-path", "-Wno-macro-redefined",
         "-I" + str(PORT), "-I" + str(INCLUDE), "-I" + str(ROOT / "components"),
         "-I" + str(ROOT / "components/GVIEW"), "-I" + str(ROOT / "components/GVIEW"), "-I" + str(ROOT / "main")]
if PASSPORT:
    flags += ["-DCONFIG_GVIEW_AI_PASSPORT=1", "-I"+str(BOARD / "include")]
else:
    flags += ["-I"+str(ROOT / "ports/waveshare/include")]
sanitizers = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if os.environ.get("GVIEW_ASAN") == "1" else []
def compile_source(item):
    index, path = item
    obj = BUILD / f"part-{index}.o"
    result = subprocess.run([compiler, *flags, *sanitizers, "-c", str(path), "-o", str(obj)],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return str(obj), result

with ThreadPoolExecutor(max_workers=4) as pool:
    results = list(pool.map(compile_source, enumerate(sources)))
for obj, result in results:
    if result.stdout:
        print(result.stdout.decode("utf8", errors="replace"))
if any(result.returncode for _, result in results):
    raise SystemExit("Desktop C compilation failed; see diagnostics above")
objects = [obj for obj, _ in results]
main = BUILD / "main.o"
subprocess.run([compiler, "-fobjc-arc", "-O1", "-g", *sanitizers,
                *( ["-DCONFIG_GVIEW_AI_PASSPORT=1"] if PASSPORT else []), "-c", str(PORT / "main.m"),
                "-o", str(main)], check=True)
binary = APP / "Contents/MacOS/GVIEW-Simulator"
subprocess.run([compiler, *sanitizers, *objects, str(main), "-framework", "Cocoa",
                "-framework", "AVFoundation", "-lm", "-o", str(binary)], check=True)
with (APP / "Contents/Info.plist").open("wb") as f:
    plistlib.dump({"CFBundleExecutable": "GVIEW-Simulator", "CFBundleIdentifier": "cn.gview.travel.passport.simulator" if PASSPORT else "cn.gview.travel.simulator",
                  "CFBundleName": "GVIEW Travel Simulator", "CFBundlePackageType": "APPL",
                  "CFBundleVersion": "1", "NSHighResolutionCapable": True}, f)
resources = APP / "Contents/Resources"
resources.mkdir(exist_ok=True)
subprocess.run([sys.executable, str(ROOT / "tools/pack_sound_data.py")], check=True)
shutil.copy2(ROOT / "build/sounddata.bin", resources / "sounddata.bin")
for slot in range(10):
    name = f"track_{slot:02d}.mp3"
    shutil.copy2(ROOT / "main/assets" / name, resources / name)
print(f"Built {APP}")
