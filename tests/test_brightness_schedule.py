"""Check daily brightness boundaries and migration of the old default slots."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def source(path):
    return (ROOT / path).read_bytes().decode("gb18030")


def function(body, signature):
    start = body.index(signature)
    brace = body.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (body[end] == "{") - (body[end] == "}")
        end += 1
    return body[start:end] + "\n"


backlight = source("main/LCD/backLight.c")
rom = source("main/rom.c")
assert "backLightIdleTick" not in backlight
assert "backLightNoteActivity" not in backlight
assert "idleTimeoutMinutes" not in source("main/PAGE/SETTING/setting.c")

harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
enum { PAGE_HOME, PAGE_BGLIGHT_SCHEDULE };
static int pageNow = PAGE_HOME;
static struct { int tm_hour, tm_min; } clockData, *tStruct=&clockData;
static struct { u16 brightnessInfo[5]; u8 brightnessPresetVersion; } dataSaveInfo;
static u8 backLightNum;
static int writes;
static void setBackLightVal(u8 level){ backLightNum=level; ++writes; }
'''

harness += next(line + "\n" for line in rom.splitlines()
                if line.startswith("#define PREVIOUS_BRIGHTNESS_PRESET_VERSION"))
harness += rom[rom.index("#define BRIGHTNESS_SLOT"):rom.index("static u8 brightnessPresetSaveNeeded;")]
harness += function(rom, "static void loadDefaultBrightnessSchedule(void){")
harness += function(rom, "static u8 hasLegacyBrightnessDefaults(void){")
harness += function(rom, "static u8 shouldMigrateBrightnessSchedule(void){")
harness += function(backlight, "int findLastAlarmBeforeCurrent(")
harness += "static int currentScheduleIndex = -1;\n"
harness += function(backlight, "static int scheduledBrightnessIndex(void){")
harness += function(backlight, "void resetBackLightVal(void){")
harness += function(backlight, "void backLightScheduleTick(void){")
harness += r'''
static void at(int hour,int minute,u8 expected){
    tStruct->tm_hour=hour; tStruct->tm_min=minute;
    backLightScheduleTick();
    assert(backLightNum==expected);
    int previous=writes;
    backLightScheduleTick();
    assert(writes==previous);
}
int main(void){
    dataSaveInfo.brightnessPresetVersion=0xA7;
    for(int i=0;i<5;i++) dataSaveInfo.brightnessInfo[i]=BRIGHTNESS_SLOT(12,8);
    assert(hasLegacyBrightnessDefaults());
    assert(shouldMigrateBrightnessSchedule());
    loadDefaultBrightnessSchedule();
    assert(!hasLegacyBrightnessDefaults());
    dataSaveInfo.brightnessPresetVersion=PREVIOUS_BRIGHTNESS_PRESET_VERSION;
    memcpy(dataSaveInfo.brightnessInfo,previousBrightnessSchedule,
           sizeof(previousBrightnessSchedule));
    assert(shouldMigrateBrightnessSchedule());
    dataSaveInfo.brightnessInfo[0]=BRIGHTNESS_SLOT(7,5);
    assert(!shouldMigrateBrightnessSchedule());
    memcpy(dataSaveInfo.brightnessInfo,previousBrightnessSchedule,
           sizeof(previousBrightnessSchedule));
    dataSaveInfo.brightnessPresetVersion=0xB2;
    assert(!shouldMigrateBrightnessSchedule());
    dataSaveInfo.brightnessPresetVersion=PREVIOUS_BRIGHTNESS_PRESET_VERSION;
    assert(shouldMigrateBrightnessSchedule());
    loadDefaultBrightnessSchedule();
    at(0,0,2); at(6,59,2); at(7,0,4); at(8,59,4);
    at(9,0,8); at(16,59,8); at(17,0,6); at(18,59,6);
    at(19,0,4); at(22,59,4); at(23,0,2); at(23,59,2);
    pageNow=PAGE_BGLIGHT_SCHEDULE;
    at(9,0,2);
    pageNow=PAGE_HOME;
    at(9,0,8);
    dataSaveInfo.brightnessInfo[0]=BRIGHTNESS_SLOT(7,5);
    assert(!hasLegacyBrightnessDefaults());
    assert(dataSaveInfo.brightnessInfo[0]==BRIGHTNESS_SLOT(7,5));
    resetBackLightVal();
    assert(backLightNum==8);
    puts("PASS: brightness schedule boundaries, night wrap, preview, and legacy migration");
}
'''

with tempfile.TemporaryDirectory(prefix="gview-brightness-") as folder:
    test = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    test.write_text(harness, encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
