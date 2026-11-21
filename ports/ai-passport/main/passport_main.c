#include "config.h"
#include "rom.h"
#include "adc.h"
#include "sound.h"
#include "imu_rotation.h"
#include "LCD/backLight.h"
#include "LCD/lcd_init.h"
#include "NET/connect.h"
#include "NET/RTC_Internet.h"
#include "PAGE/pageDraw.h"
#include "passport_display.h"
#include "passport_input.h"
#include "passport_audio.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/usb_serial_jtag.h"
#include "power_status.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdatomic.h>
#include "PAGE/HOME/home.h"

u16 KeyBtn_Val,VoltageVal;
u8 roundStateGet,longRDown,longLDown;
uint16_t KEY0Temp,KEY1Temp,KEY2Temp,KEY3Temp,KEY4Temp;
int passportBatteryPercent=-1;
#include "app_runtime.h"
extern void doKeyFun(KeyName);
static QueueHandle_t events;
static bool battery_ready;
static atomic_bool time_sync_ready;
void passport_time_sync_ready(void){atomic_store(&time_sync_ready,true);}
typedef struct { uint8_t button,down;uint64_t milliseconds; } ButtonEvent;
// Official portrait order is UP / DOWN / OK, also left / middle / right in
// the default landscape direction (confirmed on this device).
_Static_assert(BSP_BTN_UP==0 && BSP_BTN_DOWN==1 && BSP_BTN_OK==2,
               "Recheck physical button order after changing the BSP");

// BSP callbacks are bounded producers. All UI and storage run on the app task.
static void button_event(bsp_btn_t button,bsp_btn_ev_t event,void *user){
    (void)user;
    if(event!=BSP_BTN_PRESS && event!=BSP_BTN_RELEASE)return;
    if((unsigned)button>=3)return;
    ButtonEvent message={(uint8_t)button,event==BSP_BTN_PRESS,(uint64_t)esp_timer_get_time()/1000};
    if(xQueueSend(events,&message,0)!=pdTRUE){
        // Drop queued presses as a unit; polling below clears any stale holds.
        xQueueReset(events);
        message.button=255;xQueueSend(events,&message,0);
    }
}
// Passport has no IMU: this saved switch selects the two landscape directions.
void imuRotationSetEnabled(bool enabled){LCD_SetRotation(enabled?0:2);}
i2c_master_bus_handle_t waveshare_i2c_bus(void){return bsp_i2c_bus();}
void requestPowerOff(void){
    saveSystemSettingsIfDirty();saveSoundSettingsIfDirty();saveTimerSoundSettingsIfDirty();writeData();
    soundStopAll();passport_audio_prepare_sleep();bsp_display_backlight(0);
    esp_lcd_panel_disp_on_off(bsp_display_panel(),false);
    int level=0;
    if(bsp_button_prepare_deep_sleep(&level)!=ESP_OK || !level){esp_restart();return;}
    ESP_ERROR_CHECK(esp_deep_sleep_enable_gpio_wakeup(UINT64_C(1)<<BSP_BTN_GPIO,ESP_GPIO_WAKEUP_GPIO_LOW));
    esp_deep_sleep_start();
}
// SOF packets identify an active computer USB connection, not a wall charger.
PowerStatus powerStatusGet(void){
    return usb_serial_jtag_is_connected()?POWER_STATUS_USB_POWERED:POWER_STATUS_NONE;
}
static void read_battery(void){
    passportBatteryPercent=battery_ready?bsp_battery_soc():-1;
    int mv=battery_ready?bsp_battery_mv():-1;
    VoltageVal=mv>0?(uint16_t)mv:0;
}
void app_main(void){
    ESP_LOGI("travel_passport","Travel Clock: AI Passport, native landscape 320x240, no PSRAM");
    update_rtc_Txt();romInit();LCD_Init();backLightInit();
    passport_display_start();
    // The original 1200-object allocation and decoded forest caches require PSRAM.
    viewInit(320,240,10,PASSPORT_OBJECT_POOL,50,40,25);
    configASSERT(gviewIsReady());
    imuRotationSetEnabled(dataSaveInfo.autoRotationEnabled!=0);
    snprintf(tempTxt,sizeof(char)*10,"--");
    battery_ready=bsp_battery_init()==ESP_OK;read_battery();
    soundInit();configASSERT(xTaskCreate(soundTask,"travel_audio",PASSPORT_AUDIO_STACK_BYTES,NULL,6,NULL)==pdPASS);
    events=xQueueCreate(24,sizeof(ButtonEvent));configASSERT(events);
    ESP_ERROR_CHECK(bsp_button_init(button_event,NULL));
    wifi_Init();rtcInit();backToHomePage();resetBackLightVal();
    PassportOrientedInput input={0};TickType_t next=xTaskGetTickCount();
    uint64_t last_second=esp_timer_get_time()/1000000;
    uint64_t last_battery=last_second;bool was_alarm=false;
    uint64_t profile_start=esp_timer_get_time(),render_us=0,max_render_us=0;
    uint32_t profile_frames=0;
    const TickType_t frame_period=pdMS_TO_TICKS(40);
    while(1){
        ButtonEvent message;
        while(xQueueReceive(events,&message,0)==pdTRUE){
            if(message.button==255){memset(&input,0,sizeof(input));continue;}
            passport_dispatch(passport_oriented_event(&input,message.button,message.down,
                                                      LCD_GetRotation(),message.milliseconds));
        }
        passport_dispatch(passport_oriented_poll(&input,LCD_GetRotation(),esp_timer_get_time()/1000));
        if(atomic_exchange(&time_sync_ready,false)){
            update_rtc_Txt();refrushDayTime();checkDayTime();
            if(pageNow==PAGE_HOME)changeSceneColor();
        }
        uint64_t now=esp_timer_get_time()/1000000;
        if(now!=last_second){
            last_second=now;++localeTime;update_rtc_Txt();backLightScheduleTick();checkAlarmClock();
            if(now-last_battery>=10){last_battery=now;read_battery();}
            if(now%60==0)ESP_LOGI("travel_passport","heap=%lu largest=%lu stack=%u",
                (unsigned long)esp_get_free_heap_size(),(unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                (unsigned)uxTaskGetStackHighWaterMark(NULL));
        }
        if(was_alarm && !aClockTimes)hideTips();was_alarm=aClockTimes!=0;
        wifiProcessSavedConnection();keyState=0;
        uint64_t render_start=esp_timer_get_time();
        update();++FrameRateTemp;++profile_frames;
        uint64_t duration=esp_timer_get_time()-render_start;
        render_us+=duration;if(duration>max_render_us)max_render_us=duration;
        uint64_t profile_end=esp_timer_get_time(),elapsed=profile_end-profile_start;
        if(elapsed>=5000000){
            unsigned long rate=(uint64_t)profile_frames*10000000/elapsed;
            GVIEWRenderStats stats=gviewGetRenderStats();
            PassportAudioStats audio=passport_audio_take_stats();
            ESP_LOGI("travel_perf","fps=%lu.%lu frame_us=%lu max_us=%lu spi_wait_us=%lu objects=%u lookups=%lu draws=%lu heap=%lu largest=%lu music=%d volume=%u audio_us=%lu audio_peak_us=%lu audio_stack=%lu rotation=%u",
                rate/10,rate%10,(unsigned long)(render_us/profile_frames),(unsigned long)max_render_us,
                (unsigned long)(passport_display_take_wait_us()/profile_frames),(unsigned)stats.commands,
                (unsigned long)stats.sceneLookups,(unsigned long)stats.drawCalls,
                (unsigned long)esp_get_free_heap_size(),
                (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                audio.slot+1,audio.volume,(unsigned long)audio.decode_us,
                (unsigned long)audio.peak_us,(unsigned long)audio.stack_free,(unsigned)LCD_GetRotation());
            profile_start=profile_end;profile_frames=0;render_us=0;max_render_us=0;
        }
        // An overrun drops the missed deadline instead of starving the idle task
        // while trying to catch up several obsolete frames.
        if((TickType_t)(xTaskGetTickCount()-next)>=frame_period){
            next=xTaskGetTickCount();vTaskDelay(1);
        }else vTaskDelayUntil(&next,frame_period);
    }
}
