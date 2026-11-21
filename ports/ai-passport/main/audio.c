#include "sound.h"
#include "audio_policy.h"
#include "rom.h"
#include "sound_catalog.h"
#include "mp3_player.h"
#include "bsp_audio.h"
#include "passport_audio.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdatomic.h>

volatile u16 aClockTimes;
static atomic_int preview=-1;
static atomic_uint volume=50,stop_generation;
static atomic_bool home_allowed=true;
static bool ready;
static atomic_bool sleep_requested;
static SemaphoreHandle_t sleep_done;
static atomic_uint decode_us,decode_peak_us,decoded_chunks;
static atomic_int playing_slot=-1;
static TaskHandle_t audio_task;
PassportAudioStats passport_audio_take_stats(void){
    return (PassportAudioStats){
        .decode_us=atomic_exchange(&decode_us,0),.peak_us=atomic_exchange(&decode_peak_us,0),
        .chunks=atomic_exchange(&decoded_chunks,0),.slot=atomic_load(&playing_slot),
        .volume=atomic_load(&volume),
        .stack_free=audio_task?(uint32_t)uxTaskGetStackHighWaterMark(audio_task):0
    };
}

void setSoundVal(u8 value){atomic_store(&volume,value>100?100:value);}
void soundInit(void){
    sleep_done=xSemaphoreCreateBinary();configASSERT(sleep_done);
    ready=bsp_audio_init_playback()==ESP_OK && bsp_audio_set_format(12000,16,1)==ESP_OK;
    soundCatalogInit();setSoundVal(dataSaveInfo.volumeNum);
    if(!ready)ESP_LOGW("passport_audio","Audio unavailable; UI remains usable");
}
void soundStopAll(void){atomic_store(&preview,-1);atomic_fetch_add(&stop_generation,1);}
void homeBgmSuspend(void){atomic_store(&home_allowed,false);}
void homeBgmResume(void){atomic_store(&home_allowed,true);}
bool soundPreviewStart(u8 slot){
    if(!soundPreviewCanStart(slot,ready))return false;
    atomic_store(&preview,slot);return true;
}
void soundPreviewStop(void){atomic_store(&preview,-1);}
int soundPreviewSelection(void){return atomic_load(&preview);}
static void close_track(void){mp3_player_stop();soundCatalogClose();atomic_store(&playing_slot,-1);}
void passport_audio_prepare_sleep(void){
    atomic_store(&sleep_requested,true);
    if(xSemaphoreTake(sleep_done,pdMS_TO_TICKS(1500))!=pdTRUE)esp_restart();
}
void soundTask(void *argument){
    audio_task=xTaskGetCurrentTaskHandle();
    (void)argument;int mode=0,slot=-1;unsigned generation=atomic_load(&stop_generation),old_volume=101;
    int16_t samples[480];
    while(1){
        if(atomic_load(&sleep_requested)){
            close_track();bsp_audio_suspend();audio_task=NULL;xSemaphoreGive(sleep_done);vTaskDelete(NULL);
        }
        unsigned gain=atomic_load(&volume);
        if(ready && gain!=old_volume){bsp_audio_set_volume(gain);old_volume=gain;}
        unsigned next_generation=atomic_load(&stop_generation);
        if(generation!=next_generation){close_track();mode=0;slot=-1;generation=next_generation;}
        const SoundPlayback request=soundPlaybackSelect(ready,gain,
            atomic_load(&home_allowed),atomic_load(&preview));
        const int wanted=request.slot,wanted_mode=request.mode;
        if(wanted<0 || wanted>=SOUND_TRACK_COUNT){
            if(mode)close_track();mode=0;slot=-1;vTaskDelay(pdMS_TO_TICKS(20));continue;
        }
        if(wanted!=slot || wanted_mode!=mode){
            close_track();const uint8_t *data=NULL;size_t size=0;
            if(!soundCatalogOpen(wanted,&data,&size) ||
               !(wanted_mode==1?mp3_player_start(data,size):mp3_player_start_once(data,size))){
                close_track();if(wanted_mode==3)aClockTimes=0;
                if(wanted_mode==2)atomic_store(&preview,-1);
                mode=0;slot=-1;vTaskDelay(pdMS_TO_TICKS(100));continue;
            }
            slot=wanted;mode=wanted_mode;
            atomic_store(&playing_slot,slot);
            if(mode==3)aClockTimes=soundAlarmStarted(aClockTimes);
        }
        if(!mp3_player_is_active()){
            if(mode==2){int expected=slot;atomic_compare_exchange_strong(&preview,&expected,-1);}
            if(mode==3)aClockTimes=soundAlarmFinished(aClockTimes);
            close_track();slot=-1;mode=0;continue;
        }
        uint64_t decode_start=esp_timer_get_time();
        for(unsigned i=0;i<480;i++)samples[i]=mp3_player_render(100);
        unsigned duration=(unsigned)(esp_timer_get_time()-decode_start);
        atomic_fetch_add(&decode_us,duration);atomic_fetch_add(&decoded_chunks,1);
        unsigned peak=atomic_load(&decode_peak_us);
        while(duration>peak && !atomic_compare_exchange_weak(&decode_peak_us,&peak,duration)){}
        if(bsp_audio_write(samples,sizeof(samples))!=ESP_OK){
            close_track();slot=-1;mode=0;vTaskDelay(pdMS_TO_TICKS(40));
        }
    }
}
