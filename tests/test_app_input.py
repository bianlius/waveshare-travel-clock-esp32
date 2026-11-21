"""Exercise shared three-button actions and the actual Passport adapter."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
production = "\n".join(line for line in (ROOT / "main/app_input.c").read_text().splitlines()
                       if not line.startswith("#include"))
harness = r'''
#include <assert.h>
#include <stdio.h>
#include "app_input.h"
#include "passport_input.h"
typedef enum { KM1, KM2, KM3, KMU1, KMU2, KMU3, KML, KMR } KeyName;
static int backHomeTime, aClockTimes, softKeyHeaderVisible;
static int headerActions[3] = {2, 1, 0};
static int lastKey=-1, dispatched, menus, stops, hides;
static int pageUIKeyAction(int slot){return headerActions[slot];}
static void soundStopAll(void){++stops;}
static void hideTips(void){++hides;}
static void renderPowerMenuInit(void){++menus;}
void doKeyFun(KeyName key){lastKey=key;++dispatched;}
__PRODUCTION__
static void check(void (*dispatch)(int,bool)){
    softKeyHeaderVisible=0;
    const int expected[3]={KM3, KM2, KM1};
    for(int slot=0;slot<3;++slot){
        lastKey=-1;backHomeTime=0;
        dispatch(slot,false);
        assert(lastKey==expected[slot] && backHomeTime==120);
    }
    dispatch(0,true);assert(lastKey==KML);
    dispatch(2,true);assert(lastKey==KMR);
    int before=dispatched, priorMenus=menus;
    dispatch(1,true);assert(menus==priorMenus+1 && dispatched==before);
    softKeyHeaderVisible=1;headerActions[0]=3;headerActions[2]=-1;
    dispatch(0,false);assert(lastKey==KML);
    before=dispatched;dispatch(2,false);assert(dispatched==before);
    dispatch(1,false);assert(lastKey==KM2);
    headerActions[2]=9;before=dispatched;
    dispatch(2,false);assert(dispatched==before);
    for(int slot=0;slot<3;++slot){
        for(int held=0;held<2;++held){
            aClockTimes=4;before=dispatched;priorMenus=menus;
            int priorStops=stops,priorHides=hides;
            dispatch(slot,held);
            assert(!aClockTimes && dispatched==before && menus==priorMenus);
            assert(stops==priorStops+1 && hides==priorHides+1);
        }
    }
}
static void passport(int slot,bool held){passport_dispatch(slot+(held?3:0));}
int main(void){
    check(appInputDispatch);check(passport);
    backHomeTime=7;
    appInputDispatch(-1,false);appInputDispatch(3,true);
    passport_dispatch(-1);passport_dispatch(6);
    assert(backHomeTime==7);
    assert(appInputAction(-1,false)==-1 && appInputAction(3,true)==-1);
    assert(appInputAction(1,true)==-1);
    puts("PASS: shared short/long actions, header promotion, disabled keys, alarm dismissal and Passport mapping");
}
'''
with tempfile.TemporaryDirectory(prefix="travel-app-input-") as temp:
    folder=Path(temp);source=folder/"test.c";binary=folder/"test"
    source.write_text(harness.replace("__PRODUCTION__",production))
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
                    "-fsanitize=address,undefined","-I"+str(ROOT/"main"),
                    "-I"+str(ROOT/"ports/ai-passport/include"),str(source),
                    str(ROOT/"ports/ai-passport/main/passport_controls.c"),
                    "-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
