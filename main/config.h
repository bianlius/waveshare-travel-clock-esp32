#ifndef __CONFIG_H_
#define __CONFIG_H_

#include "sdkconfig.h"

#include "stdint.h"
#include "./pic_data/picGroup.h"

#define true 1
#define false 0
#define deviceHd 0 //0:单片机 1:win版
typedef unsigned int  u32;
typedef unsigned short  u16;
typedef unsigned char  u8;


#define sceneBGColor 0x863e

extern uint16_t VoltageVal;   
extern u8 homeState;//主页形式类型
extern u16 backHomeTime;//无操作回主页倒计时
extern u8 tomatoPageStartFlag;//番茄闹钟是否在进行
extern u8 alarmClockNow;//当前闹钟编号;
extern uint16_t *picCache;//单次渲染的像素数量

typedef enum{
    PAGE_WIFIHOME,
    PAGE_HOME,
    PAGE_WEATHER,
    PAGE_CLOCK,
    PAGE_CALENDAR,
    
    PAGE_TCLCK,
    PAGE_WIFI_HOME,
    PAGE_WIFI_SAVED,
    PAGE_WIFI_SAVED_ONE,
    PAGE_WIFI_SCAN,
    PAGE_WIFI_PW,
    PAGE_WIFI_CONNECT,
    PAGE_MENU,
    PAGE_CITY,
    PAGE_CLOCKCOUNT,
    PAGE_TCLCK_SET,
    PAGE_TIME_SET,
    PAGE_BGLIGHT_SET, // Reserved for saved page identifiers; no longer opened.
    PAGE_BGLIGHT_SCHEDULE,
    PAGE_SOUND_SET,
    PAGE_CLOCK_SET,
    PAGE_CLOCKTIME_SET,
    PAGE_MONSTER,
    PAGE_MONSTERLIST,
    PAGE_MONSTERNEW,
    PAGE_GARDEN,
    PAGE_SHOP,
    PAGE_GARDENSHOP,
    PAGE_INPUTTXT,
    PAGE_HELP,
    PAGE_DEMO,
    PAGE_POWER_MENU,
    PAGE_BGM_SET,
    PAGE_SYSTEM_SET,
    PAGE_CLOCKCOUNT_SET,
}PAGE_ENUM;

//公共方法
void DelayMs(u16 num);
void update_rtc_Txt(void);
void updateTTime(u32 $time);
void resetHomeTempX(void);
char* getCityTxt(u8 p,u8 c);

//页面入口
extern void backToHomePage(void);
extern void weatherInit(void);
extern void clockSettingInit(void);
extern void setSoundInit(void);
extern void setSystemInit(void);
extern void saveSystemSettingsIfDirty(void);
extern void saveSoundSettingsIfDirty(void);
extern void saveTimerSoundSettingsIfDirty(void);
extern void leaveSystemSettings(void);
extern void leaveClockSettings(void);
extern void setBGLightInit(void);
extern void setBGLightScheduleInit(void);
extern void clockPageInit(void);
extern void tomatoPageInit(void);
extern void addTompBottomTxt(void);
extern void wifiHomePageInit(void);
extern void renderHomeInit(void);
extern void wifiSavedInit(void);
extern void wifiScanPageInit(void);
extern void inputPageInit(void (*fun)(void),u8 cnEnable);
extern void renderMenuInit(void);
extern void renderPowerMenuInit(void);
extern void CitySelectInit(void);
extern void clockCountPageInit(void);
extern void renderMonsterInit(void);
extern void renderGardenInit(void);
extern void renderWifihomeInit(void);
extern void renderCalendarInit(void);
extern void renderShopInit(void);
extern void renderGardenShopInit(void);
extern void renderInputtxtInit(void);

extern u8 pageNow;//当前页面
extern u8 weatherCode;//天气图标编号
extern u16 BGColor;//渲染的背景色
extern u8 DayTime;//画面变换标志 0-早上 1-白天 2-傍晚 3-夜晚

extern char tempTxt[10];
extern char waterTxt[10];
extern char tempNetTxt[10];
extern char weatherTxt[20];
//======win版专用==========
#if deviceHd
#define bool	_Bool
int random(void);
#else
//======单片机专用=========
//----2.1版本----
/* 定义LCD背光管脚端口 */
// #define LEDC_EN_IO              (10) // Define the output GPIO
// #define LEDC_OUTPUT_IO          (20) // Define the output GPIO
// //设置唤醒IO
// #define DEFAULT_WAKEUP_PIN      0 
// //蜂鸣器
// #define SD_OUTPUT_IO            (21) // Define the output GPIO
// /* 定义LCD通信管脚端口 */
// #define LCD_RES_GPIO_PIN        8
// #define LCD_DC_GPIO_PIN         7
// /* 定义ADC管脚端口 */
// #define ADC_INPUT_IO            (2)
// #define ADC_KEY_IO              (3)
//----3.0版本----
// /* 定义LCD背光管脚端口 */
#define LEDC_EN_IO              1//(10) // Define the output GPIO
#define LEDC_OUTPUT_IO          46
//设置唤醒IO
#define DEFAULT_WAKEUP_PIN      3//0 
//蜂鸣器
#define SD_OUTPUT_IO            4//(21) // Define the output GPIO
/* 定义LCD通信管脚端口 */
#define LCD_RES_GPIO_PIN        40
#define LCD_DC_GPIO_PIN         45
/* 定义ADC管脚端口 */
#define ADC_INPUT_IO           (2)
#define ADC_KEY_IO             (3)
//------------------------------
//版本说明 2.0与3.0需要更改config引脚 ADC值 main的GPIO初始化 休眠唤醒引脚
//

extern uint32_t IPAdressGet;
extern u16 sleepTimeNum;
#endif
//=========================
#endif