
#include "./menu.h"
#include "../../rom.h"
#include "../../adc.h"
#include "../weather/weather.h"
#include "../pageDraw.h"
#include <math.h>
#define  menuNumAll 10

short MenuAngleNow;
short MenuSeleteNow = 8;
void helpInit(void);
struct DisplayObject* menuIcoArr[menuNumAll];
struct DisplayObject* menuNameSp;
unsigned char* menuIcoDataArr[] = {ICO_city,ICO_light,ICO_ring,ICO_time,ICO_tomato,setIco,wifiIco,ICO_weather,ICO_music,ICO_help};
const char* menuNameTxt[] ={"城市选择","亮度设置","闹钟设置","计时工具","番茄时钟","系统设置","WIFI设置","恢复出厂","声音设置","更新与支持"};

void factoryEx(void){
    rendWin(0,0,winW,winH,BGColor);
}
void Factory_KeyDownML(void){
    renderMenuInit();
}
void Factory_KeyDownMR(void){
    delayed_restart();
}
void renderFactory(void){
    pageNow = PAGE_MENU;
    BGColor = 0xFFFF;
    resetPageView(&factoryEx);
    enablePageReturnHint();
    drawPageUITxt("取消", "确认重置", "");
    setPageUILongKeyText(NULL,"重置");

    static const char *const description[]={
        "将清除个人设置和", "Wi-Fi、花园植物等数据",
        "闹钟与铃声选择会重置", "内置MP3文件保留", "确定恢复出厂?"
    };
    const u8 lines=sizeof(description)/sizeof(description[0]);
    const short linePitch=24, textHeight=16;
    const short blockHeight=(lines-1)*linePitch+textHeight;
    const short top=WS_UI_HEADER_HEIGHT+
        (WS_UI_FOOTER_TOP-WS_UI_HEADER_HEIGHT-blockHeight)/2;
    for(u8 i=0;i<lines;++i){
        addChild(0,newChar(12,description[i],0,
            WS_UI_MID-getTxtLength(description[i])*4,top+i*linePitch));
    }


    setKeyFun(KML,&Factory_KeyDownML);
    setKeyFun(KMR,&Factory_KeyDownMR);
    setKeyFun(KM3,&Factory_KeyDownML);
    setKeyFun(KM2,&Factory_KeyDownMR);
}
//-------------按键处理----------------
u8 changeMenuSeleteNow(void){

    if(MenuSeleteNow>=0)
    return (menuNumAll-1 - MenuSeleteNow % menuNumAll) ;
    else
    return (-MenuSeleteNow-1) % menuNumAll ;
}

//按键操作回调函数
void Menu_KeyDownM3(void){
    MenuSeleteNow --;
    Tween(&MenuAngleNow,.5,(MenuSeleteNow+5)*(360/menuNumAll) ,0,fs_Cubic_EaseOut);
    menuNameSp->f = menuNameTxt[changeMenuSeleteNow()];
    menuNameSp->x = WS_UI_LEFT+10;
}
void Menu_KeyDownM2(void){
     switch (changeMenuSeleteNow())
    {
    case 0:
        CitySelectInit();
        break;
    case 1:
        setBGLightInit();
        break;
    case 2:
        clockSettingInit();
        break;
    case 3:
        clockCountPageInit();
        break;
    case 4:
        tomatoPageInit();
        break;
    case 5:
        setSystemInit();
        break;
    case 6:
        wifiHomePageInit();
        break;
    case 7:
        //恢复出厂
        renderFactory();
        break;
    case 8:
        setSoundInit();
        break;
    case 9:
        helpInit();
        break;
    default:
        break;
    }
}


void Menu_KeyDownM1(void){
    MenuSeleteNow ++;
    Tween(&MenuAngleNow,.5,(MenuSeleteNow+5)*(360/menuNumAll) ,0,fs_Cubic_EaseOut);
    menuNameSp->f = menuNameTxt[changeMenuSeleteNow()];
    menuNameSp->x = WS_UI_LEFT+10;
}
    




void Menu_KeyDownML(void){
    if(pageNow == PAGE_HELP){
        renderMenuInit();
    }else backToHomePage();
}

void Menu_KeyDownMR(void){    
    if(pageNow == PAGE_HELP){
        renderMenuInit();
    }else{
        Menu_KeyDownM1();
    }
}

//-----------------------------------------------------

//操作逻辑及渲染
#define  cW  90
// Restore the original right-side semicircle: the circle centre is off-screen.
short BCx = WS_UI_RIGHT+20;    //中心点X
short BCy = 120;  //中心点Y
void menuEx(void){

    for(u8 i=0;i<menuNumAll;i++){
        short cx   =   BCx   +   cW   *   cos((MenuAngleNow + i*(360/menuNumAll) + 36)  *   3.14/180  );
        short cy   =   BCy   +   cW   *   sin((MenuAngleNow + i*(360/menuNumAll) + 36)  *   3.14/180  );
        menuIcoArr[i]->x = cx-menuIcoArr[i]->width/2;
        menuIcoArr[i]->y = cy-menuIcoArr[i]->height/2;
    } 
    rendWin(0,0,winW,winH,BGColor);
}
void helpEx(void){
    rendWin(0,0,winW,winH,BGColor);
}

void helpInit(void){
    pageNow = PAGE_HELP;
    BGColor = 0xFFFF;
    resetPageView(&helpEx);
    drawPageUITxt("", "返回", "");
    setPageUILongKeyText(NULL,"返回");
    addChild(0,newPicSingle(&helpQr,0,100,64));
    
    // addChild(0,newChar(12,"https://babybbq.cn/travelclock/index.php",0,34,150));
    setKeyFun(KML,&Menu_KeyDownML);
    setKeyFun(KM2,&Menu_KeyDownML);
    setKeyFun(KMR,&Menu_KeyDownMR);
}

void renderMenuInit(void){
    pageNow = PAGE_MENU;
    BGColor = 0xFFFF;
    resetPageView(&menuEx);
    MenuSeleteNow = MenuSeleteNow%menuNumAll;
    MenuAngleNow = (MenuSeleteNow+5)*(360/menuNumAll) ;
    
    for(u8 i=0;i<menuNumAll;i++){
        menuIcoArr[i] = newNativePic(menuIcoDataArr[i],0,30);
        addChild(0,menuIcoArr[i]);
    }

    addChild(0,newRoundRectDisObj(0x8F1E,140,38,WS_UI_LEFT,102,12,1));
    //列表名
    menuNameSp = newChar(12,menuNameTxt[changeMenuSeleteNow()],0,
                         WS_UI_LEFT+10,112);
    addChild(1,menuNameSp);

    setKeyFun(KML,&Menu_KeyDownML);
    setKeyFun(KMR,&Menu_KeyDownMR);
    setKeyFun(KM1,&Menu_KeyDownM1);
    setKeyFun(KM2,&Menu_KeyDownM2);
    setKeyFun(KM3,&Menu_KeyDownM3);
}

// Hold PWR to open the action menu.
static u8 powerMenuSelection;
static DisplayObject *powerMenuBg[4];
static DisplayObject *powerMenuText[4];
static char powerMenuMusicText[24];
static char *powerMenuLabels[4] = {"设置", "关机", "返回主界面", powerMenuMusicText};

static void powerMenuEx(void){
    rendWin(0,0,winW,winH,BGColor);
}

static void powerMenuRefresh(void){
    snprintf(powerMenuMusicText,sizeof(powerMenuMusicText),"首页音乐：%s",
             dataSaveInfo.homeBgmSelection ? "开" : "关");
    powerMenuText[3]->x=WS_UI_MID-getTxtLength(powerMenuMusicText)*4;
    for(u8 i=0;i<4;i++){
        powerMenuBg[i]->color = i == powerMenuSelection ? 0x0635 : 0xE71C;
        powerMenuText[i]->color = i == powerMenuSelection ? 0xFFFF : 0x0000;
    }
}
static void powerMenuPrevious(void){
    powerMenuSelection = (powerMenuSelection + 3) % 4;
    powerMenuRefresh();
}

static void powerMenuNext(void){
    powerMenuSelection = (powerMenuSelection + 1) % 4;
    powerMenuRefresh();
}

static void powerMenuConfirm(void){
    switch(powerMenuSelection){
        case 0: renderMenuInit(); break;
        case 1: requestPowerOff(); break;
        case 2: backToHomePage(); break;
        case 3:
            if(dataSaveInfo.homeBgmSelection){
                dataSaveInfo.homeBgmPreferredSelection=dataSaveInfo.homeBgmSelection;
                dataSaveInfo.homeBgmSelection=HOME_BGM_SILENT;
            }else{
                dataSaveInfo.homeBgmSelection=dataSaveInfo.homeBgmPreferredSelection;
                if(dataSaveInfo.homeBgmSelection<1 ||
                   dataSaveInfo.homeBgmSelection>HOME_BGM_TRACK_COUNT)
                    dataSaveInfo.homeBgmSelection=HOME_BGM_DEFAULT_SELECTION;
            }
            writeData();
            powerMenuRefresh();
            break;
    }
}

void renderPowerMenuInit(void){
    pageNow = PAGE_POWER_MENU;
    backHomeTime = 120;
    BGColor = 0xFFFF;
    resetPageView(&powerMenuEx);
    powerMenuSelection = 0;
    const short panelWidth = 162, panelHeight = 185;
    const short panelTop = (winH-panelHeight)/2;
    const short buttonWidth = 144, buttonHeight = 28;
    const short textHeight = 16; // Logical size 12 renders as 16-pixel glyphs.
    const char *title = "操作选项";
    addChild(0,newRoundRectDisObj(0xFFFF,panelWidth,panelHeight,
                                 WS_UI_MID-panelWidth/2,panelTop,12,1));
    addChild(1,newChar(12,title,0x0000,
                       WS_UI_MID-getTxtLength(title)*textHeight/4,panelTop+11));
    for(u8 i=0;i<4;i++){
        short y = panelTop+38+i*34;
        powerMenuBg[i] = newRoundRectDisObj(0xE71C,buttonWidth,buttonHeight,
                                          WS_UI_MID-buttonWidth/2,y,8,1);
        addChild(1,powerMenuBg[i]);
        powerMenuText[i] = newChar(12,powerMenuLabels[i],0x0000,
                                   WS_UI_MID-getTxtLength(powerMenuLabels[i])*textHeight/4,
                                   y+(buttonHeight-textHeight)/2);
        addChild(2,powerMenuText[i]);
    }
    powerMenuRefresh();
    setKeyFun(KML,&powerMenuPrevious);
    setKeyFun(KMR,&powerMenuNext);
    // BOOT (KM3) selects the previous row; PLUS (KM1) selects the next row.
    setKeyFun(KM1,&powerMenuNext);
    setKeyFun(KM2,&powerMenuConfirm);
    setKeyFun(KM3,&powerMenuPrevious);
}
