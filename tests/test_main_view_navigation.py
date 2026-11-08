"""Check main-view swipe actions and the shared physical key bindings."""

from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
adc = (ROOT / "main/adc.c").read_bytes().decode("gb18030")


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


gesture = function(adc, "static int content_gesture_key(int sx, int sy, int ex, int ey)")
assert "gesture_key == MAIN_SWIPE_NEXT) keyState |= 1 << 1" in adc
assert "gesture_key == MAIN_SWIPE_PREVIOUS) keyState |= 1 << 5" in adc

harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#define PAGE_HOME 1
#define PAGE_CALENDAR 2
#define PAGE_CLOCK 3
#define PAGE_WEATHER 4
#define PAGE_MENU 5
#define PAGE_POWER_MENU 6
#define PAGE_SHOP 7
#define PAGE_GARDENSHOP 8
#define WS_UI_HEADER_HEIGHT 24
#define WS_UI_FOOTER_TOP 220
#define MAIN_SWIPE_NEXT 5
#define MAIN_SWIPE_PREVIOUS 6
static int pageNow,softKeyHeaderVisible;
static struct { struct { int y; } movieclipArr[10]; } stage;
static const int moveClipMaxNum=10;
static int pageReturnHintVisible(void){return 0;}
'''
scenarios = r'''
int main(void){
    for(int page=PAGE_HOME;page<=PAGE_WEATHER;page++){
        pageNow=page;
        assert(content_gesture_key(180,110,70,115)==MAIN_SWIPE_NEXT);
        assert(content_gesture_key(70,110,180,115)==MAIN_SWIPE_PREVIOUS);
        assert(content_gesture_key(70,110,70,110)==-1);
    }
    pageNow=PAGE_MENU;
    assert(content_gesture_key(180,110,70,115)==0);
    puts("PASS: main swipes retain page cycling; main taps do nothing");
}
'''
with tempfile.TemporaryDirectory(prefix="gview-main-nav-") as directory:
    test = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    test.write_text(harness + gesture + scenarios)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

pages = {
    "PAGE/HOME/home.c": ("void renderHomeInit(void)", "KeyDownM3", "KeyDownM1"),
    "PAGE/HOME/calendar.c": ("void renderCalendarInit(void)", "KeyDownM3", "KeyDownM1"),
    "PAGE/TCLOCK/tclock.c": ("void clockPageInit(void)", "CLOCK_KeyDownM3", "CLOCK_KeyDownM1"),
    "PAGE/WEATHER/weather.c": ("void weatherInit(void)", "KeyDownM3", "KeyDownM1"),
}
for path, (signature, previous, following) in pages.items():
    source = (ROOT / "main" / path).read_bytes().decode("gb18030")
    init = function(source, signature)
    binds = dict(re.findall(r"setKeyFun\((KM\w+),\s*&?(\w+)\)", init))
    assert binds["KM3"] == "mainViewOpenShop", path
    assert binds["KM1"] == "mainViewOpenGarden", path
    assert binds["KML"] == binds["KMU3"] == previous, path
    assert binds["KMR"] == binds["KMU1"] == following, path
print("PASS: four main views bind short keys to destinations and holds to page cycling")
