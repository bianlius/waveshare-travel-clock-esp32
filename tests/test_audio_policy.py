"""Verify the playback policy shared by both hardware audio tasks."""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
production="\n".join(line for line in (ROOT/"main/audio_policy.c").read_text().splitlines()
                     if not line.startswith("#include"))
harness=r'''
#include <assert.h>
#include <stdio.h>
#include "audio_policy.h"
#define SOUND_TRACK_COUNT 10
enum { PAGE_HOME, PAGE_BGM_SET, PAGE_MENU };
static int pageNow;
static uint8_t alarmClockNow;
static uint16_t aClockTimes;
static bool catalog=true;
static struct {
    uint8_t volumeNum,homeBgmSelection,tomatoRestRing,tomatoWorkRing,clockCountRing;
    uint16_t alarmClock[5];
} dataSaveInfo;
static bool soundCatalogReady(void){return catalog;}
__PRODUCTION__
static void expect(bool ready,unsigned volume,bool home,int preview,int slot,SoundPlaybackMode mode){
    SoundPlayback request=soundPlaybackSelect(ready,volume,home,preview);
    assert(request.slot==slot && request.mode==mode);
}
int main(void){
    dataSaveInfo.volumeNum=50;dataSaveInfo.homeBgmSelection=8;
    expect(true,50,true,3,7,SOUND_MODE_HOME);
    expect(true,50,false,3,-1,SOUND_MODE_NONE);
    expect(false,50,true,3,-1,SOUND_MODE_NONE);
    expect(true,0,true,3,-1,SOUND_MODE_NONE);
    catalog=false;expect(true,50,true,3,-1,SOUND_MODE_NONE);catalog=true;
    for(int selection=0;selection<=11;++selection){
        dataSaveInfo.homeBgmSelection=selection;
        bool valid=selection>=1 && selection<=10;
        expect(true,50,true,-1,valid?selection-1:-1,valid?SOUND_MODE_HOME:SOUND_MODE_NONE);
    }
    pageNow=PAGE_BGM_SET;
    for(int slot=-1;slot<=10;++slot){
        bool valid=slot>=0 && slot<10;
        expect(true,50,false,slot,valid?slot:-1,valid?SOUND_MODE_PREVIEW:SOUND_MODE_NONE);
    }
    pageNow=PAGE_MENU;expect(true,50,true,3,-1,SOUND_MODE_NONE);
    pageNow=PAGE_BGM_SET;aClockTimes=2;
    for(int alarm=0;alarm<5;++alarm){
        alarmClockNow=alarm;
        for(int selection=0;selection<=11;++selection){
            dataSaveInfo.alarmClock[alarm]=(8*60<<4)|selection;
            bool valid=selection>=1 && selection<=10;
            assert(soundAlarmSlot()==(valid?selection-1:-1));
            expect(true,50,true,3,valid?selection-1:-1,valid?SOUND_MODE_ALARM:SOUND_MODE_NONE);
        }
    }
    alarmClockNow=101;dataSaveInfo.clockCountRing=9;assert(soundAlarmSlot()==9);
    alarmClockNow=201;dataSaveInfo.tomatoRestRing=5;assert(soundAlarmSlot()==5);
    alarmClockNow=202;dataSaveInfo.tomatoWorkRing=6;assert(soundAlarmSlot()==6);
    expect(true,50,true,3,6,SOUND_MODE_ALARM);
    dataSaveInfo.tomatoWorkRing=255;assert(soundAlarmSlot()==-1);
    alarmClockNow=100;assert(soundAlarmSlot()==-1);
    alarmClockNow=200;assert(soundAlarmSlot()==9);
    aClockTimes=0;
    for(int slot=0;slot<=10;++slot)assert(soundPreviewCanStart(slot,true)==(slot<10));
    assert(!soundPreviewCanStart(0,false));
    dataSaveInfo.volumeNum=0;assert(!soundPreviewCanStart(0,true));
    dataSaveInfo.volumeNum=50;catalog=false;assert(!soundPreviewCanStart(0,true));
    for(unsigned rings=1;rings<100;++rings){
        uint16_t count=rings+1;unsigned played=0;
        while(count){count=soundAlarmStarted(count);++played;count=soundAlarmFinished(count);assert(played<=rings);}
        assert(played==rings);
    }
    assert(soundAlarmStarted(0)==0 && soundAlarmFinished(0)==0);
    assert(soundAlarmStarted(101)==101 && soundAlarmFinished(101)==101);
    puts("PASS: alarm/preview/home priority, ringtone bounds, silence, unavailable output and exact repeats");
}
'''
with tempfile.TemporaryDirectory(prefix="travel-audio-policy-") as temp:
    folder=Path(temp);source=folder/"test.c";binary=folder/"test"
    source.write_text(harness.replace("__PRODUCTION__",production))
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
                    "-fsanitize=address,undefined","-I"+str(ROOT/"main"),
                    str(source),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
