"""Check the four-item power menu and saved homepage music toggle."""

from pathlib import Path
import subprocess
import tempfile


source = (Path(__file__).resolve().parents[1] / "main/PAGE/MENU/menu.c").read_bytes().decode("gb18030")


def function(signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


production = "\n".join(function(signature) for signature in (
    "static void powerMenuPrevious(void){",
    "static void powerMenuNext(void){",
    "static void powerMenuConfirm(void){",
))

harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t u8;
#define HOME_BGM_SILENT 0
#define HOME_BGM_TRACK_COUNT 10
#define HOME_BGM_DEFAULT_SELECTION 8
static u8 powerMenuSelection;
static struct { u8 homeBgmSelection, homeBgmPreferredSelection; } dataSaveInfo = {8,8};
static int refreshes, writes, settings, poweroffs, homes;
static void powerMenuRefresh(void){ ++refreshes; }
static void writeData(void){ ++writes; }
static void renderMenuInit(void){ ++settings; }
static void requestPowerOff(void){ ++poweroffs; }
static void backToHomePage(void){ ++homes; }
__PRODUCTION__
int main(void){
    powerMenuPrevious();
    assert(powerMenuSelection==3);
    powerMenuConfirm();
    assert(!dataSaveInfo.homeBgmSelection && dataSaveInfo.homeBgmPreferredSelection==8);
    assert(writes==1 && refreshes==2);
    powerMenuConfirm();
    assert(dataSaveInfo.homeBgmSelection==8 && writes==2);
    dataSaveInfo.homeBgmSelection=2;
    powerMenuConfirm();
    assert(dataSaveInfo.homeBgmPreferredSelection==2 && !dataSaveInfo.homeBgmSelection);
    powerMenuConfirm();
    assert(dataSaveInfo.homeBgmSelection==2);
    dataSaveInfo.homeBgmSelection=0;
    dataSaveInfo.homeBgmPreferredSelection=255;
    powerMenuConfirm();
    assert(dataSaveInfo.homeBgmSelection==8);
    powerMenuNext(); assert(powerMenuSelection==0);
    powerMenuConfirm(); assert(settings==1);
    powerMenuNext(); powerMenuConfirm(); assert(poweroffs==1);
    powerMenuNext(); powerMenuConfirm(); assert(homes==1);
    puts("PASS: power menu navigation and remembered homepage music");
}
'''

with tempfile.TemporaryDirectory(prefix="gview-power-menu-") as folder:
    test = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    test.write_text(harness.replace("__PRODUCTION__", production), encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
