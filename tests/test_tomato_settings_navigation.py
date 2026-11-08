"""Run the production Pomodoro settings handlers through browse and edit modes."""

from pathlib import Path
import subprocess
import tempfile


source = (Path(__file__).resolve().parents[1] / "main/PAGE/TCLOCK/tclock.c").read_bytes().decode("gb18030")


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
    "static void refreshTomatoSetKeyHints(void){",
    "static u8 stepTomatoSettingValue(",
    "static void adjustTomatoSetting(int direction){",
    "static void KeyDownM3(void){",
    "static void KeyDownM2(void){",
    "static void KeyDownM1(void){",
))

assert 'drawPageUITxt("上一项","编辑","下一项")' in source
for binding in ("KM1,KeyDownM1", "KM2,KeyDownM2", "KM3,KeyDownM3"):
    assert f"setKeyFun({binding})" in source

harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
enum { PAGE_TCLCK, PAGE_TCLCK_SET, moveClipMaxNum=5, WS_UI_HEADER_HEIGHT=24 };
static int pageNow=PAGE_TCLCK_SET, tomatoPageStartFlag;
static u8 tomatoSetSelect, tomatoSetEditing;
static unsigned tomatoSetNumTemp1=25, tomatoSetNumTemp2=5;
static u8 tomatoSetNumTemp3=4;
static struct { u8 tomatoRestRing, tomatoWorkRing; } dataSaveInfo = {3, 6};
static struct { struct { int y; } movieclipArr[5]; } stage;
static const char *hints[3];
static int movedTo=-1, refreshed, opened, shown, hidden;
static void setPageUIKeyText(int cell,const char *label){ hints[cell]=label; }
static void setPageUIMiddleText(const char *label){ hints[1]=label; }
static void moveTClockSetSelect(u8 selected){ movedTo=selected; }
static void arrangeTClockSetVal(void){ ++refreshed; }
static void showTClockTitle(void){ ++shown; }
static void hideTClockTitle(void){ ++hidden; }
static void tomatoPageSet(void){ ++opened; }
__PRODUCTION__
static void expect(const char *left,const char *middle,const char *right){
    assert(strcmp(hints[0],left)==0);
    assert(strcmp(hints[1],middle)==0);
    assert(strcmp(hints[2],right)==0);
}
int main(void){
    refreshTomatoSetKeyHints();
    expect("上一项","编辑","下一项");
    KeyDownM3();
    assert(tomatoSetSelect==4 && movedTo==4 && dataSaveInfo.tomatoWorkRing==6);
    KeyDownM1();
    assert(tomatoSetSelect==0 && movedTo==0 && tomatoSetNumTemp1==25);
    KeyDownM2();
    assert(tomatoSetEditing==1);
    expect("减少","完成","增加");
    KeyDownM1();
    assert(tomatoSetNumTemp1==26 && tomatoSetSelect==0);
    KeyDownM3();
    assert(tomatoSetNumTemp1==25);
    tomatoSetNumTemp1=1;
    KeyDownM3();
    assert(tomatoSetNumTemp1==99);
    KeyDownM1();
    assert(tomatoSetNumTemp1==1);
    KeyDownM2();
    assert(tomatoSetEditing==0);
    expect("上一项","编辑","下一项");
    KeyDownM1();
    assert(tomatoSetSelect==1 && tomatoSetNumTemp2==5);
    KeyDownM2();
    KeyDownM3();
    assert(tomatoSetNumTemp2==4);
    KeyDownM2();
    KeyDownM1();
    assert(tomatoSetSelect==2);
    KeyDownM2();
    tomatoSetNumTemp3=0;
    KeyDownM3();
    assert(tomatoSetNumTemp3==99);
    KeyDownM2();
    KeyDownM1();
    KeyDownM2();
    dataSaveInfo.tomatoRestRing=9;
    KeyDownM1();
    assert(dataSaveInfo.tomatoRestRing==0 && tomatoSetSelect==3);
    KeyDownM2();
    KeyDownM1();
    KeyDownM2();
    dataSaveInfo.tomatoWorkRing=0;
    KeyDownM3();
    assert(dataSaveInfo.tomatoWorkRing==9 && tomatoSetSelect==4);
    assert(refreshed==8);
    pageNow=PAGE_TCLCK;
    tomatoPageStartFlag=0;
    KeyDownM2();
    assert(opened==1 && shown==0 && hidden==0);
    puts("PASS: Pomodoro settings browse, edit, wrap and timer entry");
}
'''

with tempfile.TemporaryDirectory(prefix="gview-tomato-settings-") as folder:
    test = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    test.write_text(harness.replace("__PRODUCTION__", production), encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
