"""Run production MP3 switching and alarm selection against mocked audio."""

from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "main/sound.c").read_text()


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
    "static void stopPlaybackLocked(void)",
    "static bool startTrackLocked(u8 slot, bool loop)",
    "void soundStopAll(void)",
    "void homeBgmSuspend(void)",
    "void homeBgmResume(void)",
    "bool soundPreviewStart(u8 slot)",
    "void soundPreviewStop(void)",
    "int soundPreviewSelection(void)",
    "static bool alarmTrack(u8 *slot)",
    "static void serviceAlarmLocked(void)",
    "static void servicePreviewLocked(void)",
    "static void serviceHomeBgmLocked(void)",
))

harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t u8;
typedef uint16_t u16;
#define SOUND_TRACK_COUNT 10
#define HOME_BGM_SILENT 0
#define PAGE_HOME 1
#define PAGE_MENU 2
#define PAGE_BGM_SET 3
#define portMAX_DELAY 0
static int playbackLock=1;
static atomic_bool homeBgmAllowed;
static atomic_int previewRequest=ATOMIC_VAR_INIT(-1);
static bool bgmPlaying,alarmPlaying,previewPlaying,mp3Active,codecOpen,mp3Output;
static bool homeBgmOutputAllowed,mp3IsHomeBgm;
static u8 bgmSelection,previewSelection,alarmClockNow,lastSlot;
static volatile u16 aClockTimes;
static int pageNow=PAGE_HOME, loopStarts, onceStarts, closes, hides;
static struct { u8 homeBgmSelection,tomatoRestRing,tomatoWorkRing,clockCountRing,volumeNum;
                u16 alarmClock[5]; } dataSaveInfo;
static const uint8_t fileBytes[4]={0xff,0xe3,0,0};
static void xSemaphoreTake(int lock,int wait){ (void)lock;(void)wait; }
static void xSemaphoreGive(int lock){ (void)lock; }
static bool soundCatalogReady(void){ return true; }
static bool soundCatalogOpen(u8 slot,const uint8_t **file,size_t *length){
    lastSlot=slot; *file=fileBytes; *length=sizeof(fileBytes); return slot<10;
}
static void soundCatalogClose(void){ ++closes; }
static void waveshare_audio_set_active(bool active){
    if(!active) assert(!mp3Output);
    codecOpen=active;
}
static void waveshare_audio_set_mp3_active(bool active,bool home_bgm){
    if(active) assert(codecOpen && mp3Active);
    mp3Output=active;
    mp3IsHomeBgm=active && home_bgm;
}
static void waveshare_audio_set_home_bgm_allowed(bool allowed){
    homeBgmOutputAllowed=allowed;
}
static void mp3_player_stop(void){ assert(!mp3Output); mp3Active=false; }
static bool mp3_player_is_active(void){ return mp3Active; }
static bool mp3_player_start(const uint8_t *file,size_t length){
    assert(file==fileBytes && length==4); ++loopStarts; mp3Active=true; return true;
}
static bool mp3_player_start_once(const uint8_t *file,size_t length){
    assert(file==fileBytes && length==4); ++onceStarts; mp3Active=true; return true;
}
static void hideTips(void){ ++hides; }
__PRODUCTION__
int main(void){
    dataSaveInfo.volumeNum=90;
    homeBgmResume();
    serviceHomeBgmLocked();
    assert(!loopStarts && !codecOpen);
    dataSaveInfo.homeBgmSelection=8;
    serviceHomeBgmLocked();
    assert(loopStarts==1 && lastSlot==7 && bgmPlaying && codecOpen && mp3Output);
    assert(mp3IsHomeBgm && homeBgmOutputAllowed);
    serviceHomeBgmLocked();
    assert(loopStarts==1);
    dataSaveInfo.homeBgmSelection=2;
    serviceHomeBgmLocked();
    assert(loopStarts==2 && lastSlot==1 && closes==1);
    aClockTimes=2;
    alarmClockNow=0;
    dataSaveInfo.alarmClock[0]=3;
    serviceAlarmLocked();
    assert(!bgmPlaying && alarmPlaying && lastSlot==2 && onceStarts==1);
    assert(!mp3IsHomeBgm);
    assert(aClockTimes==1 && closes==2);
    mp3Active=false; // Decoder reached EOF.
    serviceAlarmLocked();
    assert(!alarmPlaying && !aClockTimes && hides==1 && closes==3);
    serviceHomeBgmLocked();
    assert(loopStarts==3 && lastSlot==1);
    homeBgmSuspend();
    assert(bgmPlaying && codecOpen && !homeBgmOutputAllowed);
    // Audio output is gated immediately; cleanup still runs on the sound task.
    serviceHomeBgmLocked();
    assert(!bgmPlaying && !codecOpen && closes==4);
    pageNow=PAGE_MENU;
    homeBgmResume();
    serviceHomeBgmLocked();
    assert(loopStarts==3);
    u8 slot=255;
    alarmClockNow=201; dataSaveInfo.tomatoRestRing=5;
    assert(alarmTrack(&slot) && slot==5);
    alarmClockNow=202; dataSaveInfo.tomatoWorkRing=6;
    assert(alarmTrack(&slot) && slot==6);
    alarmClockNow=101; dataSaveInfo.clockCountRing=9;
    assert(alarmTrack(&slot) && slot==9);
    alarmClockNow=0; dataSaveInfo.alarmClock[0]=0;
    assert(!alarmTrack(&slot));
    pageNow=PAGE_HOME;
    homeBgmResume();
    dataSaveInfo.volumeNum=0;
    serviceHomeBgmLocked();
    assert(loopStarts==3 && !codecOpen);
    dataSaveInfo.volumeNum=50;
    serviceHomeBgmLocked();
    assert(loopStarts==4 && bgmPlaying && codecOpen);
    dataSaveInfo.volumeNum=0;
    serviceHomeBgmLocked();
    assert(!bgmPlaying && !codecOpen && closes==5);
    assert(!soundPreviewStart(0) && !soundPreviewStart(10));
    dataSaveInfo.volumeNum=50;
    pageNow=PAGE_BGM_SET;
    assert(soundPreviewStart(7) && soundPreviewSelection()==7);
    servicePreviewLocked();
    assert(previewPlaying && onceStarts==2 && lastSlot==7 && codecOpen);
    assert(!mp3IsHomeBgm);
    servicePreviewLocked(); assert(onceStarts==2);
    soundPreviewStop(); servicePreviewLocked();
    assert(!previewPlaying && !codecOpen && closes==6);
    assert(soundPreviewStart(3)); servicePreviewLocked();
    assert(previewPlaying && onceStarts==3 && lastSlot==3);
    assert(soundPreviewStart(4)); servicePreviewLocked();
    assert(previewPlaying && onceStarts==4 && lastSlot==4 && closes==7);
    mp3Active=false; servicePreviewLocked();
    assert(!previewPlaying && soundPreviewSelection()==-1 && closes==8);
    assert(soundPreviewStart(5)); servicePreviewLocked();
    aClockTimes=2; alarmClockNow=0; dataSaveInfo.alarmClock[0]=1;
    serviceAlarmLocked();
    assert(!previewPlaying && soundPreviewSelection()==-1);
    assert(alarmPlaying && lastSlot==0 && onceStarts==6 && closes==9);
    soundStopAll(); aClockTimes=0;
    assert(soundPreviewStart(2)); servicePreviewLocked();
    assert(previewPlaying && lastSlot==2);
    pageNow=PAGE_MENU; servicePreviewLocked();
    assert(!previewPlaying && soundPreviewSelection()==-1 && !codecOpen);
    puts("PASS: MP3 BGM, audition toggle/end/page exit, alarm priority, and ringtone slots");
}
'''

with tempfile.TemporaryDirectory(prefix="gview-mp3-policy-") as directory:
    test = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    test.write_text(harness.replace("__PRODUCTION__", production), encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
