"""Exercise sound settings and the remaining temperature setting handlers."""

from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "main/PAGE/SETTING/setting.c").read_bytes().decode("gb18030")
assert "dataSaveInfo.clockCountRing" not in source
assert "dataSaveInfo.clockCountTimes" not in source


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
    "static void updateSoundKeyHints(void){",
    "static u8 stepSoundVolume(u8 current,int direction){",
    "static void adjustSoundSetting(int direction){",
    "static void soundPrevious(void){",
    "static void soundNext(void){",
    "static void soundConfirm(void){",
    "static void updateHomeBgmPreviewHint(void){",
    "static void homeBgmPrevious(void){",
    "static void homeBgmNext(void){",
    "static void homeBgmPreviewToggle(void){",
    "static void homeBgmChoose(void){",
    "static void updateSystemSettings(void){",
    "static void systemPrevious(void){",
    "static void systemNext(void){",
    "static void systemConfirm(void){",
))
assert 'drawPageUITxt("上一项","编辑","下一项")' in source
assert 'drawPageUITxt("","编辑","")' in source

harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
#define SOUND_SET_COUNT 2
#define SOUND_VOLUME_STEP 5
#define HOME_BGM_SILENT 0
#define HOME_BGM_TRACK_COUNT 10
#define ORANGE 17
static struct { u8 volumeNum, clockCountRing, clockCountTimes;
                u8 homeBgmSelection, homeBgmPreferredSelection;
                signed char tempOffset; } dataSaveInfo = {50, 3, 4, 8, 8, 0};
typedef struct { int color; } DisplayObject;
static DisplayObject systemValueStorage;
static DisplayObject *systemValue=&systemValueStorage;
static char tempOffsetText[8];
static u8 soundSetSelect, soundSettingsEditing, homeBgmCursor;
static int homeBgmHintPreview=-2, previewSlot=-1;
static u8 homeBgmHintCursor=255;
static DisplayObject *homeBgmList;
static u8 systemEditing, systemDirty;
static const char *hints[3];
static const char *holdHints[2];
static int moved, updated, soundVolume, queued, saved, entered, returned, written, delays;
static int previewsStarted,previewsStopped,selectionMoved,previewErrors;
static void setPageUIKeyText(int cell,const char *label){ hints[cell]=label; }
static void setPageUIMiddleText(const char *label){ hints[1]=label; }
static void updateSoundValues(void){ ++updated; }
static void moveSoundSetSelect(void){ ++moved; }
static void setSoundVal(u8 volume){ soundVolume=volume; }
static void queueSoundSettingsSave(void){ ++queued; }
static void saveSoundSettingsIfDirty(void){ ++saved; }
static void renderHomeBgmSettingsInit(void){ ++entered; }
static void returnFromHomeBgmSettings(void){ ++returned; }
static void setPageUILongKeyText(const char *left,const char *right){
    holdHints[0]=left; holdHints[1]=right;
}
static int soundPreviewSelection(void){ return previewSlot; }
static bool soundPreviewStart(u8 slot){ previewSlot=slot; ++previewsStarted; return true; }
static void soundPreviewStop(void){ previewSlot=-1; ++previewsStopped; }
static void setMenuListY(DisplayObject *list,u8 cursor){
    (void)list; (void)cursor; ++selectionMoved;
}
static void showTips(const char *message,float seconds){
    (void)message; (void)seconds; ++previewErrors;
}
static void writeData(void){ ++written; }
static void saveSystemSettingsIfDirty(void){}
static void clearDelayFunction(void (*fn)(void)){ (void)fn; }
static void DelayFunction(void (*fn)(void),float seconds){ (void)fn;(void)seconds;++delays; }
__PRODUCTION__
static void expect(const char *left,const char *middle,const char *right){
    assert(strcmp(hints[0],left)==0);
    assert(strcmp(hints[1],middle)==0);
    assert(strcmp(hints[2],right)==0);
}
int main(void){
    updateSoundKeyHints();
    expect("上一项","编辑","下一项");
    soundPrevious();
    assert(soundSetSelect==1 && moved==1 && queued==0);
    expect("上一项","进入","下一项");
    soundConfirm();
    assert(entered==1 && saved==1 && !soundSettingsEditing);
    soundNext();
    assert(soundSetSelect==0 && dataSaveInfo.volumeNum==50);
    soundConfirm();
    expect("减少","完成","增加");
    soundNext();
    assert(soundSetSelect==0 && dataSaveInfo.volumeNum==55 && soundVolume==55 && queued==1);
    soundPrevious();
    assert(dataSaveInfo.volumeNum==50 && queued==2);
    dataSaveInfo.volumeNum=93;
    soundNext();
    assert(dataSaveInfo.volumeNum==95 && soundVolume==95 && queued==3);
    dataSaveInfo.volumeNum=93;
    soundPrevious();
    assert(dataSaveInfo.volumeNum==90 && soundVolume==90 && queued==4);
    dataSaveInfo.volumeNum=100;
    updateSoundKeyHints();
    expect("减少","完成","");
    soundNext();
    assert(dataSaveInfo.volumeNum==100 && queued==4);
    soundPrevious();
    assert(dataSaveInfo.volumeNum==95 && queued==5);
    dataSaveInfo.volumeNum=0;
    updateSoundKeyHints();
    expect("","完成","增加");
    soundPrevious();
    assert(queued==5);
    soundConfirm();
    soundNext();
    expect("上一项","进入","下一项");
    assert(soundSetSelect==1 && queued==5);
    dataSaveInfo.volumeNum=50;
    homeBgmCursor=0;
    updateHomeBgmPreviewHint();
    assert(strcmp(holdHints[0],"返回")==0 && holdHints[1]==NULL);
    homeBgmNext();
    assert(homeBgmCursor==1 && selectionMoved==1 && strcmp(holdHints[1],"试听")==0);
    homeBgmPreviewToggle();
    assert(previewSlot==0 && previewsStarted==1 && strcmp(holdHints[1],"停止试听")==0);
    homeBgmPreviewToggle();
    assert(previewSlot==-1 && strcmp(holdHints[1],"试听")==0);
    homeBgmPreviewToggle();
    homeBgmNext();
    assert(homeBgmCursor==2 && previewSlot==-1 && strcmp(holdHints[1],"试听")==0);
    homeBgmPrevious();
    assert(homeBgmCursor==1 && selectionMoved==3);
    dataSaveInfo.volumeNum=0;
    homeBgmCursor=0; updateHomeBgmPreviewHint();
    assert(holdHints[1]==NULL && previewErrors==0);
    homeBgmCursor=2;
    homeBgmChoose();
    assert(dataSaveInfo.homeBgmSelection==2 && dataSaveInfo.homeBgmPreferredSelection==2);
    assert(written==1 && returned==1);
    homeBgmCursor=0;
    homeBgmChoose();
    assert(dataSaveInfo.homeBgmSelection==0 && dataSaveInfo.homeBgmPreferredSelection==2);
    updateSystemSettings();
    expect("","编辑","");
    systemConfirm();
    expect("减少","完成","增加");
    systemNext();
    assert(dataSaveInfo.tempOffset==1 && systemDirty && delays==1);
    systemPrevious();
    assert(dataSaveInfo.tempOffset==0 && delays==2);
    dataSaveInfo.tempOffset=5;
    updateSystemSettings();
    expect("减少","完成","");
    systemNext();
    assert(dataSaveInfo.tempOffset==5 && delays==2);
    systemConfirm();
    expect("","编辑","");
    puts("PASS: sound browse/edit, BGM preference, and temperature editing");
}
'''

with tempfile.TemporaryDirectory(prefix="gview-sound-settings-") as folder:
    test = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    test.write_text(harness.replace("__PRODUCTION__", production), encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
