"""Check that the Pomodoro clock restores its time row after settings."""

from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "main/PAGE/TCLOCK/tclock.c").read_bytes().decode("gb18030")
enum_start = source.index("enum {\n    TOMATO_TIME_LAYER")
enum_end = source.index("};", enum_start) + 2
function_start = source.index("static void placeTomatoTimeRow(void){")
function_end = source.index("\n}\n", function_start) + 2
init_start = source.index("void tomatoPageInit(void){")
init_end = source.index("\n}\n", init_start)
page_init = source[init_start:init_end]
assert "placeTomatoTimeRow();" in page_init
assert page_init.count("addChild(TOMATO_TIME_LAYER,TNum") == 5
assert "placeTomatoTimeRow();" in source[source.index("void tomatoEx(void){"):init_start]

harness = r'''
#include <assert.h>
typedef struct { short x,y; } DisplayObject;
typedef struct { short x,y; } MovieClip;
static struct { MovieClip movieclipArr[10]; } stage;
enum { WS_UI_LEFT=40, WS_UI_MID=160, WS_UI_RIGHT=280, PAGE_TCLCK=12 };
static int pageNow=PAGE_TCLCK;
static DisplayObject objects[5];
static DisplayObject *TNumM2=&objects[0], *TNumM1=&objects[1];
static DisplayObject *TNumM=&objects[2], *TNumS2=&objects[3], *TNumS1=&objects[4];
__ENUM__
__FUNCTION__
static void check_layout(void){
    const int left=TNumM2->x-TOMATO_DIGIT_WIDTH/2;
    const int right=TNumS1->x+TOMATO_DIGIT_WIDTH/2;
    assert(left>=WS_UI_LEFT && right<=WS_UI_RIGHT);
    assert(left+right==2*WS_UI_MID);
    assert(TNumM1->x-TOMATO_DIGIT_WIDTH/2 - (TNumM2->x+TOMATO_DIGIT_WIDTH/2)==TOMATO_GAP);
    assert(TNumM->x - (TNumM1->x+TOMATO_DIGIT_WIDTH/2)==TOMATO_GAP);
    assert(TNumS2->x-TOMATO_DIGIT_WIDTH/2 - (TNumM->x+TOMATO_COLON_WIDTH)==TOMATO_GAP);
    assert(TNumS1->x-TOMATO_DIGIT_WIDTH/2 - (TNumS2->x+TOMATO_DIGIT_WIDTH/2)==TOMATO_GAP);
    assert(TNumM->y+TOMATO_COLON_HEIGHT/2==TNumM2->y);
    assert(TNumM2->y==TNumM1->y && TNumM1->y==TNumS2->y && TNumS2->y==TNumS1->y);
    assert(stage.movieclipArr[4].x==0 && stage.movieclipArr[4].y==0);
}
int main(void){
    stage.movieclipArr[4].x=19;
    stage.movieclipArr[4].y=-27;
    placeTomatoTimeRow();
    check_layout();
    for(int i=0;i<5;++i){ objects[i].x+=23; objects[i].y-=16; }
    stage.movieclipArr[4].x=-11;
    stage.movieclipArr[4].y=8;
    placeTomatoTimeRow();
    check_layout();
    pageNow=0;
    TNumM->x=99;
    placeTomatoTimeRow();
    assert(TNumM->x==99);
}
'''

with tempfile.TemporaryDirectory(prefix="gview-tomato-layout-") as folder:
    test = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    test.write_text(harness.replace("__ENUM__", source[enum_start:enum_end])
                    .replace("__FUNCTION__", source[function_start:function_end]), encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
    print("PASS: Pomodoro time row stays centred after settings return")
