#include "sound.h"
#include "audio_policy.h"

#include <stdio.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "adc.h"
#include "rom.h"
#include "mp3_player.h"
#include "sound_catalog.h"
#include "waveshare_audio.h"
#include "PAGE/pageDraw.h"

volatile u16 aClockTimes;
static SemaphoreHandle_t playbackLock;
static atomic_bool homeBgmAllowed;
static atomic_int previewRequest = ATOMIC_VAR_INIT(-1);
static bool bgmPlaying;
static bool alarmPlaying;
static bool previewPlaying;
static u8 bgmSelection;
static u8 previewSelection;

static void stopPlaybackLocked(void)
{
    if (!bgmPlaying && !alarmPlaying && !previewPlaying) return;
    // The codec lock inside this call waits for the current decode block to end.
    waveshare_audio_set_mp3_active(false, false);
    mp3_player_stop();
    soundCatalogClose();
    waveshare_audio_set_active(false);
    bgmPlaying = false;
    alarmPlaying = false;
    previewPlaying = false;
}

static bool startTrackLocked(u8 slot, bool loop)
{
    const uint8_t *file = NULL;
    size_t length = 0;
    if (!soundCatalogOpen(slot, &file, &length)) return false;
    bool started = loop ? mp3_player_start(file, length) :
                          mp3_player_start_once(file, length);
    if (!started) {
        soundCatalogClose();
        return false;
    }
    waveshare_audio_set_active(true);
    waveshare_audio_set_mp3_active(true, loop);
    return true;
}

void setSoundVal(u8 volume)
{
    if (volume > 100) volume = 100;
    waveshare_audio_set_volume(volume);
}

void soundInit(void)
{
    waveshare_audio_init(waveshare_i2c_bus());
    playbackLock = xSemaphoreCreateMutex();
    configASSERT(playbackLock);
    setSoundVal(dataSaveInfo.volumeNum);
    printf("MP3 catalog %s\n", soundCatalogInit() ? "ready" : "missing");
}

void soundStopAll(void)
{
    atomic_store(&previewRequest, -1);
    if (!playbackLock) return;
    xSemaphoreTake(playbackLock, portMAX_DELAY);
    stopPlaybackLocked();
    xSemaphoreGive(playbackLock);
}

void homeBgmSuspend(void)
{
    // Page changes run on the render task. Let the sound task stop the codec
    // instead of waiting here for a decoder or I2S write to release its lock.
    atomic_store(&homeBgmAllowed, false);
    waveshare_audio_set_home_bgm_allowed(false);
}

void homeBgmResume(void)
{
    atomic_store(&homeBgmAllowed, true);
    waveshare_audio_set_home_bgm_allowed(true);
}

bool soundPreviewStart(u8 slot)
{
    if (!soundPreviewCanStart(slot, true)) return false;
    atomic_store(&previewRequest, slot);
    return true;
}

void soundPreviewStop(void)
{
    atomic_store(&previewRequest, -1);
}

int soundPreviewSelection(void)
{
    return atomic_load(&previewRequest);
}

static bool alarmTrack(u8 *slot)
{
    const int selected = soundAlarmSlot();
    if (selected < 0) return false;
    *slot = (u8)selected;
    return true;
}

static void serviceAlarmLocked(void)
{
    atomic_store(&previewRequest, -1);
    if (bgmPlaying || previewPlaying) stopPlaybackLocked();
    if (alarmPlaying) {
        if (mp3_player_is_active()) return;
        stopPlaybackLocked();
        aClockTimes = soundAlarmFinished(aClockTimes);
        if (!aClockTimes) {
            hideTips();
            return;
        }
    }
    u8 slot;
    if (!alarmTrack(&slot) || !startTrackLocked(slot, false)) {
        aClockTimes = 0;
        hideTips();
        return;
    }
    alarmPlaying = true;
    aClockTimes = soundAlarmStarted(aClockTimes);
}

static void servicePreviewLocked(void)
{
    const int requested=atomic_load(&previewRequest);
    const SoundPlayback wanted = soundPlaybackSelect(true, dataSaveInfo.volumeNum,
                                                     atomic_load(&homeBgmAllowed), requested);
    if (pageNow != PAGE_BGM_SET || dataSaveInfo.volumeNum == 0 ||
        !soundCatalogReady()) {
        if (requested >= 0) atomic_store(&previewRequest, -1);
        if (previewPlaying) stopPlaybackLocked();
        return;
    }
    if (wanted.mode != SOUND_MODE_PREVIEW) {
        if (previewPlaying) stopPlaybackLocked();
        return;
    }
    if (previewPlaying && previewSelection == requested) {
        if (mp3_player_is_active()) return;
        stopPlaybackLocked();
        int expected=requested;
        atomic_compare_exchange_strong(&previewRequest, &expected, -1);
        return;
    }
    if (bgmPlaying || previewPlaying) stopPlaybackLocked();
    if (startTrackLocked(requested, false)) {
        previewPlaying = true;
        previewSelection = requested;
    } else {
        int expected=requested;
        atomic_compare_exchange_strong(&previewRequest, &expected, -1);
    }
}

static void serviceHomeBgmLocked(void)
{
    const SoundPlayback wanted = soundPlaybackSelect(true, dataSaveInfo.volumeNum,
        atomic_load(&homeBgmAllowed), atomic_load(&previewRequest));
    if (wanted.mode != SOUND_MODE_HOME) {
        if (bgmPlaying) stopPlaybackLocked();
        return;
    }
    const u8 selected = (u8)(wanted.slot + 1);
    if (bgmPlaying && bgmSelection == selected && mp3_player_is_active())
        return;
    if (bgmPlaying || previewPlaying) stopPlaybackLocked();
    if (startTrackLocked(selected - 1, true)) {
        bgmPlaying = true;
        bgmSelection = selected;
    }
}

void soundTask(void *arg)
{
    (void)arg;
    while (1) {
        xSemaphoreTake(playbackLock, portMAX_DELAY);
        if (aClockTimes) serviceAlarmLocked();
        else {
            if (alarmPlaying) stopPlaybackLocked();
            servicePreviewLocked();
            serviceHomeBgmLocked();
        }
        xSemaphoreGive(playbackLock);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
