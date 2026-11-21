"""Check target-specific action menus and the saved homepage music toggle."""

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


declarations = source[source.index("enum {\n    POWER_MENU_SETTINGS"):source.index("static void powerMenuEx(")]
# Display objects are not needed to exercise the actual menu actions.
declarations = "\n".join(line for line in declarations.splitlines() if "DisplayObject" not in line)
production = declarations + "\n" + "\n".join(function(signature) for signature in (
    "static void powerMenuPrevious(void){",
    "static void powerMenuNext(void){",
    "static void powerMenuConfirm(void){",
))

harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
#define HOME_BGM_SILENT 0
#define HOME_BGM_TRACK_COUNT 10
#define HOME_BGM_DEFAULT_SELECTION 8
static struct { u8 homeBgmSelection, homeBgmPreferredSelection; } dataSaveInfo = {8,8};
static int refreshes, writes, settings, poweroffs, homes;
static void powerMenuRefresh(void){ ++refreshes; }
static void writeData(void){ ++writes; }
static void renderMenuInit(void){ ++settings; }
#if !defined(CONFIG_GVIEW_AI_PASSPORT)
static void requestPowerOff(void){ ++poweroffs; }
#endif
static void backToHomePage(void){ ++homes; }
__PRODUCTION__
int main(void){
    assert(!strcmp(powerMenuLabels[POWER_MENU_SETTINGS],"设置"));
    assert(!strcmp(powerMenuLabels[POWER_MENU_HOME],"返回主界面"));
    assert(powerMenuLabels[POWER_MENU_MUSIC]==powerMenuMusicText);
#if defined(CONFIG_GVIEW_AI_PASSPORT)
    assert(POWER_MENU_COUNT==3);
    for(unsigned i=0;i<POWER_MENU_COUNT;++i)assert(strcmp(powerMenuLabels[i],"关机"));
#else
    assert(POWER_MENU_COUNT==4 && !strcmp(powerMenuLabels[POWER_MENU_SHUTDOWN],"关机"));
#endif
    powerMenuPrevious();
    assert(powerMenuSelection==POWER_MENU_MUSIC);
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
    powerMenuNext();
#if !defined(CONFIG_GVIEW_AI_PASSPORT)
    powerMenuConfirm(); assert(poweroffs==1);
    powerMenuNext();
#endif
    assert(powerMenuSelection==POWER_MENU_HOME);
    powerMenuConfirm(); assert(homes==1);
#if defined(CONFIG_GVIEW_AI_PASSPORT)
    assert(poweroffs==0);
#endif
    puts("PASS: power menu navigation and remembered homepage music");
}
'''

with tempfile.TemporaryDirectory(prefix="gview-power-menu-") as folder:
    test = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    test.write_text(harness.replace("__PRODUCTION__", production), encoding="utf-8")
    for defines in ([], ["-DCONFIG_GVIEW_AI_PASSPORT=1"]):
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", *defines, str(test), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
