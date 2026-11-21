// Shared application state, home navigation, autosave and alarm scheduling.
#include "app_runtime.h"
#include "config.h"
#include "GVIEW/gview.h"
#include "rom.h"
#include "sound.h"
#include "NET/RTC_Internet.h"
#include "PAGE/pageDraw.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

u8 pageNow = 0xFF;
u16 BGColor = defaultBGColor;
u16 sleepTimeNum = 0;

void DelayMs(u16 num){
	vTaskDelay(num / portTICK_PERIOD_MS);
}

u16 backHomeTime = 0;
u8 homeState = 0;//0: home, 1: weather, 3: calendar; retired mode 2 falls back to home.
u8 alarmClockNow = 0;//当前闹钟编号

void backToHomePage(void){

    switch (homeState)
    {
        case 0:
            renderHomeInit();
            break;
        case 1:
            weatherInit();
            break;
        case 3:
            renderCalendarInit();
            break;
        default:
            renderHomeInit();
            break;
    }
}
static u32 systemTimeCount = 0;
static u32 moneyOld = 0;
void appRuntimeRememberSavedBalance(void){moneyOld=dataSaveInfo.money;}
void checkAlarmClock(void){
    //定时保存money
    systemTimeCount ++;
    if(!(systemTimeCount%5))//5秒检查一次
    {
        if(moneyOld!=dataSaveInfo.money){
            moneyOld = dataSaveInfo.money;
            writeData();
        }
    }
    //定时回主页
    if(pageNow > 5 && (pageNow != PAGE_TCLCK && !tomatoPageStartFlag && pageNow != PAGE_CLOCKCOUNT)){

        if(backHomeTime){
            backHomeTime--;
        }else{
            if(pageNow == PAGE_SOUND_SET) saveSoundSettingsIfDirty();
            if(pageNow == PAGE_CLOCKCOUNT_SET) saveTimerSoundSettingsIfDirty();
            if(pageNow == PAGE_SYSTEM_SET) saveSystemSettingsIfDirty();
            if(pageNow == PAGE_BGLIGHT_SCHEDULE || pageNow == PAGE_CLOCK_SET)
                leaveClockSettings();
            backToHomePage();
        }
    }


    if(!tStruct->tm_sec){
        u16 timeNumTemp = tStruct->tm_hour*60+tStruct->tm_min;
        //闹钟判断
         //获取星期几
        u8 weekdayTemp = weekday;
        if(!weekdayTemp)weekdayTemp = 6;
        else weekdayTemp = weekdayTemp-1;
        printf("week Now check : %d \n",weekdayTemp);
        for(u8 i=0;i<5;i++){
            if((dataSaveInfo.alarmClockWeek[i] >> weekdayTemp)&0x01){
                u16 alarmTime = dataSaveInfo.alarmClock[i]>>4;
                if(timeNumTemp == alarmTime){
                    alarmClockNow = i;
                    aClockTimes = dataSaveInfo.clockCountTimes?dataSaveInfo.clockCountTimes+1:101;//响铃5次
                    showTips("闹钟时间到!",0);
                    break;
                }
            }
        }
    }
}
