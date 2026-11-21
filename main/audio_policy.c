#include "audio_policy.h"
#include "rom.h"
#include "sound.h"
#include "sound_catalog.h"

int soundAlarmSlot(void){
    int slot = -1;
    if (alarmClockNow > 200)
        slot = alarmClockNow == 201 ? dataSaveInfo.tomatoRestRing : dataSaveInfo.tomatoWorkRing;
    else if (alarmClockNow > 100)
        slot = dataSaveInfo.clockCountRing;
    else if (alarmClockNow < 5)
        slot = (dataSaveInfo.alarmClock[alarmClockNow] & 15) - 1;
    return slot >= 0 && slot < SOUND_TRACK_COUNT ? slot : -1;
}

bool soundPreviewCanStart(uint8_t slot, bool available){
    return available && slot < SOUND_TRACK_COUNT && dataSaveInfo.volumeNum && soundCatalogReady();
}

SoundPlayback soundPlaybackSelect(bool available, unsigned volume,
                                 bool home_allowed, int preview_slot){
    SoundPlayback wanted = {-1, SOUND_MODE_NONE};
    if (!available || !volume || !soundCatalogReady()) return wanted;
    if (aClockTimes) {
        wanted.slot = soundAlarmSlot();
        wanted.mode = SOUND_MODE_ALARM;
    } else if (pageNow == PAGE_BGM_SET && preview_slot >= 0) {
        wanted.slot = preview_slot;
        wanted.mode = SOUND_MODE_PREVIEW;
    } else if (pageNow == PAGE_HOME && home_allowed && dataSaveInfo.homeBgmSelection) {
        wanted.slot = dataSaveInfo.homeBgmSelection - 1;
        wanted.mode = SOUND_MODE_HOME;
    }
    if (wanted.slot < 0 || wanted.slot >= SOUND_TRACK_COUNT)
        wanted = (SoundPlayback){-1, SOUND_MODE_NONE};
    return wanted;
}
