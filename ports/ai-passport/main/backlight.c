#include "rom.h"
#include "LCD/backLight.h"
#include "NET/RTC_Internet.h"
#include "bsp_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
u8 backLightNum;
static int currentScheduleIndex=-1;
static SemaphoreHandle_t backLightLock;
void backLightInit(void){backLightLock=xSemaphoreCreateMutex();configASSERT(backLightLock);}
void updateBackLight(u8 num){if(num>10)num=10;bsp_display_backlight(num*num);}
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
