"""Exercise production header hit testing and Wi-Fi callbacks without a device."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def source(path):
    return (ROOT / path).read_bytes().decode("gb18030")


def function(text, signature):
    start = text.index(signature)
    brace = text.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end] + "\n"


page = source("main/PAGE/pageDraw.c")
wifi = source("main/PAGE/WIFI/wifiScanPage.c")
adc = source("main/adc.c")
swipe_constants = next(
    line for line in adc.splitlines() if line.startswith("enum { MAIN_SWIPE_NEXT=")
)
city = source("main/PAGE/CITY/city.c")
tomato = source("main/PAGE/TCLOCK/tclock.c")
constants = "\n".join(
    line for line in source("main/PAGE/pageDraw.h").splitlines()
    if line.startswith("#define WS_UI_")
)
stubs = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
enum { PAGE_HOME, PAGE_CALENDAR, PAGE_CLOCK, PAGE_WEATHER, PAGE_MENU,
       PAGE_POWER_MENU, PAGE_WIFI_HOME, PAGE_WIFI_SAVED, PAGE_WIFI_SAVED_ONE,
       PAGE_WIFI_SCAN, PAGE_WIFI_CONNECT, PAGE_CITY, PAGE_TCLCK, PAGE_TCLCK_SET,
       PAGE_WIFI_PW, PAGE_SOUND_SET, PAGE_SYSTEM_SET, PAGE_CLOCK_SET, PAGE_CLOCKCOUNT_SET, PAGE_BGLIGHT_SET,
       PAGE_BGLIGHT_SCHEDULE, PAGE_SHOP, PAGE_GARDENSHOP };
enum { HEADER_TEXT_LEFT=3, HEADER_TEXT_MIDDLE=4, HEADER_TEXT_RIGHT=5 };
enum { moveClipMaxNum=10, sleeteBGColor=1, defaultBGColor=2, wifiHomeListAll=2 };
typedef struct { int x,y,height,visible,color; const char *f; } DisplayObject;
static struct { struct { int y; DisplayObject disObj; } movieclipArr[10]; } stage;
static DisplayObject header[3], selection, deleteButton, connectButton, menu;
static DisplayObject *seleteBg=&selection, *Wifi_delectBtn=&deleteButton;
static DisplayObject *Wifi_connectBtn=&connectButton, *wifiMenuListObj=&menu;
static u8 softKeyHeaderVisible=1, headerKeyEnabled[3];
static const char *longKeyText[2], *shortKeyText[3];
static u8 holdPromotionEnabled[2]={1,1};
static int headerKeyActions[3];
static struct { unsigned saveNumAll; } wifiSaveInfo;
static int pageNow, ScaningHd, wifiListNum, WifiConnectingStateOld;
static u8 WIFI_seleteNum, wifiMenuListNum, longDownL, longDownR;
static short wifiScanRowTop[21];
static short proviceNow, cityNow;
static u8 cityProviceNow, cityLen;
static int entered, selectedMenu;
static u8 tomatoPageStartFlag, tomatoPagePaushFlag;
static unsigned tomatoSetNumTemp1, tomatoSetNumTemp2, tomatoSetNumTemp3;
static struct { unsigned tomatoTime, tomatoTimeRelax, tomatoTimes; } dataSaveInfo;
static void showTClockTitle(void){ stage.movieclipArr[9].y=0; }
static void TClockStop(void){ tomatoPageStartFlag=0; tomatoPagePaushFlag=0; entered=10; }
static void TClocktart(u8 reset){
    (void)reset; showTClockTitle(); tomatoPageStartFlag=1; tomatoPagePaushFlag=0; entered=11;
}
static void TClockPaush(void){ showTClockTitle(); tomatoPagePaushFlag=1; entered=12; }
static void renderMenuInit(void){ entered=13; }
static void writeData(void){}
static void tomatoPageInit(void){ entered=14; }
static void KeyDownML(void){ entered=7; }
#define GRAY 0x8410
static unsigned footerDraws;
static uint8_t LCD_NativeTextDraw(const char *text,int16_t x,int16_t y,uint16_t color,
                                uint8_t size,int16_t left,int16_t top,int16_t right,int16_t bottom){
    (void)text;(void)x;(void)y;(void)color;(void)size;(void)left;(void)top;(void)right;(void)bottom;
    ++footerDraws; return 1;
}
#define KEY_COUNT 5
#define GPIO_NUM_0 0
#define GPIO_NUM_4 4
#define GPIO_NUM_5 5
static uint16_t KEY0Temp,KEY1Temp,KEY2Temp,KEY3Temp,KEY4Temp;
static int shutting_down,keyState,keySureTime=2,backHomeTime,aClockTimes;
static int levels[6]={1,1,1,1,1,1};
static int64_t fakeTime=1000000;
static bool usbConnected;
static bool touchPresent;
static uint8_t LCD_GetRotation(void){ return 0; }
static int touchX=120,touchY=120;
static bool touch_point(int *x,int *y){ *x=touchX; *y=touchY; return touchPresent; }
static int gpio_get_level(int pin){ return levels[pin]; }
static int64_t esp_timer_get_time(void){ return fakeTime; }
static bool usb_serial_jtag_is_connected(void){ return usbConnected; }
static void input_trace_record(const char *event,int64_t now,bool boot,bool usb){
    (void)event;(void)now;(void)boot;(void)usb;
}
static void input_trace_dump(int64_t now,bool boot,bool usb){
    (void)now;(void)boot;(void)usb;
}
#define USB_BOOT_GUARD_US 2000000
static void inputPageGesture(int a,int b,int c,int d){ (void)a;(void)b;(void)c;(void)d; }
static void saveSystemSettingsIfDirty(void){}
static void saveSoundSettingsIfDirty(void){}
static void saveTimerSoundSettingsIfDirty(void){}
static void leaveClockSettings(void){}
static void renderPowerMenuInit(void){}
static void hideTips(void){}
static void soundStopAll(void){}
#define WS_WIFI_LIST_TOP 32
static DisplayObject *getChildAt(DisplayObject *parent,unsigned index){
    (void)parent;
    assert(index>=3 && index<6);
    return &header[index-3];
}
static unsigned getTxtLength(const char *text){ return strlen(text); }
static int pageReturnHintVisible(void){ return 1; }
static void setMenuListY(DisplayObject *obj,int index){ (void)obj; selectedMenu=index; }
static void wifiSavedOneInit(void){ entered=1; }
static void wifiScanPageInit(void){ entered=2; }
static void wifiSavedInit(void){ entered=3; }
static void wifiConnectPageInit(void){}
static void renderHomeInit(void){ entered=5; }
static void inputPageInit(void (*callback)(void),int mode){
    (void)callback; (void)mode; entered=4;
}
static void KeyDownMR(void){ entered=5; }
'''

production = "\n".join([
    function(page, "static const char *pageHoldKeyText("),
    function(page, "static u8 hasActionText("),
    function(page, "static void refreshHeaderLabel("),
    function(page, "static void refreshHeaderLabels(void){"),
    function(page, "static void setHeaderLabel("),
    function(page, "void setPageUIKeyText("),
    function(page, "void setPageUIMiddleText("),
    function(page, "u8 pageUIKeyEnabled("),
    function(page, "int pageUIKeyAction("),
    function(page, "void setPageUILongKeyText("),
    function(page, "void setPageUIHoldPromotion("),
    function(page, "void drawPageReturnHint("),
    function(adc, "static int touch_key_at("),
    function(adc, "static int content_gesture_key("),
    function(adc, "void keyCheckC("),
    function(wifi, "static void refreshWifiKeyHints("),
    function(wifi, "static void updateScanSelection("),
    function(wifi, "static void KeyDownM1("),
    function(wifi, "static void KeyDownM2("),
    function(wifi, "static void KeyDownM3("),
    function(city, "static void refreshCityKeyHints("),
    function(tomato, "static void refreshTomatoKeyHints("),
    function(tomato, "static void KeyDownML(").replace("KeyDownML", "tomatoLeft"),
    function(tomato, "static void KeyDownMR(").replace("KeyDownMR", "tomatoRight"),
])

scenarios = r'''
static void expect(const char *left,const char *middle,const char *right,int leftKey,int rightKey){
    const char *expected[3]={left,middle,right};
    const int physicalKeys[3]={leftKey,1,rightKey};
    for(unsigned i=0;i<3;++i){
        assert(strcmp(header[i].f,expected[i])==0);
        assert(header[i].visible==(*expected[i]!=0));
        assert(touch_key_at(i*80+40,10)==(*expected[i] ? physicalKeys[i] : -1));
        assert(content_gesture_key(i*80+40,10,i*80+40,10)==-1);
    }
}
static void dispatch(int key){
    if(pageNow==PAGE_TCLCK){
        if(key==3) tomatoLeft();
        else if(key==4) tomatoRight();
        refreshTomatoKeyHints();
    }else{
        if(key==2) KeyDownM3();
        else if(key==1) KeyDownM2();
        else if(key==0) KeyDownM1();
        else if(key==3) KeyDownML();
        else if(key==4) KeyDownMR();
        refreshWifiKeyHints();
    }
}
static void tap(unsigned cell){ dispatch(touch_key_at(cell*80+40,10)); }
static void physicalClick(int pin){
    levels[pin]=0; keyCheckC(); assert(keyState==0);
    fakeTime+=100000;
    levels[pin]=1; keyCheckC();
    assert(keyState!=0);
    for(int key=0;key<5;++key) if(keyState & (1<<(key*2))) dispatch(key);
    fakeTime+=100000;
    keyCheckC(); assert(keyState==0);
}
static void visibleSelection(void){
    int top=seleteBg->y+stage.movieclipArr[0].y;
    assert(top>=32 && top+seleteBg->height<=220);
    assert(seleteBg->height==wifiScanRowTop[WIFI_seleteNum+1]-wifiScanRowTop[WIFI_seleteNum]);
}
int main(void){
    pageNow=PAGE_WIFI_SCAN;
    keyCheckC(); // Initialize button state with BOOT released.
    wifiListNum=20; seleteBg->visible=1; stage.movieclipArr[0].y=32;
    for(int i=0;i<20;++i) wifiScanRowTop[i+1]=wifiScanRowTop[i]+(i%2 ? 44 : 28);
    updateScanSelection(); refreshWifiKeyHints();
    expect("上一项","输入密码","下一项",2,0);
    footerDraws=0; drawPageReturnHint(); assert(footerDraws==2);
    tap(0); assert(WIFI_seleteNum==19); visibleSelection();
    tap(2); assert(WIFI_seleteNum==0); visibleSelection();
    tap(1); assert(entered==4);
    for(int i=1;i<20;++i){
        tap(2); assert(WIFI_seleteNum==i); visibleSelection();
        expect("上一项","输入密码","下一项",2,0);
    }
    tap(2); assert(WIFI_seleteNum==0 && entered==4);
    KeyDownM1(); assert(WIFI_seleteNum==1);
    tap(0); assert(WIFI_seleteNum==0); visibleSelection();
    assert(stage.movieclipArr[0].y==32);
    physicalClick(GPIO_NUM_4); assert(WIFI_seleteNum==1);
    physicalClick(GPIO_NUM_0); assert(WIFI_seleteNum==0);
    physicalClick(GPIO_NUM_0); assert(WIFI_seleteNum==19);
    ScaningHd=1; refreshWifiKeyHints(); expect("返回","","",3,-1);
    assert(!*longKeyText[1]);
    tap(2); assert(WIFI_seleteNum==19);
    ScaningHd=0; wifiListNum=0; seleteBg->visible=0;
    refreshWifiKeyHints(); expect("返回","","刷新",3,4);
    footerDraws=0; drawPageReturnHint(); assert(footerDraws==0);
    physicalClick(GPIO_NUM_4); assert(entered==5);

    pageNow=PAGE_WIFI_SAVED; wifiSaveInfo.saveNumAll=3; wifiMenuListNum=0;
    refreshWifiKeyHints(); expect("上一项","查看","下一项",2,0);
    tap(0); assert(wifiMenuListNum==2 && selectedMenu==2);
    tap(2); assert(wifiMenuListNum==0 && selectedMenu==0);
    tap(1); assert(entered==1);
    wifiSaveInfo.saveNumAll=1; refreshWifiKeyHints(); expect("上一项","查看","下一项",2,0);
    tap(0); assert(wifiMenuListNum==0);
    tap(2); assert(wifiMenuListNum==0);
    wifiSaveInfo.saveNumAll=0; refreshWifiKeyHints(); expect("返回","","",3,-1);

    pageNow=PAGE_WIFI_HOME; wifiMenuListNum=0;
    refreshWifiKeyHints(); expect("上一项","进入","下一项",2,0);
    tap(0); assert(wifiMenuListNum==1);
    tap(2); assert(wifiMenuListNum==0);
    tap(1); assert(entered==2);

    pageNow=PAGE_WIFI_SAVED_ONE; deleteButton.color=defaultBGColor;
    refreshWifiKeyHints(); expect("切换","连接","切换",2,0);
    tap(0); expect("切换","删除","切换",2,0);
    tap(2); expect("切换","连接","切换",2,0);

    pageNow=PAGE_WIFI_CONNECT;
    for(int state=0;state<=4;++state){
        WifiConnectingStateOld=state;
        refreshWifiKeyHints();
        expect("返回",state>=3 ? "返回首页" : "","",3,-1);
        footerDraws=0; drawPageReturnHint();
        assert(footerDraws==0 && !*longKeyText[1]);
    }
    entered=0; tap(1); assert(entered==5);

    holdPromotionEnabled[1]=1; // A new page resets the promotion policy.
    pageNow=PAGE_CITY; setPageUILongKeyText("返回","保存");
    cityProviceNow=0; proviceNow=0;
    refreshCityKeyHints(); expect("上一项","选城市","下一项",2,0);
    proviceNow=28; refreshCityKeyHints(); expect("上一项","选城市","下一项",2,0);
    cityProviceNow=1; cityLen=1; cityNow=0;
    refreshCityKeyHints(); expect("上一项","选省份","下一项",2,0);

    pageNow=PAGE_TCLCK; tomatoPageStartFlag=tomatoPagePaushFlag=0;
    refreshTomatoKeyHints(); expect("返回","设置","启动",3,4);
    physicalClick(GPIO_NUM_4); assert(entered==11 && tomatoPageStartFlag==1);
    expect("停止","隐藏菜单","暂停",3,4);
    physicalClick(GPIO_NUM_4); assert(entered==12 && tomatoPagePaushFlag==1);
    expect("停止","隐藏菜单","继续",3,4);
    physicalClick(GPIO_NUM_4); assert(entered==11 && !tomatoPagePaushFlag);
    // Side clicks perform their action directly even when the menu is hidden.
    stage.movieclipArr[9].y=-24; refreshTomatoKeyHints();
    physicalClick(GPIO_NUM_0); assert(entered==10 && !tomatoPageStartFlag);
    stage.movieclipArr[9].y=0; refreshTomatoKeyHints();
    physicalClick(GPIO_NUM_0); assert(entered==13);
    // Holding a key must not also run its short action on release.
    pageNow=PAGE_WIFI_SCAN; ScaningHd=0; wifiListNum=0; seleteBg->visible=0;
    refreshWifiKeyHints(); levels[4]=0; keyCheckC();
    fakeTime+=700000; keyCheckC(); assert(keyState==(1<<(4*2)));
    levels[4]=1; keyCheckC(); assert(keyState==0);
    puts("PASS: short-first physical/touch actions; Pomodoro start/pause/resume/stop/return; Wi-Fi empty/busy/boundary states; no duplicate release actions");
}
'''

with tempfile.TemporaryDirectory(prefix="gview-button-hints-") as folder:
    test = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    test.write_text(constants + "\n" + swipe_constants + "\n" + stubs + production + scenarios,
                    encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
