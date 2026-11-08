#ifdef GVIEW_UI_AUDIT
void uiAuditBeforeFrame(void);
void uiAuditAfterFrame(void);
#endif


#include "esp_sleep.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "driver/spi_master.h"

#include "LCD/backLight.h"
#include "LCD/lcd_init.h"
#include "GVIEW/gview.h"

#include "NET/connect.h"
#include "NET/RTC_Internet.h"

#include "config.h"
#include "rom.h"
#include "adc.h"
#include "sound.h"
#include "freertos/queue.h"
#include "esp_timer.h"           // 主要头文件
#include "soc/rtc.h"            // RTC相关
#include "driver/rtc_cntl.h"    // RTC控制
#include "PAGE/pageDraw.h"    // RTC控制


u8 pageNow = 0xFF;
u16 BGColor = defaultBGColor;
u16 sleepTimeNum = 0;

u8 randHd;
// spi_device_handle_t spi;
u8 FrameRateHd;//帧率限制

void DelayMs(u16 num){
	vTaskDelay(num / portTICK_PERIOD_MS);
}

void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    //printf("spi complete\n");
}


void gpioInit(void){
    gpio_reset_pin(20); 
    gpio_pullup_en(20);
    gpio_set_direction(20, GPIO_MODE_INPUT);
    gpio_reset_pin(21); 
    gpio_pullup_en(21);
    gpio_set_direction(21, GPIO_MODE_INPUT);
}

//1S / 次
u16 backHomeTime = 0;
u8 homeState = 0;//0 - 精灵主页, 1 - 3天天气主页, 2 - 纯数字时钟
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
        case 2:
            clockPageInit();
            break;
        case 3:
            renderCalendarInit();
            break;
        default:
            renderHomeInit();
            break;
    }
}
u32 systemTimeCount = 0;
u32 moneyOld = 0;
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

// u32 localeTime = 0;
void OneSecFunction(void *arg){
    while(1){
        localeTime ++;
        //天气及时钟逻辑
        update_rtc_Txt();  
        backLightScheduleTick();
        //闹钟逻辑
        checkAlarmClock();
        // printf("Sec:%d,F,%d\n",tStruct->tm_sec,(int)FrameRateTemp);//打印帧率
        FrameRateTemp = 0;    
        vTaskDelay(1000 / portTICK_PERIOD_MS); 

        //----------打印相关-----------
        static uint8_t diagnosticsSeconds;
        if(++diagnosticsSeconds >= 60){
            diagnosticsSeconds = 0;
            printf("free_RAM:%d,Frame:%d\n",(int)esp_get_free_heap_size(),(int)FrameRateTemp);
            printf("K:%d  V:%d\n",KeyBtn_Val,(int)VoltageVal);
        }
    }
}

TaskHandle_t spiTaskHandle = NULL;
static TaskHandle_t renderTaskHandle = NULL;
static volatile uint8_t lcd_send_active;

void rendLcd(void *arg){
    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
#ifdef GVIEW_UI_AUDIT
        uiAuditBeforeFrame();
#else
        keyCheckC();
#endif
        update();
        drawPageReturnHint();
        // The LCD task reads the prepared image, so GVIEW can draw the next
        // frame while the previous image is being sent.
        while(lcd_send_active) vTaskDelay(1);
        LCD_SetSkipUnchanged(pageNow != PAGE_HOME && pageNow != PAGE_GARDEN &&
                             pageNow != PAGE_SHOP && pageNow != PAGE_GARDENSHOP);
        LCD_PrepareFrame(rendSX, rendSY, rendEX, rendEY);
#ifdef GVIEW_UI_AUDIT
        uiAuditAfterFrame();
#endif
        lcd_send_active = 1;
        xTaskNotifyGive(spiTaskHandle);
        FrameRateTemp ++;
    }
}

#include "esp_log.h"
void SceneTask(void *arg){
    while(1){
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if(lcd_send_active){
            LCD_SendPrepared();
            lcd_send_active = 0;
        }
    }
}

void app_main(void)
{
    // 将所有组件的日志级别设置为“无”，关闭所有日志
    // close(STDOUT_FILENO);
    printf("version 2.1\n");
    
    // 1. 配置慢速时钟源
    //rtc_clk_slow_freq_set(RTC_SLOW_FREQ_RTC);       // 内部RTC
    // 2. 启用时钟校准（如果使用内部RC）
    //rtc_clk_cal(RTC_CAL_32K_XTAL, 100);  // 校准周期
    

    //DelayMs(10); 
    update_rtc_Txt();
    
    
    romInit();
    moneyOld = dataSaveInfo.money;
    //DelayMs(10); 
    //LCD初始化
    LCD_Init();
    //DelayMs(10);   
    viewInit(320,240,10,1200,50,40,25);
    gviewSetNativeTextHooks(LCD_NativeTextBegin,LCD_NativeTextDraw);
    // Every page now draws images in the 1:1 framebuffer; no icon overlay is needed.
    gviewSetNativeIconHooks(NULL,NULL);
    
    rtcInit();
    xTaskCreate(SceneTask,"SPCT",4096,NULL,5,&spiTaskHandle);
    renderHomeInit();
    // renderWifihomeInit();
    rendWin(0,0,winW,winH,BGColor);
    LCD_PrepareFrame(rendSX, rendSY, rendEX, rendEY);
    lcd_send_active = 1;
    xTaskNotifyGive(spiTaskHandle);
    //-----背光pwm控制------
    backLightInit();
    // setBackLightVal(dataSaveInfo.brightnessNum); 
    resetBackLightVal(); 

    //----------------------
    // gpioInit();    
    adcInit();
    soundInit();

     //-----任务-----
    xTaskCreate(adc_task,"ADCT",4096,NULL,5,NULL);
    xTaskCreate(soundTask,"SDCT",4096,NULL,5,NULL);
    // xTaskCreate(GXTHC3Task,"GXCT",2048,NULL,5,NULL);//温湿度
    xTaskCreate(OneSecFunction,"OSCT",8192,NULL,5,NULL);
    xTaskCreate(rendLcd,"REDT",4096,NULL,6,&renderTaskHandle);

    wifi_Init();

    const TickType_t frameDelay = pdMS_TO_TICKS(1000 / fps);
    while(1){
        xTaskNotifyGive(renderTaskHandle);
        vTaskDelay(frameDelay);
    }
}
