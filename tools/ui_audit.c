#include "config.h"
#include "GVIEW/gview.h"
#include "LCD/lcd_init.h"
#include "NET/connect.h"
#include "rom.h"
#include "PAGE/INPUTTXT/inputtxt.h"
#include "PAGE/WIFI/wifiScanPage.h"
#include "PAGE/GARDEN/garden.h"
#include <stdio.h>
#include <string.h>

extern void doKeyFun(KeyName key);
#include <assert.h>
extern void renderFactory(void);
extern void wifiAuditConnectPage(void);
extern void helpInit(void);
extern void LCD_AuditDumpFrame(const char *name);

typedef struct { const char *name; void (*init)(void); unsigned categories, moves; } AuditPage;
static void passwordPage(void){
    snprintf(inputTxtTemp,sizeof(inputTxtTemp),"Demo_password_1234567890_abcdefghijklmnopqrstuvwxyz_ABCDEFGH");
    inputPageInit(wifiConnectPageInit,0);
}

static unsigned keyboardConfirmCount;
static void keyboardConfirmed(void){ ++keyboardConfirmCount; }
static void passwordConfirmPage(void){ passwordPage(); doKeyFun(KM3); }
static void emptyPasswordPage(void){ inputTxtTemp[0]=0; inputPageInit(wifiConnectPageInit,0); }
static void maximumPasswordPage(void){
    for(unsigned i=0;i<63;++i) inputTxtTemp[i]='0'+i%10;
    inputTxtTemp[63]=0;
    inputPageInit(wifiConnectPageInit,0);
    doKeyFun(KM2);
    assert(strlen(inputTxtTemp)==63);
    hideTips();
    for(int i=0;i<3;i++) doKeyFun(KM3);
    doKeyFun(KM2);
    assert(strlen(inputTxtTemp)==62);
    for(int i=0;i<3;i++) doKeyFun(KM1);
    doKeyFun(KM2);
    assert(strlen(inputTxtTemp)==63 && inputTxtTemp[62]=='a');
    printf("@UI_CHECK keyboard_capacity passed\n");
}
static void keyboardButtonsPage(void){
    inputTxtTemp[0]=0;
    keyboardConfirmCount=0;
    inputPageInit(keyboardConfirmed,0);
    doKeyFun(KM2);
    doKeyFun(KMR); doKeyFun(KM2);
    doKeyFun(KMR); doKeyFun(KM2);
    doKeyFun(KMR); doKeyFun(KM2);
    assert(strcmp(inputTxtTemp,"aA1!")==0);
    doKeyFun(KM3); doKeyFun(KM3); doKeyFun(KM2);
    assert(strcmp(inputTxtTemp,"aA1! ")==0);
    doKeyFun(KM3); doKeyFun(KM2);
    assert(strcmp(inputTxtTemp,"aA1!")==0);
    doKeyFun(KM1); doKeyFun(KM1); doKeyFun(KM2);
    assert(keyboardConfirmCount==1);
    printf("@UI_CHECK keyboard_buttons passed\n");
}
static void keyboardTouchPage(void){
    inputTxtTemp[0]=0;
    keyboardConfirmCount=0;
    inputPageInit(keyboardConfirmed,0);
    inputPageGesture(21,122,21,122);
    inputPageGesture(92,99,92,99);
    inputPageGesture(21,122,21,122);
    inputPageGesture(148,99,148,99);
    inputPageGesture(45,122,45,122);
    inputPageGesture(204,99,204,99);
    inputPageGesture(21,122,21,122);
    assert(strcmp(inputTxtTemp,"aA1!")==0);
    inputPageGesture(40,210,40,210);
    inputPageGesture(120,210,120,210);
    assert(strcmp(inputTxtTemp,"aA1 ")==0);
    inputPageGesture(200,210,200,210);
    assert(keyboardConfirmCount==1);
    inputPageGesture(100,50,100,50);
    inputPageGesture(239,115,239,115);
    assert(strcmp(inputTxtTemp,"aA1 ")==0 && keyboardConfirmCount==1);
    inputPageGesture(120,230,120,230);
    assert(strcmp(inputTxtTemp,"aA1 ")==0 && keyboardConfirmCount==1);
    inputPageGesture(180,150,80,150);
    inputPageGesture(21,122,21,122);
    assert(strcmp(inputTxtTemp,"aA1 a")==0);
    inputPageGesture(80,150,180,150);
    inputPageGesture(21,122,21,122);
    assert(strcmp(inputTxtTemp,"aA1 a!")==0);
    printf("@UI_CHECK keyboard_touch passed\n");
}
static void shopPurchasePage(void){
    dataSaveInfo.money=200;
    itemSaveInfo.itemMonster=1;
    itemSaveInfo.monsterIndex=0;
    renderShopInit();
    doKeyFun(KM1);
    doKeyFun(KM2);
    assert(dataSaveInfo.money==0 && itemSaveInfo.itemMonster==3 && itemSaveInfo.monsterIndex==0);
    doKeyFun(KM2);
    assert(dataSaveInfo.money==0 && itemSaveInfo.monsterIndex==1);
    hideTips();
    printf("@UI_CHECK shop_purchase passed\n");
}
static void plantPurchasePage(void){
    dataSaveInfo.money=200;
    gardenSaveInfo.itemGarden=1;
    gardenSaveInfo.gardenIndex=0;
    renderGardenShopInit();
    doKeyFun(KM1);
    doKeyFun(KM2);
    assert(dataSaveInfo.money==0 && gardenSaveInfo.itemGarden==3);
    doKeyFun(KM2);
    assert(dataSaveInfo.money==0);
    hideTips();
    printf("@UI_CHECK plant_purchase passed\n");
}
static void brightnessControlsPage(void){
    u8 oldTimeout=dataSaveInfo.idleTimeoutMinutes;
    u8 oldLevel=dataSaveInfo.idleBrightnessLevel;
    setBGLightInit();
    assert(pageNow==PAGE_BGLIGHT_SET);
    doKeyFun(KM2); doKeyFun(KM1); doKeyFun(KM2);
    assert(dataSaveInfo.idleTimeoutMinutes!=oldTimeout);
    doKeyFun(KM1); doKeyFun(KM2); doKeyFun(KM1); doKeyFun(KM2);
    assert(dataSaveInfo.idleBrightnessLevel!=oldLevel);
    doKeyFun(KM1); doKeyFun(KM2);
    assert(pageNow==PAGE_BGLIGHT_SCHEDULE);
    doKeyFun(KML);
    assert(pageNow==PAGE_BGLIGHT_SET);
    printf("@UI_CHECK brightness_navigation passed\n");
}
static const AuditPage pages[] = {
    {"menu",renderMenuInit,0,0},
    {"system",setSystemInit,0,0},
    {"sound",setSoundInit,0,0},
    {"alarm",clockSettingInit,0,0},
    {"brightness",setBGLightInit,0,0},
    {"brightness_schedule",setBGLightScheduleInit,0,0},
    {"brightness_controls",brightnessControlsPage,0,0},
    {"city",CitySelectInit,0,0},
    {"wifi_home",wifiHomePageInit,0,0},
    {"wifi_saved",wifiSavedInit,0,0},
    {"wifi_detail",wifiSavedOneInit,0,0},
    {"wifi_scan",wifiScanPageInit,0,0},
    {"wifi_connection",wifiAuditConnectPage,0,0},
    {"wifi_keyboard",passwordPage,0,0},
    {"wifi_upper",passwordPage,1,0},
    {"wifi_numbers",passwordPage,2,0},
    {"wifi_symbols",passwordPage,3,0},
    {"wifi_confirm",passwordConfirmPage,0,0},
    {"wifi_empty",emptyPasswordPage,0,0},
    {"wifi_maximum",maximumPasswordPage,0,0},
    {"wifi_buttons",keyboardButtonsPage,0,0},
    {"wifi_touch",keyboardTouchPage,0,0},
    {"shop_dragon",renderShopInit,0,1},
    {"shop_horse",renderShopInit,0,5},
    {"shop_tent",renderShopInit,1,1},
    {"shop_bike",renderShopInit,2,1},
    {"shop_hat",renderShopInit,3,1},
    {"shop_shirt",renderShopInit,4,1},
    {"shop_pants",renderShopInit,5,1},
    {"shop_shoes",renderShopInit,6,1},
    {"shop_gloves",renderShopInit,7,1},
    {"shop_bag",renderShopInit,8,1},
    {"garden_shop",renderGardenShopInit,0,9},
    {"garden",renderGardenInit,0,0},
    {"timer",clockCountPageInit,0,0},
    {"tomato",tomatoPageInit,0,0},
    {"help",helpInit,0,0},
    {"factory",renderFactory,0,0},
    {"calendar",renderCalendarInit,0,0},
    {"weather",weatherInit,0,0},
    {"clock",clockPageInit,0,0},
    {"home",renderHomeInit,0,0},
    {"shop_purchase",shopPurchasePage,0,0},
    {"plant_purchase",plantPurchasePage,0,0},
};
static unsigned step, frame, bootFrames;
static int done;
static DATASAVEINFO originalData;
static ITEMSAVEINFO originalItems;
static GARDENSAVEINFO originalGarden;
static WIFISAVEINFO originalWifi;

void uiAuditBeforeFrame(void){
    if(done || bootFrames++ < 200) return;
    if(frame == 0){
        if(step == 0){
            originalData=dataSaveInfo; originalItems=itemSaveInfo;
            originalGarden=gardenSaveInfo; originalWifi=wifiSaveInfo;
            // Diagnostic data stays in RAM; the audit never confirms a purchase.
            wifiSaveInfo.saveNumAll=1;
            snprintf(wifiSaveInfo.ssid[0],sizeof(wifiSaveInfo.ssid[0]),"DEMO_NETWORK_LONG_NAME_12345678");
            snprintf(wifiSaveInfo.password[0],sizeof(wifiSaveInfo.password[0]),"demo_password_1234567890_abcdefghijklmnopqrstuvwxyz_ABCDEFGH");
            itemSaveInfo.monsterIndex=0;
            for(int i=0;i<5;i++) addCloverToFloor();
        }
        const AuditPage *page=&pages[step];
        backHomeTime=120;
        page->init();
        for(unsigned i=0;i<page->categories;i++) doKeyFun(KMR);
        for(unsigned i=0;i<page->moves;i++) doKeyFun(KM1);
    }
    if(strcmp(pages[step].name,"wifi_scan") == 0){
        wifiListNum=2;
        ScaningHd=0;
        snprintf((char*)ap_info[0].ssid,sizeof(ap_info[0].ssid),"DEMO_NETWORK_LONG_NAME_12345678");
        snprintf((char*)ap_info[1].ssid,sizeof(ap_info[1].ssid),"DEMO_SECOND_NETWORK");
        refrushScanHd=1;
    }
}

void uiAuditAfterFrame(void){
    if(done || bootFrames <= 200) return;
    if(++frame < 20) return;
    LCD_AuditDumpFrame(pages[step].name);
    frame=0;
    if(++step >= sizeof(pages)/sizeof(pages[0])){
        done=1;
        dataSaveInfo=originalData; itemSaveInfo=originalItems;
        gardenSaveInfo=originalGarden; wifiSaveInfo=originalWifi;
        printf("@UI_DONE\n");
        renderHomeInit();
    }
}
