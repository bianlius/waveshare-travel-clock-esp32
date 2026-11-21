"""Check main-view swipes and the common middle-click navigation binding."""

from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
adc = (ROOT / "ports/waveshare/adc.c").read_bytes().decode("gb18030")


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
    const int pages[]={PAGE_HOME,PAGE_CALENDAR,PAGE_WEATHER};
    for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);i++){
        pageNow=pages[i];
        assert(content_gesture_key(180,110,70,115)==MAIN_SWIPE_NEXT);
        assert(content_gesture_key(70,110,180,115)==MAIN_SWIPE_PREVIOUS);
        assert(content_gesture_key(70,110,70,110)==-1);
    }
    pageNow=PAGE_CLOCK; // The retired page identifier must not handle main swipes.
    assert(content_gesture_key(180,110,70,115)==-1);
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
    "PAGE/WEATHER/weather.c": ("void weatherInit(void)", "KeyDownM3", "KeyDownM1"),
}
for path, (signature, previous, following) in pages.items():
    source = (ROOT / "main" / path).read_bytes().decode("gb18030")
    init = function(source, signature)
    assert f"mainViewBindKeys({previous},{following});" in init, path
    assert not re.search(r"setKeyFun\(KM\w+", init), path

home = (ROOT / "main/PAGE/HOME/home.c").read_bytes().decode("gb18030")
binding = function(home, "void mainViewBindKeys(")
key_harness = r'''
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
enum { KM1, KM2, KM3, KML, KMR, KMU1, KMU3, KEY_COUNT };
static void (*callbacks[KEY_COUNT])(void);
static int destination;
static void setKeyFun(int key,void (*callback)(void)){callbacks[key]=callback;}
static void mainViewOpenShop(void){destination=1;}
static void mainViewOpenGarden(void){destination=2;}
static void previous(void){destination=3;}
static void next(void){destination=4;}
static void dispatch(int key){if(callbacks[key])callbacks[key]();}
'''
key_scenarios = r'''
int main(void){
    // A stale side-hold callback from the previous page must also be cleared.
    for(int i=0;i<KEY_COUNT;i++)callbacks[i]=previous;
    mainViewBindKeys(previous,next);
    dispatch(KM2);assert(destination==4);
    destination=0;dispatch(KML);dispatch(KMR);assert(destination==0);
    dispatch(KM3);assert(destination==1);
    dispatch(KM1);assert(destination==2);
    dispatch(KMU1);assert(destination==4);
    dispatch(KMU3);assert(destination==3);
    puts("PASS: middle click advances; side holds do nothing; shop, garden and swipes remain available");
}
'''
with tempfile.TemporaryDirectory(prefix="gview-main-keys-") as directory:
    test = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    test.write_text(key_harness + binding + key_scenarios)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("PASS: all three main views use the same middle-click navigation binding")

# Execute the actual page-transition callbacks, including reverse wrapping and
# restoring the selected home view after leaving it for a child page.
calendar = (ROOT / "main/PAGE/HOME/calendar.c").read_bytes().decode("gb18030")
weather = (ROOT / "main/PAGE/WEATHER/weather.c").read_bytes().decode("gb18030")
runtime = (ROOT / "main/app_runtime.c").read_bytes().decode("gb18030")
callbacks = []
for source, prefix in ((home, "home"), (calendar, "calendar"), (weather, "weather")):
    for key, direction in (("KeyDownM1", "next"), ("KeyDownM3", "previous")):
        callback = function(source, f"static void {key}(void)")
        callbacks.append(callback.replace(key, f"{prefix}_{direction}"))

route_harness = r'''
#include <assert.h>
#include <stdio.h>
enum { HOME, CALENDAR, WEATHER };
static unsigned homeState, page;
static void renderHomeInit(void);
static void renderCalendarInit(void);
static void weatherInit(void);
'''
route_scenarios = r'''
static void renderHomeInit(void){page=HOME;homeState=0;}
static void renderCalendarInit(void){page=CALENDAR;homeState=3;}
static void weatherInit(void){page=WEATHER;homeState=1;}
int main(void){
    void (*next[])(void)={home_next,calendar_next,weather_next};
    void (*previous[])(void)={home_previous,calendar_previous,weather_previous};
    renderHomeInit();
    for(unsigned i=0;i<9;i++){
        next[page]();assert(page==(i+1)%3);
        unsigned selected=page;page=99;backToHomePage();assert(page==selected);
    }
    for(unsigned i=0;i<9;i++){
        previous[page]();assert(page==(2-i%3));
    }
    homeState=2;backToHomePage();assert(page==HOME && homeState==0);
    puts("PASS: home/calendar/weather cycle both ways; return restores selection; retired mode falls back to home");
}
'''
with tempfile.TemporaryDirectory(prefix="gview-main-routes-") as directory:
    test = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    test.write_text(route_harness + "\n".join(callbacks)
                    + function(runtime, "void backToHomePage(void)") + route_scenarios)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

assert "clockPageInit" not in (ROOT / "main/PAGE/TCLOCK/tclock.c").read_text(encoding="gb18030")
