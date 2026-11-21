#if deviceHd
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#endif

#include <string.h>
#include "wifiScanPage.h"
#include "wifi_layout.h"
#include "GVIEW/viewport.h"
#include "GVIEW/gview.h"
#include "../../NET/connect.h"
#include "GVIEW/utf8_gb2312_switch.h"
#include "../../pic_data/picGroup.h"
#include "../pageDraw.h"
#include "../../NET/connect.h"
#include "../../rom.h"
#include "../../wifi_ssid.h"
#include "../INPUTTXT/inputtxt.h"

const char seachTxt[] = "正在搜索中...";


//==============主页变量==============
DisplayObject *wifiMenuListObj;//列表
u8 wifiMenuListNum = 0;//当前选择
//=============搜索页面变量======================

u8 WifiConnectingStateOld;//wifi连接状态
static u8 wifiDetailOld,wifiFailureOld,wifiSaveOld;
static short wifiScanRowTop[DEFAULT_SCAN_LIST_SIZE+1];
struct DisplayObject *seleteBg;//SSID选择框
uint8_t WIFI_seleteNum;//当前选择的wifi ssid 编号



u8 backPageFlag;//返回的上一页标志

#define wifiHomeListAll  2 

static DisplayObject* Wifi_delectBtn;
static DisplayObject* Wifi_connectBtn;
static char savedSsidText[80], savedPasswordText[96];
static char savedListText[10][72];

static u8 longDownL = 0;
static u8 longDownR = 0;
static void KeyDownMR(void);

static void refreshWifiKeyHints(void){
    const char *left="", *middle="", *right="";
    setPageUILongKeyText("返回",NULL);
    switch(pageNow){
        case PAGE_WIFI_HOME:
            left="上一项"; middle="进入"; right="下一项";
            setPageUILongKeyText("返回","进入");
            break;
        case PAGE_WIFI_SAVED:
            if(wifiSaveInfo.saveNumAll) middle="查看";
            if(wifiSaveInfo.saveNumAll){ left="上一项"; right="下一项"; }
            setPageUILongKeyText("返回",wifiSaveInfo.saveNumAll ? "查看" : "");
            break;
        case PAGE_WIFI_SAVED_ONE:
            left=right="切换";
            middle=Wifi_delectBtn->color==sleeteBGColor ? "删除" : "连接";
            setPageUILongKeyText("返回",middle);
            break;
        case PAGE_WIFI_SCAN:
            if(!ScaningHd && wifiListNum && seleteBg->visible){
                left="上一项";
                middle="输入密码";
                right="下一项";
            }
            setPageUILongKeyText("返回",ScaningHd ? "" : "刷新");
            break;
        case PAGE_WIFI_CONNECT:
            middle=WifiConnectingStateOld>=3 ? "返回首页" : "后台连接";
            setPageUILongKeyText("返回","");
            break;
        default:
            break;
    }
    setPageUIKeyText(0,left);
    setPageUIMiddleText(middle);
    setPageUIKeyText(2,right);
}
// Keep the selected row inside the list, including names that wrap.
static void updateScanSelection(void){
    if(!wifiListNum) return;
    short top=wifiScanRowTop[WIFI_seleteNum];
    short height=wifiScanRowTop[WIFI_seleteNum+1]-top;
    seleteBg->y=top;
    seleteBg->height=height;
    short visibleTop=top+stage.movieclipArr[0].y;
    if(visibleTop<WS_WIFI_LIST_TOP)
        stage.movieclipArr[0].y=WS_WIFI_LIST_TOP-top;
    else if(visibleTop+height>WS_UI_FOOTER_TOP)
        stage.movieclipArr[0].y=WS_UI_FOOTER_TOP-top-height;
}

//左
static void KeyDownM1(void){
    longDownL ++;   
    
    if(pageNow == PAGE_WIFI_SAVED_ONE){
        if(Wifi_delectBtn->color == sleeteBGColor){
            Wifi_delectBtn->color = defaultBGColor;
            Wifi_connectBtn->color = sleeteBGColor;
        }else{
            Wifi_delectBtn->color = sleeteBGColor;
            Wifi_connectBtn->color = defaultBGColor;
        }
    }else
    if(pageNow == PAGE_WIFI_SAVED){
        if (!wifiSaveInfo.saveNumAll) return;
        wifiMenuListNum=(wifiMenuListNum+1)%wifiSaveInfo.saveNumAll;
        setMenuListY(wifiMenuListObj,wifiMenuListNum);
    }else
    if(pageNow == PAGE_WIFI_HOME){
        wifiMenuListNum=(wifiMenuListNum+1)%wifiHomeListAll;
        setMenuListY(wifiMenuListObj,wifiMenuListNum);
    }else
    //WIFI列表选择
    if(pageNow==PAGE_WIFI_SCAN){
        if(!ScaningHd && seleteBg->visible && wifiListNum){
            WIFI_seleteNum=(WIFI_seleteNum+1)%wifiListNum;
            updateScanSelection();
        }
    }
}
//中
static void KeyDownM2(void){
    if(pageNow==PAGE_WIFI_SAVED_ONE){
        KeyDownMR();
    }else if(pageNow==PAGE_WIFI_SAVED){
        if(wifiSaveInfo.saveNumAll>0)wifiSavedOneInit();
    }else
    if(pageNow==PAGE_WIFI_HOME){
        if(wifiMenuListNum == 0){
            wifiScanPageInit();
        }else{
            wifiSavedInit();
        }
    }else
    if(pageNow==PAGE_WIFI_SCAN){
        //跳到输入密码页面
        if (!ScaningHd && wifiListNum > 0 && seleteBg->visible)
            inputPageInit(wifiConnectPageInit,0);
    }
    else if(pageNow==PAGE_WIFI_CONNECT){
        renderHomeInit();
    }
}
//右
static void KeyDownM3(void){
    longDownR ++;

    if(pageNow == PAGE_WIFI_SAVED_ONE){
        if(Wifi_delectBtn->color == sleeteBGColor){
            Wifi_delectBtn->color = defaultBGColor;
            Wifi_connectBtn->color = sleeteBGColor;
        }else{
            Wifi_delectBtn->color = sleeteBGColor;
            Wifi_connectBtn->color = defaultBGColor;
        }
    }else
    if(pageNow == PAGE_WIFI_SAVED){
        if (!wifiSaveInfo.saveNumAll) return;
        wifiMenuListNum=(wifiMenuListNum+wifiSaveInfo.saveNumAll-1)%wifiSaveInfo.saveNumAll;
        setMenuListY(wifiMenuListObj,wifiMenuListNum);
    }else 
    if(pageNow == PAGE_WIFI_HOME){
        wifiMenuListNum=(wifiMenuListNum+wifiHomeListAll-1)%wifiHomeListAll;
        setMenuListY(wifiMenuListObj,wifiMenuListNum);
    }else    

    
    //WIFI列表选择
     if(pageNow==PAGE_WIFI_SCAN){
        if(!ScaningHd && seleteBg->visible && wifiListNum){
            WIFI_seleteNum=(WIFI_seleteNum+wifiListNum-1)%wifiListNum;
            updateScanSelection();
        }
       
    }
}
static void KeyDownML(void){
    
    printf("ML:%d\n",pageNow);

    if(pageNow == PAGE_WIFI_SAVED_ONE){
        wifiSavedInit();
    }else
    if(pageNow == PAGE_WIFI_SAVED){
        wifiHomePageInit();
    }else
    if(pageNow == PAGE_WIFI_HOME){
        renderMenuInit();
    }else
    if(pageNow==PAGE_WIFI_SCAN){
        //回到主页
        wifiHomePageInit();
    }else if(pageNow==PAGE_WIFI_CONNECT){
        if(WifiConnectingStateOld == 4 && !backPageFlag){
            inputPageInit(wifiConnectPageInit,0);
        }else{
            wifiHomePageInit();
        }
    }
}

static void KeyDownMR(void){
    printf("MR:%d\n",pageNow);
    
    if(pageNow==PAGE_WIFI_SAVED_ONE){
        //删除已保存WIFI
        if(Wifi_delectBtn->color == sleeteBGColor){
            //删除WIFI
            wifi_forgetSaved(wifiSaveInfo.ssid[wifiMenuListNum]);
            deleteSavedWIFI(wifiMenuListNum);
            wifiSavedInit();
        }else{
            //连接WIFI
            //跳转提示页面
            wifiConnectPageInit_save();
            //确认连接WIFI
            //断开连接
            // Preferred saved network changes only after obtaining an address.
            //设置密码
            resetWIFIConnectInfo(wifiSaveInfo.ssid[wifiMenuListNum],wifiSaveInfo.password[wifiMenuListNum]);
            //开始连接
            wifi_startConnect();
        }
    }else
    if(pageNow==PAGE_WIFI_SAVED){
        if(wifiSaveInfo.saveNumAll>0)wifiSavedOneInit();
    }else
    if(pageNow==PAGE_WIFI_HOME){
        if(wifiMenuListNum == 0){
            wifiScanPageInit();
        }else{
            wifiSavedInit();
        }
    }else
    if(pageNow==PAGE_WIFI_SCAN){
        //刷新WIFI列表
        if(!ScaningHd){
            showTips(seachTxt,0);
            wifi_scan();
        }
    }
}



char ssidNameTemp[DEFAULT_SCAN_LIST_SIZE][72];//记录GB2312编码WIFI名
void resetScanView(void){
   hideTips();
   seleteBg->y = 0;
   seleteBg->height = WS_WIFI_ROW_MIN;
   WIFI_seleteNum = 0;
   wifiScanRowTop[0] = 0;
   stage.movieclipArr[0].y = WS_WIFI_LIST_TOP;
    while(stage.movieclipArr[0].length>1){
        removeChildAt(0,1);
    }
    if(!wifiListNum) addChild(0,newChar(12,WifiScanFailed ? "搜索失败,请长按右键重试" : "未发现可用的Wi-Fi",GRAY,WIFI_TEXT_LEFT,20));
    for(u8 i=0;i<wifiListNum;i++){
        char converted[66] = {0};
        utf8_to_gb2312((u8*)ap_info[i].ssid, strlen((char*)ap_info[i].ssid), (u8*)converted, sizeof(converted));
        unsigned lines=wrapPageText(ssidNameTemp[i],sizeof(ssidNameTemp[i]),converted,WIFI_TEXT_COLUMNS);
        short textHeight=16+(lines-1)*20;
        short rowHeight=textHeight+8;
        if(rowHeight<WS_WIFI_ROW_MIN) rowHeight=WS_WIFI_ROW_MIN;
        wifiScanRowTop[i+1]=wifiScanRowTop[i]+rowHeight;
        addChild(0,newChar(12,ssidNameTemp[i],0,WIFI_SCAN_TEXT_LEFT,
                           wifiScanRowTop[i]+(rowHeight-textHeight)/2));
    }
    updateScanSelection();
}

void addWIFIPageKey(void){
    setKeyFun(KM1,KeyDownM1);//拨轮左
    setKeyFun(KM2,KeyDownM2);//拨轮中
    setKeyFun(KM3,KeyDownM3);//拨轮右
    setKeyFun(KML,KeyDownML);//左肩键
    setKeyFun(KMR,KeyDownMR);//右肩键
    refreshWifiKeyHints();
}


//==========================================页面初始化================================================
void wifiSavedOneEx(void){
    refreshWifiKeyHints();
    rendWin(0,0,winW,winH,BGColor);
}


//单个已保存WIFI详情界面
void wifiSavedOneInit(void){
    pageNow = PAGE_WIFI_SAVED_ONE;
    BGColor = 0xFFFF;
    resetPageView(&wifiSavedOneEx);
    LCD_SetPageViewport(&pageNow,pageNow,WS_UI_LEFT,0,WS_UI_WIDTH,240);
    
    //-----按钮及标题---------
    drawPageUITxt("切换", "连接", "切换");    
    
    char converted[66] = {0};
    wifiSsidToDisplay(wifiSaveInfo.ssid[wifiMenuListNum],converted,sizeof(converted));
    wrapPageText(savedSsidText,sizeof(savedSsidText),converted,WIFI_SAVED_COLUMNS);
    wrapPageText(savedPasswordText,sizeof(savedPasswordText),wifiSaveInfo.password[wifiMenuListNum],WIFI_SAVED_COLUMNS);
    if(WIFI_SSID_TITLE_Y>=0)
        addChild(0,newChar(12,"网络名称",GRAY,WIFI_TEXT_LEFT,WIFI_SSID_TITLE_Y));
    addChild(0,newChar(12,savedSsidText,0,WIFI_TEXT_LEFT,WIFI_SSID_TOP));
    addChild(0,newChar(12,"密码",GRAY,WIFI_TEXT_LEFT,WIFI_PASSWORD_TITLE_Y));
    addChild(0,newChar(12,savedPasswordText,0,WIFI_TEXT_LEFT,WIFI_PASSWORD_TOP));
    Wifi_connectBtn = newRoundRectDisObj(sleeteBGColor,WIFI_BUTTON_WIDTH,WIFI_BUTTON_HEIGHT,WIFI_CONNECT_LEFT,WIFI_BUTTON_TOP,8,1);
    Wifi_delectBtn = newRoundRectDisObj(defaultBGColor,WIFI_BUTTON_WIDTH,WIFI_BUTTON_HEIGHT,WIFI_DELETE_LEFT,WIFI_BUTTON_TOP,8,1);
    addChild(1,Wifi_connectBtn);
    addChild(1,Wifi_delectBtn);
    addChild(2,newChar(12,"连接",0xFFFF,WIFI_CONNECT_LEFT+WIFI_BUTTON_LABEL_INSET,WIFI_BUTTON_LABEL_TOP));
    addChild(2,newChar(12,"删除",0xFFFF,WIFI_DELETE_LEFT+WIFI_BUTTON_LABEL_INSET,WIFI_BUTTON_LABEL_TOP));
    addWIFIPageKey();
}

void wifiSavedEx(void){
    refreshWifiKeyHints();
    rendWin(0,0,winW,winH,BGColor);
}


//已保存WIFI界面
void wifiSavedInit(void){
    pageNow = PAGE_WIFI_SAVED;
    BGColor = 0xFFFF;
    resetPageView(&wifiSavedEx);
    LCD_SetPageViewport(&pageNow,pageNow,WS_UI_LEFT,0,WS_UI_WIDTH,240);
    wifiMenuListNum = 0;
    //-----按钮及标题---------
    drawPageUITxt("", "", "");    
    
    char *wifiList[wifiSaveInfo.saveNumAll];
    for(u8 i=0;i<wifiSaveInfo.saveNumAll;i++){
        char converted[66] = {0};
        wifiSsidToDisplay(wifiSaveInfo.ssid[i],converted,sizeof(converted));
        wrapPageText(savedListText[i],sizeof(savedListText[i]),converted,WIFI_SAVED_COLUMNS);
        wifiList[i] = savedListText[i];
    }
    wifiMenuListObj = addMenuList((const char* const*)wifiList,12,0xFFFF,0x04BE,wifiSaveInfo.saveNumAll,WIFI_MENU_WIDTH,WIFI_MENU_TOP,WS_UI_FOOTER_TOP,2);
    if (wifiMenuListObj) wifiMenuListObj->parent->x = WIFI_MENU_LEFT;
    //printf("ssid:%s,%s,%s,%d\n",wifiSaveInfo.ssid[0],wifiSaveInfo.ssid[1],wifiSaveInfo.ssid[2],wifiSaveInfo.saveNumAll);
    if(!wifiSaveInfo.saveNumAll) addChild(1,newChar(12,"暂无已保存的Wi-Fi",GRAY,WIFI_TEXT_LEFT,64));
    addWIFIPageKey();
}

void wifiHomeEx(void){
    refreshWifiKeyHints();
    rendWin(0,0,winW,winH,BGColor);
}

char *wifiHomeList[wifiHomeListAll] = {"搜索新的WIFI","查看已保存WIFI"};
void wifiHomePageInit(void){
    pageNow = PAGE_WIFI_HOME;
    BGColor = 0xFFFF;
    resetPageView(&wifiHomeEx);
    LCD_SetPageViewport(&pageNow,pageNow,WS_UI_LEFT,0,WS_UI_WIDTH,240);
    wifiMenuListNum = 0;
    //-----按钮及标题---------
    drawPageUITxt("上一项", "进入", "下一项");    

    wifiMenuListObj = addMenuList((const char* const*)wifiHomeList,12,0xFFFF,0x04BE,wifiHomeListAll,WIFI_MENU_WIDTH,WIFI_MENU_TOP,WS_UI_FOOTER_TOP,2);
    if (wifiMenuListObj) wifiMenuListObj->parent->x = WIFI_MENU_LEFT;

    addWIFIPageKey();
}



//搜索WIFI界面
void scaneEx(void){
    //搜索WIFI完成
    if(refrushScanHd){
        refrushScanHd = 0;
        resetScanView();
        seleteBg->visible = wifiListNum > 0;
    }
    // if(!ScaningHd){
    //     scan_keyState();
    // }
    refreshWifiKeyHints();
    rendWin(0,0,winW,winH,BGColor);
    // batterySp->num = (float)VoltageVal/1000;
    
}





//搜索WIFI界面
void wifiScanPageInit(void){
    pageNow = PAGE_WIFI_SCAN;
    refrushScanHd = 0;
    BGColor = 0xFFFF;
    resetPageView(&scaneEx);
    LCD_SetPageViewport(&pageNow,pageNow,WS_UI_LEFT,0,WS_UI_WIDTH,240);
    
    seleteBg = newRect(0xBDD7,WIFI_SCAN_WIDTH,WS_WIFI_ROW_MIN,WIFI_SCAN_LEFT,0);
    addChild(0,seleteBg);
    seleteBg->visible = 0;

    
    //-----按钮及标题---------
    drawPageUITxt("", "", "");
    //--------------------- 
    
    
    memset(inputTxtTemp,0,sizeof(inputTxtTemp));

    stage.movieclipArr[0].Mask = getRect(WIFI_SCAN_LEFT,WS_WIFI_LIST_TOP,WIFI_SCAN_WIDTH,WS_UI_FOOTER_TOP-WS_WIFI_LIST_TOP);

    showTips("正在搜索中...",0);    
    rendWin(0,0,winW,winH,BGColor); 

    wifi_scan();

    addWIFIPageKey();
    //测试
    #if deviceHd
    sprintf(ap_info[0].ssid,"%s","this WIFI1");
    sprintf(ap_info[1].ssid,"%s","this WIFI2");
    sprintf(ap_info[2].ssid,"%s","this WIFI3");
    sprintf(ap_info[3].ssid,"%s","this WIFI4");
    wifiListNum = 4;
    seleteBg->visible = wifiListNum > 0;
    resetScanView();
    #endif
}


void connectingEx(void){
    if(WifiConnectingStateOld!=WifiConnectingState || wifiDetailOld!=WifiConnectionDetail ||
       wifiFailureOld!=WifiConnectionFailure || wifiSaveOld!=WifiSaveState){
        switch(WifiConnectingState){
            case 1:
            case 2:
                if(WifiConnectionDetail==WIFI_DETAIL_DHCP)showTips("正在获取网络地址...",0);
                else if(WifiConnectionDetail==WIFI_DETAIL_RETRY)showTips("正在重试连接...",0);
                else showTips("正在连接WIFI...",0);
                break;
            case 3:
                if(WifiSaveState==WIFI_SAVE_FULL)showTips("已连接,网络列表已满",0);
                else if(WifiSaveState==WIFI_SAVE_ERROR)showTips("已连接,但保存失败",0);
                else showTips("WIFI连接成功...",0);
                break;
            case 4:
                if(WifiConnectionFailure==WIFI_FAILURE_AUTH)showTips("认证失败,请检查密码",0);
                else if(WifiConnectionFailure==WIFI_FAILURE_NOT_FOUND)showTips("未找到网络,请重试",0);
                else if(WifiConnectionFailure==WIFI_FAILURE_DHCP)showTips("获取地址超时,请重试",0);
                else if(WifiConnectionFailure==WIFI_FAILURE_TIMEOUT)showTips("连接超时,请重试",0);
                else showTips("WIFI连接失败,请重试",0);
                break;
        }
        WifiConnectingStateOld=WifiConnectingState;
        wifiDetailOld=WifiConnectionDetail;
        wifiFailureOld=WifiConnectionFailure;
        wifiSaveOld=WifiSaveState;
    }
    refreshWifiKeyHints();
    rendWin(0,0,winW,winH,BGColor);
}

//连接WIFI界面
static void wifiConnectViewInit(void){
    pageNow = PAGE_WIFI_CONNECT;
    WifiConnectingStateOld = 255;
    wifiDetailOld=wifiFailureOld=wifiSaveOld=255;
    BGColor = 0xFFFF;
    resetPageView(&connectingEx);
    LCD_SetPageViewport(&pageNow,pageNow,WS_UI_LEFT,0,WS_UI_WIDTH,240);

     
    drawPageUITxt("", "", "");
  
    setKeyFun(KM1,KeyDownM1);//拨轮左
    setKeyFun(KM2,KeyDownM2);//拨轮中
    setKeyFun(KM3,KeyDownM3);//拨轮右
    setKeyFun(KML,KeyDownML);//左肩键
    setKeyFun(KMR,NULL);//连接结果由中间键返回首页

    showTips("准备连接WIFI...",0);     

}

#ifdef GVIEW_UI_AUDIT
void wifiAuditConnectPage(void){
    wifiConnectViewInit();
    backPageFlag=1;
}
#endif

void wifiConnectPageInit_save(void){
    wifiConnectViewInit();
    backPageFlag = 1;
}

void wifiConnectPageInit(void){
    wifiConnectViewInit();
    backPageFlag = 0;
    resetWIFIConnectInfo((char*)ap_info[WIFI_seleteNum].ssid,inputTxtTemp);
    wifi_startConnect();
}
