#include "backLight.h"
#include <stdio.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "../rom.h"
#include "../NET/RTC_Internet.h"


#define LEDC_TIMER              LEDC_TIMER_1
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL            LEDC_CHANNEL_5
#define LEDC_DUTY_RES           LEDC_TIMER_10_BIT
#define LEDC_FREQUENCY          (1000)    // Frequency in Hertz. Set frequency at 50 Hz


u8 backLightNum = 4; //0-10;
static int currentScheduleIndex = -1;
static SemaphoreHandle_t backLightLock;

void backLightInit(void){
    backLightLock = xSemaphoreCreateMutex();
    configASSERT(backLightLock);
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    printf("Backlight PWM GPIO: %d\n", LEDC_OUTPUT_IO);
}

//时间排序
/* timeNumTemp:当前时间
** alarmTime:定时时间
*/
int findLastAlarmBeforeCurrent(int timeNumTemp, int alarmTime[5]) {
    int maxAlarm = -1;  // 记录小于当前时间的最大闹钟值
    int maxIndex = -1;
    
    for (int i = 0; i < 5; i++) {
        if (alarmTime[i] <= timeNumTemp && alarmTime[i] > maxAlarm) {
            maxAlarm = alarmTime[i];
            maxIndex = i;
        }
    }
    //如果找不到小于当前时间的值 
    if(maxIndex<0){
        maxIndex = 0;
        //寻找最大的闹钟时间
        for (int i = 0; i < 5; i++) {
            if (alarmTime[i] > alarmTime[maxIndex]) {
                maxIndex = i;
            }
        }
    }
    return maxIndex;  // 返回原始数组中的索引
}

static int scheduledBrightnessIndex(void){
    int alarmTime[5];
    for(u8 i=0;i<5;i++) alarmTime[i]=dataSaveInfo.brightnessInfo[i]>>4;
    const int minute=tStruct->tm_hour*60+tStruct->tm_min;
    return findLastAlarmBeforeCurrent(minute,alarmTime);
}

void resetBackLightVal(void){
    currentScheduleIndex=scheduledBrightnessIndex();
    setBackLightVal(dataSaveInfo.brightnessInfo[currentScheduleIndex]&0x0F);
}

void backLightScheduleTick(void){
    // Keep the user's brightness preview while editing a schedule entry.
    if(pageNow==PAGE_BGLIGHT_SCHEDULE) return;
    const int index=scheduledBrightnessIndex();
    if(index!=currentScheduleIndex){
        currentScheduleIndex=index;
        setBackLightVal(dataSaveInfo.brightnessInfo[index]&0x0F);
    }
}

void setBackLightVal(u8 num){
    xSemaphoreTake(backLightLock, portMAX_DELAY);
    backLightNum = num;
    if(backLightNum>10) backLightNum=10;
    if(!backLightNum) backLightNum=1;
    updateBackLight(backLightNum);
    xSemaphoreGive(backLightLock);
}

void updateBackLight(u8 num){
    num = num * num;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, (u16)(1024/100*num)));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}