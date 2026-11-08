"""Exercise timer-owned sound settings and the timer's long-right action."""

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


assert 'pageNow=PAGE_CLOCKCOUNT_SET;' in function("static void clockCountSettingsInit(void){")
assert 'setKeyFun(KML,&leaveTimerSoundSettings);' in source
assert 'setKeyFun(KM2,&timerSoundConfirm);' in source
assert 'setPageUILongKeyText("返回",CCTimeHd ? "停止"' in source

production = "\n".join(function(signature) for signature in (
    "void saveTimerSoundSettingsIfDirty(void){",
    "static void queueTimerSoundSave(void){",
    "void CC_KeyDownMR(void){",
    "static void updateTimerSoundKeyHints(void){",
    "static void adjustTimerSound(int direction){",
    "static void timerSoundPrevious(void){",
    "static void timerSoundNext(void){",
    "static void timerSoundConfirm(void){",
    "static void leaveTimerSoundSettings(void){",
))

harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
#define SOUND_TRACK_COUNT 10
#define TIMER_SOUND_SET_COUNT 2
static struct { u8 clockCountRing,clockCountTimes; } dataSaveInfo = {0,5};
static u8 timerSoundSelect,timerSoundEditing,timerSoundDirty;
static u8 CCTimeHd,CCTimeMin,CCTimeSec;
static u16 CCTimeTemp;
static const char *hints[3];
static int opened,stopped,updated,selected,writes,scheduled,returned,cleared;
static void setPageUIKeyText(int index,const char *text){ hints[index]=text; }
static void setPageUIMiddleText(const char *text){ hints[1]=text; }
static void updateTimerSoundValues(void){ ++updated; }
static void moveTimerSoundSelect(void){ ++selected; }
static void clockCountSettingsInit(void){ ++opened; }
static void clockCountPageInit(void){ ++returned; }
static void CCstop(void){ ++stopped; CCTimeHd=0; }
static void updateTTime(u16 time){ assert(time==CCTimeTemp); ++cleared; }
static void writeData(void){ ++writes; }
static void clearDelayFunction(void (*fn)(void)){ (void)fn; }
static void DelayFunction(void (*fn)(void),float seconds){
    (void)fn; assert(seconds==2.0f); ++scheduled;
}
__PRODUCTION__
static void expect(const char *left,const char *middle,const char *right){
    assert(strcmp(hints[0],left)==0);
    assert(strcmp(hints[1],middle)==0);
    assert(strcmp(hints[2],right)==0);
}
int main(void){
    CCTimeHd=1; CC_KeyDownMR();
    assert(stopped==1 && opened==0 && cleared==0);
    CCTimeMin=1; CCTimeTemp=60; CC_KeyDownMR();
    assert(CCTimeTemp==0 && CCTimeMin==0 && cleared==1 && opened==0);
    CC_KeyDownMR();
    assert(opened==1);

    updateTimerSoundKeyHints();
    expect("上一项","编辑","下一项");
    timerSoundPrevious();
    assert(timerSoundSelect==1 && selected==1);
    timerSoundConfirm();
    expect("减少","完成","增加");
    dataSaveInfo.clockCountTimes=0;
    timerSoundPrevious();
    assert(dataSaveInfo.clockCountTimes==99 && scheduled==1 && timerSoundSelect==1);
    timerSoundNext();
    assert(dataSaveInfo.clockCountTimes==0 && scheduled==2);
    timerSoundConfirm();
    timerSoundNext();
    assert(timerSoundSelect==0 && selected==2);
    timerSoundConfirm();
    expect("上一首","完成","下一首");
    timerSoundPrevious();
    assert(dataSaveInfo.clockCountRing==9 && scheduled==3);
    saveTimerSoundSettingsIfDirty();
    assert(writes==1 && !timerSoundDirty);
    saveTimerSoundSettingsIfDirty();
    assert(writes==1);
    timerSoundNext();
    assert(dataSaveInfo.clockCountRing==0 && scheduled==4);
    leaveTimerSoundSettings();
    assert(writes==2 && returned==1);
    puts("PASS: timer sound settings and long-right action");
}
'''

with tempfile.TemporaryDirectory(prefix="gview-timer-sound-") as folder:
    test = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    test.write_text(harness.replace("__PRODUCTION__", production), encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
