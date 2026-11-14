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
PORT = ROOT / "ports/desktop"
OUT = ROOT / "artifacts/desktop"
BUILD = OUT / "build"
INCLUDE = BUILD / "include"
APP = OUT / "GVIEW Travel Simulator.app"
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
    shutil.copy2(PORT / "lcd_init.h", path)

# Preserve firmware bytes and all algorithms. Only the ESP IRAM annotation is
# unsuitable for Mach-O. Native text's local include must use the host header.
engine = BUILD / "gview.c"
engine.write_bytes((ROOT / "components/GVIEW/gview.c").read_bytes().replace(
    b'#define FAST_CODE __attribute__((section(".iram1")))', b'#define FAST_CODE'))
text = BUILD / "native_text.c"
text.write_bytes((ROOT / "components/LCD/native_text.c").read_bytes())

source = (ROOT / "main/main.c").read_bytes()
start = source.index(b"void checkAlarmClock(void){")
end = source.index(b"// u32 localeTime", start)
tick = BUILD / "runtime_tick.c"
tick.write_bytes(b'#include "config.h"\n#include "rom.h"\n#include "NET/RTC_Internet.h"\n'
                b'#include "PAGE/pageDraw.h"\nstatic u32 systemTimeCount,moneyOld;\n' + source[start:end])

sources = [engine, text, ROOT / "components/GVIEW/utf8_gb2312_switch.c",
           ROOT / "components/LCD/viewport.c", tick,
           *[ROOT / "main" / name for name in (
               "pic_data/picGroup.c", "rom.c", "wifi_ssid.c", "sound_catalog.c", "pinyin.c",
               "PAGE/pageDraw.c", "PAGE/HOME/home.c", "PAGE/HOME/pkmMovie.c", "PAGE/HOME/calendar.c",
               "PAGE/SHOP/shop.c", "PAGE/SHOP/shop_model.c", "PAGE/MENU/menu.c",
               "PAGE/CITY/city.c", "PAGE/TCLOCK/tclock.c", "PAGE/SETTING/setting.c",
               "PAGE/SETTING/clockSetting.c", "PAGE/WEATHER/weather.c", "PAGE/GARDEN/garden.c",
               "PAGE/GARDEN/gardenShop.c", "PAGE/INPUTTXT/inputtxt.c", "PAGE/INPUTTXT/wifi_keyboard.c",
               "PAGE/WIFI/wifiScanPage.c")], PORT / "simulator.c"]
compiler = os.environ.get("CC", "clang")
flags = ["-std=gnu11", "-O1", "-g", "-funsigned-char", "-fno-common",
         "-Wno-invalid-source-encoding", "-Wno-incompatible-pointer-types",
         "-Wno-incompatible-pointer-types-discards-qualifiers", "-Wno-pointer-sign",
         "-Wno-int-conversion", "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast",
         "-Wno-format-security", "-Wno-parentheses-equality", "-Wno-constant-conversion",
         "-Wno-deprecated-declarations", "-Wno-nonportable-include-path", "-Wno-macro-redefined",
         "-I" + str(PORT), "-I" + str(INCLUDE), "-I" + str(ROOT / "components"),
         "-I" + str(ROOT / "components/GVIEW"), "-I" + str(ROOT / "components/LCD"), "-I" + str(ROOT / "main")]
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
                "-c", str(PORT / "main.m"),
                "-o", str(main)], check=True)
binary = APP / "Contents/MacOS/GVIEW-Simulator"
subprocess.run([compiler, *sanitizers, *objects, str(main), "-framework", "Cocoa",
                "-framework", "AVFoundation", "-lm", "-o", str(binary)], check=True)
with (APP / "Contents/Info.plist").open("wb") as f:
    plistlib.dump({"CFBundleExecutable": "GVIEW-Simulator", "CFBundleIdentifier": "cn.gview.travel.simulator",
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
