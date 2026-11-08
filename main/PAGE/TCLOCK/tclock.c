#include "LCD/viewport.h"

#include "tclock.h"
#include "../../rom.h"
#include "../../NET/RTC_Internet.h"
#include "../../sound.h"
#include "../../PAGE/HOME/home.h"
u32 localeTimeTemp;
u16 tomatoTimeTemp;//计时变量
u16 tomatoTimesTemp;//计时次数变量
u8 tomatoPageIndexTime;//UI隐藏计时变量
u8 tomatoPageStartFlag;//番茄闹钟启动标志
u8 tomatoPagePaushFlag;//暂停标志位

DisplayObject *TClockTiptObj;
DisplayObject *TNumM;
DisplayObject *TNumM2;
DisplayObject *TNumM1;
DisplayObject *TNumS2;
DisplayObject *TNumS1;




//----------------------设置页面-------------------------
DisplayObject *tomatoSetNumObj1;
DisplayObject *tomatoSetNumObj2;
DisplayObject *tomatoSetNumObj3;
DisplayObject *tomatoSetNumObj4;
DisplayObject *tomatoSetNumObj5;
DisplayObject *tomatoSetBGObj;

char tomatoSetNumTxt1[10];
char tomatoSetNumTxt2[10];
char tomatoSetNumTxt3[10];

u32 tomatoSetNumTemp1;
u32 tomatoSetNumTemp2;
u8 tomatoSetNumTemp3;



static u8 sy = 36;//菜单起始Y
static u8 sh = 26;//菜单间隔
char restLinkTxt[24];//休息铃声文本
char workLinkTxt[24];

u8 tomatoSetSelect;
static u8 tomatoSetEditing;
void moveTClockSetSelect(u8 $num){
    tomatoSetNumObj1->color = 0;
    tomatoSetNumObj2->color = 0;
    tomatoSetNumObj3->color = 0;
    tomatoSetNumObj4->color = 0;
    tomatoSetNumObj5->color = 0;
    Tween(&tomatoSetBGObj->y,.5,(sy-2)+$num*sh,0,fs_Cubic_EaseOut);
    switch ($num)
    {
        case 0:
            tomatoSetNumObj1->color = ORANGE;
            break;
        case 1:
            tomatoSetNumObj2->color = ORANGE;
            break;
        case 2:
            tomatoSetNumObj3->color = ORANGE;
            break;
        case 3:
            tomatoSetNumObj4->color = ORANGE;
            break;
        case 4:
            tomatoSetNumObj5->color = ORANGE;
            break;
        default:
            break;
    }
}
//----------------------主页面-------------------------
char* TClockTipTxt[] = {"休息中","暂停中","专注中"};

void setTClockColor(u16 color){
    TNumM->color = color;
    for(u8 i=0;i<TNumM->childrenNum;++i) getChildAt(TNumM,i)->color=color;
    TNumM2->color = color;
    TNumM1->color = color;
    TNumS2->color = color;
    TNumS1->color = color;
}
u8* getTimeNumPic(u8 num){
    if(num == 0)return (u8*)FN0;
    if(num == 1)return (u8*)FN1;
    if(num == 2)return (u8*)FN2;
    if(num == 3)return (u8*)FN3;
    if(num == 4)return (u8*)FN4;
    if(num == 5)return (u8*)FN5;
    if(num == 6)return (u8*)FN6;
    if(num == 7)return (u8*)FN7;
    if(num == 8)return (u8*)FN8;
    if(num == 9)return (u8*)FN9;
    return (u8*)FN0;
}
static DisplayObject *newWSTimerDigit(const void *pic, short x, short y){
    DisplayObject *obj = getNewMvAction(pic, x, y);
    obj->ac->playing = 0;
    obj->ac->scaleX = 100;
    obj->ac->scaleY = 100;
    return obj;
}
enum {
    TOMATO_TIME_LAYER = 4,
    TOMATO_DIGIT_WIDTH = 48,
    TOMATO_DIGIT_HEIGHT = 72,
    TOMATO_COLON_WIDTH = 14,
    TOMATO_COLON_HEIGHT = 46,
    TOMATO_GAP = 4,
    TOMATO_ROW_WIDTH = 4*TOMATO_DIGIT_WIDTH + TOMATO_COLON_WIDTH + 4*TOMATO_GAP,
    TOMATO_ROW_LEFT = WS_UI_MID - TOMATO_ROW_WIDTH/2,
    TOMATO_ROW_CENTER_Y = 133,
};

// Action sprites use centre coordinates; the colon uses its top-left corner.
static void placeTomatoTimeRow(void){
    if(pageNow!=PAGE_TCLCK) return;
    stage.movieclipArr[TOMATO_TIME_LAYER].x=0;
    stage.movieclipArr[TOMATO_TIME_LAYER].y=0;
    TNumM2->x=TOMATO_ROW_LEFT+TOMATO_DIGIT_WIDTH/2;
    TNumM1->x=TOMATO_ROW_LEFT+TOMATO_DIGIT_WIDTH+TOMATO_GAP+TOMATO_DIGIT_WIDTH/2;
    TNumM->x=TOMATO_ROW_LEFT+2*(TOMATO_DIGIT_WIDTH+TOMATO_GAP);
    TNumS2->x=TNumM->x+TOMATO_COLON_WIDTH+TOMATO_GAP+TOMATO_DIGIT_WIDTH/2;
    TNumS1->x=TNumS2->x+TOMATO_DIGIT_WIDTH+TOMATO_GAP;
    TNumM2->y=TNumM1->y=TNumS2->y=TNumS1->y=TOMATO_ROW_CENTER_Y;
    TNumM->y=TOMATO_ROW_CENTER_Y-TOMATO_COLON_HEIGHT/2;
}

static DisplayObject *newWSCompactDigit(const void *pic, short x, short y){
    DisplayObject *obj = newWSTimerDigit(pic, x, y);
    obj->ac->scaleX = 80;
    obj->ac->scaleY = 80;
    refrushScaleAction(obj->ac);
    return obj;
}
static void setWSTimerDigit(DisplayObject *obj, const void *pic){
    if (obj->ac->p != pic) {
        if (obj->p == obj->ac->p) obj->p = (uint8_t *)pic;
        obj->ac->p = (uint8_t *)pic;
        obj->ac->frameOffset = getPicOffset(0, (uint8_t *)pic);
        obj->ac->scaleXNow = 100;
        obj->ac->scaleYNow = 100;
        refrushScaleAction(obj->ac);
    }
}
void updateTTime(u32 $time){
    u8 sec = $time%60;
    u16 min = $time/60;

    setWSTimerDigit(TNumM2, getTimeNumPic(min%100/10));
    setWSTimerDigit(TNumM1, getTimeNumPic(min%10));
    setWSTimerDigit(TNumS2, getTimeNumPic(sec/10));
    setWSTimerDigit(TNumS1, getTimeNumPic(sec%10));

    if(pageNow == PAGE_CLOCK){
        TNumM->visible = !TNumM->visible;
    }
}

void TClockShowMc0(void){
    stage.movieclipArr[0].visible = 1;
}
void showTClockTitle(void){
    tomatoPageIndexTime = 0;
    stage.movieclipArr[0].visible = 1;
    Tween(&stage.movieclipArr[moveClipMaxNum - 1].y,.5,0,0,fs_Cubic_EaseOut);
}
void hideTClockTitle(void){
    DelayFunction(TClockShowMc0,.5);
    Tween(&stage.movieclipArr[moveClipMaxNum - 1].y,.5,-WS_UI_HEADER_HEIGHT,0,fs_Cubic_EaseIn);
}


static void refreshTomatoKeyHints(void){
    setPageUIKeyText(0,"");
    short headerY=stage.movieclipArr[moveClipMaxNum-1].y;
    setPageUIMiddleText(!tomatoPageStartFlag ? "设置" :
        headerY==0 ? "隐藏菜单" : headerY==-WS_UI_HEADER_HEIGHT ? "显示菜单" : "");
    setPageUIKeyText(2,"");
    setPageUILongKeyText(tomatoPageStartFlag ? "停止" : "返回",
             !tomatoPageStartFlag ? "启动" : tomatoPagePaushFlag ? "继续" : "暂停");
}

void TClocktart(u8 Res){
    printf("TClocktart\n");
    localeTimeTemp = localeTime;
    tomatoPageStartFlag = 1;
    tomatoPagePaushFlag = 0;
    updatePageUIBg(0,0,0);
    setPageUIMiddleText("显隐菜单");
    if(Res){
        tomatoTimeTemp = dataSaveInfo.tomatoTime;
        tomatoTimesTemp = dataSaveInfo.tomatoTimes -1;
    }
    showTClockTitle();
    TClockTiptObj->f = TClockTipTxt[2];
}
void TClockPaush(void){
    tomatoPagePaushFlag = 1;
    updatePageUIBg(0,0,0);
    setPageUIMiddleText("显隐菜单");
    showTClockTitle();
    TClockTiptObj->f = TClockTipTxt[1];
}

void TClockStop(void){
    updatePageUIBg(0,9,0);
    tomatoPageIndexTime = 0;
    stage.movieclipArr[moveClipMaxNum - 1].y = 0;
    stage.movieclipArr[0].visible = 1;
    tomatoPageStartFlag = 0;
    tomatoTimeTemp = dataSaveInfo.tomatoTime;
    tomatoTimesTemp = dataSaveInfo.tomatoTimes -1;
    updateTTime(tomatoTimeTemp);
    TClockTiptObj->f = TClockTipTxt[0];
}

//----------------------设置页面-----------------------
//更新并整理设置数值
void arrangeTClockSetVal(void){

    sprintf(tomatoSetNumTxt1,"%d",tomatoSetNumTemp1);
    sprintf(tomatoSetNumTxt2,"%d",tomatoSetNumTemp2);
    if(tomatoSetNumTemp3>0){
        sprintf(tomatoSetNumTxt3,"%d",tomatoSetNumTemp3);
    }else{
        sprintf(tomatoSetNumTxt3,"无限");
    }
    formatSoundTrackTitle(restLinkTxt,sizeof(restLinkTxt),dataSaveInfo.tomatoRestRing,14);
    formatSoundTrackTitle(workLinkTxt,sizeof(workLinkTxt),dataSaveInfo.tomatoWorkRing,14);

    middleTxtX(tomatoSetNumObj1,16);
    middleTxtX(tomatoSetNumObj2,16);
    middleTxtX(tomatoSetNumObj3,16);
}
//------------------------------按钮-----------------------------
static void refreshTomatoSetKeyHints(void){
    setPageUIKeyText(0,tomatoSetEditing ? "减少" : "上一项");
    setPageUIMiddleText(tomatoSetEditing ? "完成" : "编辑");
    setPageUIKeyText(2,tomatoSetEditing ? "增加" : "下一项");
}

static u8 stepTomatoSettingValue(u8 value,u8 minimum,u8 maximum,int direction){
    if(direction<0) return value>minimum ? value-1 : maximum;
    return value<maximum ? value+1 : minimum;
}

static void adjustTomatoSetting(int direction){
    switch(tomatoSetSelect){
        case 0: tomatoSetNumTemp1=stepTomatoSettingValue(tomatoSetNumTemp1,1,99,direction); break;
        case 1: tomatoSetNumTemp2=stepTomatoSettingValue(tomatoSetNumTemp2,1,99,direction); break;
        case 2: tomatoSetNumTemp3=stepTomatoSettingValue(tomatoSetNumTemp3,0,99,direction); break;
        case 3: dataSaveInfo.tomatoRestRing=stepTomatoSettingValue(dataSaveInfo.tomatoRestRing,0,9,direction); break;
        case 4: dataSaveInfo.tomatoWorkRing=stepTomatoSettingValue(dataSaveInfo.tomatoWorkRing,0,9,direction); break;
    }
    arrangeTClockSetVal();
}

static void KeyDownM3(void){
    if(pageNow!=PAGE_TCLCK_SET) return;
    if(tomatoSetEditing) adjustTomatoSetting(-1);
    else{
        tomatoSetSelect=(tomatoSetSelect+4)%5;
        moveTClockSetSelect(tomatoSetSelect);
        refreshTomatoSetKeyHints();
    }
}

static void KeyDownM2(void){
    if(pageNow==PAGE_TCLCK){
        if(tomatoPageStartFlag){
            if(stage.movieclipArr[moveClipMaxNum-1].y==-WS_UI_HEADER_HEIGHT)
                showTClockTitle();
            else if(stage.movieclipArr[moveClipMaxNum-1].y==0)
                hideTClockTitle();
        }else tomatoPageSet();
    }else if(pageNow==PAGE_TCLCK_SET){
        tomatoSetEditing=!tomatoSetEditing;
        refreshTomatoSetKeyHints();
    }
}

static void KeyDownM1(void){
    if(pageNow!=PAGE_TCLCK_SET) return;
    if(tomatoSetEditing) adjustTomatoSetting(1);
    else{
        tomatoSetSelect=(tomatoSetSelect+1)%5;
        moveTClockSetSelect(tomatoSetSelect);
        refreshTomatoSetKeyHints();
    }
}

static void KeyDownML(void){
    //左肩
    switch (pageNow)
    {
    case PAGE_TCLCK:
        if(tomatoPageStartFlag){
            TClockStop();
        }else{
            renderMenuInit();
        }
        break;
    case PAGE_TCLCK_SET:
        dataSaveInfo.tomatoTime = tomatoSetNumTemp1*60;
        dataSaveInfo.tomatoTimeRelax = tomatoSetNumTemp2*60;
        dataSaveInfo.tomatoTimes = tomatoSetNumTemp3;
        writeData();
        tomatoPageInit();
        break;
    default:
        break;
    }
}
static void KeyDownMR(void){
   //右肩
   switch (pageNow)
   {
    case PAGE_TCLCK:
        if(!tomatoPageStartFlag){   //未运行
            //重新开始         
            TClocktart(1);
        }else if(tomatoPagePaushFlag){   //暂停中
            TClocktart(0);//继续
        }else{ //运行中
            //暂停
            TClockPaush();
        }
    break;
    case PAGE_TCLCK_SET:
        KeyDownM1();
        break;
   default:
    break;
   }

}





//----------------------设置页面-------------------------
void tomatoSetEx(void){
    rendWin(0,0,winW,winH,BGColor);
    if(tomatoSetEditing && tomatoSetSelect<=1){
        if(KEY1HOLD) KeyDownM1();
        else if(KEY3HOLD) KeyDownM3();
    }
}

void tomatoPageSet(void){
    pageNow = PAGE_TCLCK_SET;
    BGColor = 0xFFFF;
    resetPageView(&tomatoSetEx);

    tomatoSetSelect = 0;
    tomatoSetEditing = 0;
    tomatoSetNumTemp1 = dataSaveInfo.tomatoTime/60;
    tomatoSetNumTemp2 = dataSaveInfo.tomatoTimeRelax/60;
    tomatoSetNumTemp3 = dataSaveInfo.tomatoTimes;

    drawPageUITxt("上一项","编辑","下一项");

    tomatoSetBGObj = newRoundRectDisObj(0x9efc,164,20,(320-164)/2,44,8,1);
    addChild(0,tomatoSetBGObj);

    addChild(0,newChar(12,"专注",0,80,sy));
    addChild(0,newChar(12,"分钟",0,207,sy));
    tomatoSetNumObj1 = newChar(12,tomatoSetNumTxt1,0,164,sy);
    addChild(0,tomatoSetNumObj1);

    addChild(0,newChar(12,"休息",0,80,sy+sh*1));
    addChild(0,newChar(12,"分钟",0,207,sy+sh*1));
    tomatoSetNumObj2 = newChar(12,tomatoSetNumTxt2,0,164,sy+sh*1);
    addChild(0,tomatoSetNumObj2);

    addChild(0,newChar(12,"循环",0,80,sy+sh*2));
    addChild(0,newChar(12,"次",0,207,sy+sh*2));
    tomatoSetNumObj3 = newChar(12,tomatoSetNumTxt3,0,164,sy+sh*2);
    addChild(0,tomatoSetNumObj3);

    addChild(0,newChar(12,"休息铃声",0,80,sy+sh*3));
    tomatoSetNumObj4 = newChar(12,restLinkTxt,0,164,sy+sh*3);
    addChild(0,tomatoSetNumObj4);
    addChild(0,newChar(12,"专注铃声",0,80,sy+sh*4));
    tomatoSetNumObj5 = newChar(12,workLinkTxt,0,164,sy+sh*4);
    addChild(0,tomatoSetNumObj5);


    arrangeTClockSetVal();

    moveTClockSetSelect(tomatoSetSelect);
    refreshTomatoSetKeyHints();

    setKeyFun(KM1,KeyDownM1);//拨轮左
    setKeyFun(KM2,KeyDownM2);//拨轮中
    setKeyFun(KM3,KeyDownM3);//拨轮右
    setKeyFun(KML,KeyDownML);//左肩键
    setKeyFun(KMR,KeyDownMR);//右肩键
}

//----------------------主页------------------------------

void tomatoEx(void){
    placeTomatoTimeRow();
    refreshTomatoKeyHints();
    rendWin(0,0,winW,winH,BGColor);

    //----------首页UI逻辑--------------
    if(tomatoPageIndexTime > 0){
        tomatoPageIndexTime--;
        if(tomatoPageIndexTime==1){
            hideTClockTitle();
        }
    }

    //---------倒计时----------
    if(tomatoPageStartFlag&&!tomatoPagePaushFlag){
        //-----------番茄闹钟进行中-------------
        switch(tomatoPageStartFlag){
            case 1:
            //-------专注中----------
            if(localeTime != localeTimeTemp){
                localeTimeTemp = localeTime;
                tomatoTimeTemp --;
                updateTTime(tomatoTimeTemp);
                setTClockColor(0);
                //计时结束
                if(!tomatoTimeTemp){
                    alarmClockNow = 201;//专注结束铃声标注(铃声根据这个判断)
                    aClockTimes = 2;//响铃1次
                    //休息时间
                    if(!dataSaveInfo.tomatoTimes || tomatoTimesTemp>0){
                        if(tomatoTimesTemp)tomatoTimesTemp --;
                        tomatoPageStartFlag = 2;
                        tomatoTimeTemp = dataSaveInfo.tomatoTimeRelax;
                        TClockTiptObj->f = TClockTipTxt[0];
                        
                        printf("fff%d",tomatoTimesTemp);
                    }else{
                        //结束
                        TClockStop();
                    }
                }
            }
            break;
            case 2:
            //-------休息中----------
            if(localeTime != localeTimeTemp){
                localeTimeTemp = localeTime;
                tomatoTimeTemp --;
                updateTTime(tomatoTimeTemp);
                setTClockColor(GRAY);
                //计时结束
                if(!tomatoTimeTemp){
                    alarmClockNow = 202;//休息结束铃声标注
                    aClockTimes = 2;//响铃1次
                    tomatoPageStartFlag = 1;
                    TClockTiptObj->f = TClockTipTxt[2];
                    tomatoTimeTemp = dataSaveInfo.tomatoTime;
                    
                }
            }
            break;
        }
    }else{
        //-----------番茄闹钟结束-------------
    }
}


//番茄闹钟界面
void tomatoPageInit(void){
    pageNow = PAGE_TCLCK;
    BGColor = 0xFFFF;
    resetPageView(&tomatoEx);

    tomatoPageStartFlag = 0;
    tomatoPagePaushFlag = 0;

    // addChild(0,newRoundRectDisObj(0xc638,90,24,160-45,2,9,1));

    addChild(0,newNativePic(&tmtPic,48,28));
    //addChild(0,newChar(16,"番",RED,160-32,6));
    //addChild(0,newChar(16,"茄",GREEN,160-16,6));

    // The digit sprites and colon have different anchor conventions.
    // placeTomatoTimeRow keeps their visible bounds centred as one MM:SS row.
    TNumM2 = newWSCompactDigit(&FN0,0,0);
    TNumM1 = newWSCompactDigit(&FN0,0,0);
    TNumS2 = newWSCompactDigit(&FN0,0,0);
    TNumS1 = newWSCompactDigit(&FN0,0,0);
    TNumM = newPicSingle(&FNM,0,0,0);
    placeTomatoTimeRow();

     //------------按钮底和文字---------
    drawPageUIBg(0,9,0);
    stage.movieclipArr[moveClipMaxNum - 1].y = -WS_UI_HEADER_HEIGHT;

    addChild(TOMATO_TIME_LAYER,TNumM2);
    addChild(TOMATO_TIME_LAYER,TNumM1);
    addChild(TOMATO_TIME_LAYER,TNumM);
    addChild(TOMATO_TIME_LAYER,TNumS2);
    addChild(TOMATO_TIME_LAYER,TNumS1);
    TNumM->visible = 1;

    TClockTiptObj = newChar(12,"休息中",GRAY,78,28);
    addChild(0,TClockTiptObj);


    TClockStop();


    setKeyFun(KM1,KeyDownM1);//拨轮左
    setKeyFun(KM2,KeyDownM2);//拨轮中
    setKeyFun(KM3,KeyDownM3);//拨轮右
    setKeyFun(KML,KeyDownML);//左肩键
    setKeyFun(KMR,KeyDownMR);//右肩键
}

//-------------时钟界面----------------

void CLOCK_KeyDownM1(void){
    weatherInit();
}
void CLOCK_KeyDownM3(void){
    renderCalendarInit();
}
u32 clockOneSecTime = 0;
void clockEx(void){
    refreshWeatherDisplay();
    rendWin(0,0,winW,winH,BGColor);
}

//纯数字时钟模式
void clockPageInit(void){
    homeState = 2;
    pageNow = PAGE_CLOCK;
    BGColor = 0;
    resetPageView(&clockEx);
    LCD_SetPageViewport(&pageNow, PAGE_CLOCK, 40, 0, 240, 240);

    // Scale this clock's 60x90 digits to 48x72, leaving room for
    // the original 14x46 pixel colon between the hour and minute pairs.
    TNumM2 = newWSCompactDigit(&FN0,WS_UI_MID-84,124);
    TNumM1 = newWSCompactDigit(&FN0,WS_UI_MID-34,124);
    TNumS2 = newWSCompactDigit(&FN0,WS_UI_MID+34,124);
    TNumS1 = newWSCompactDigit(&FN0,WS_UI_MID+84,124);
    TNumM = newPicSingle(&FNM,0xFFFF,WS_UI_MID-7,101);

    //------------按钮底和文字---------

    addChild(4,TNumM2);
    addChild(4,TNumM1);
    addChild(4,TNumM);
    addChild(4,TNumS2);
    addChild(4,TNumS1);

    setTClockColor(0xFFFF);

    addTompBottomTxt();

    update_rtc_Txt();
    

    setKeyFun(KM1,mainViewOpenGarden);
    setKeyFun(KM3,mainViewOpenShop);
    setKeyFun(KML,CLOCK_KeyDownM3);
    setKeyFun(KMR,CLOCK_KeyDownM1);
    setKeyFun(KMU1,CLOCK_KeyDownM1);
    setKeyFun(KMU3,CLOCK_KeyDownM3);
}

//------------计时界面---------------------
u16 CCTimeTemp;//计时变量
u8 CCTimeMin = 0;
u8 CCTimeSec = 0;
u8 CCTimeHd = 0;//0停止 1开始 2暂停
u8 CCMode = 0;//计时模式 0 顺序计时 1倒计时

#define TIMER_SOUND_SET_COUNT 2
#define TIMER_SOUND_SET_TOP 74
#define TIMER_SOUND_SET_PITCH 52
static DisplayObject *timerSoundBgObj;
static DisplayObject *timerSoundValues[TIMER_SOUND_SET_COUNT];
static u8 timerSoundSelect;
static u8 timerSoundEditing;
static u8 timerSoundDirty;
static char timerRingText[24];
static char timerRepeatText[12];

void saveTimerSoundSettingsIfDirty(void){
    clearDelayFunction(&saveTimerSoundSettingsIfDirty);
    if(timerSoundDirty){
        timerSoundDirty=0;
        writeData();
    }
}
static void queueTimerSoundSave(void){
    timerSoundDirty=1;
    clearDelayFunction(&saveTimerSoundSettingsIfDirty);
    DelayFunction(&saveTimerSoundSettingsIfDirty,2.0f);
}
static void updateTimerSoundValues(void){
    formatSoundTrackTitle(timerRingText,sizeof(timerRingText),
                          dataSaveInfo.clockCountRing,8);
    if(dataSaveInfo.clockCountTimes)
        snprintf(timerRepeatText,sizeof(timerRepeatText),"%u次",dataSaveInfo.clockCountTimes);
    else snprintf(timerRepeatText,sizeof(timerRepeatText),"无限");
}
static void clockCountSettingsInit(void);

void CCstop(void){
    CCTimeHd = 0;
    drawPageUITxt("分钟+1","启动","秒数+1");//启动 清零
    CCTimeTemp = CCTimeMin*60+CCTimeSec;
    updateTTime(CCTimeTemp);
}

void CC_KeyDownM3(void){
    if(!CCTimeHd){
        if(CCTimeMin<99){
            CCTimeMin ++;
        }else{
            CCTimeMin = 0;
        }
        CCTimeTemp = CCTimeMin*60+CCTimeSec;
        updateTTime(CCTimeTemp);
    }
}

void CC_KeyDownM2(void){
    if(!CCTimeHd){
        if(!CCTimeMin&&!CCTimeSec){
            CCMode = 0;
        }else{
            CCMode = 1;
        }
        CCTimeHd = 1;
        localeTimeTemp = localeTime;
        drawPageUITxt("","暂停","");//暂停 停止
    }else if(CCTimeHd == 1){
        CCTimeHd = 2;
        drawPageUITxt("","继续","");//继续 停止
    }else if(CCTimeHd == 2){
        localeTimeTemp = localeTime;
        CCTimeHd = 1;
        drawPageUITxt("","暂停","");//继续 停止
    }
}

void CC_KeyDownM1(void){
    if(!CCTimeHd){
        if(CCTimeSec<59){
            CCTimeSec ++;
        }else{
            CCTimeSec = 0;
        }
        CCTimeTemp = CCTimeMin*60+CCTimeSec;
        updateTTime(CCTimeTemp);
    }
}
void CC_KeyDownML(void){
    renderMenuInit();
}
void CC_KeyDownMR(void){
    if(CCTimeHd){
        //停止
        CCstop();
    }else if(CCTimeTemp || CCTimeMin || CCTimeSec){
        //清零
        CCTimeTemp = 0;
        CCTimeMin = 0;
        CCTimeSec = 0;
        updateTTime(CCTimeTemp);
    }else{
        clockCountSettingsInit();
    }
}

static void moveTimerSoundSelect(void){
    for(u8 i=0;i<TIMER_SOUND_SET_COUNT;++i)
        timerSoundValues[i]->color=i==timerSoundSelect ? ORANGE : 0;
    Tween(&timerSoundBgObj->y,.5,
          TIMER_SOUND_SET_TOP-2+timerSoundSelect*TIMER_SOUND_SET_PITCH,
          0,fs_Cubic_EaseOut);
}
static void updateTimerSoundKeyHints(void){
    setPageUIKeyText(0,timerSoundEditing ? "上一首" : "上一项");
    setPageUIMiddleText(timerSoundEditing ? "完成" : "编辑");
    setPageUIKeyText(2,timerSoundEditing ? "下一首" : "下一项");
    if(timerSoundEditing && timerSoundSelect==1){
        setPageUIKeyText(0,"减少");
        setPageUIKeyText(2,"增加");
    }
}
static void adjustTimerSound(int direction){
    if(timerSoundSelect==0)
        dataSaveInfo.clockCountRing=(dataSaveInfo.clockCountRing+
                                     SOUND_TRACK_COUNT+direction)%SOUND_TRACK_COUNT;
    else
        dataSaveInfo.clockCountTimes=(dataSaveInfo.clockCountTimes+100+direction)%100;
    updateTimerSoundValues();
    queueTimerSoundSave();
}
static void timerSoundPrevious(void){
    if(timerSoundEditing){ adjustTimerSound(-1); return; }
    timerSoundSelect=(timerSoundSelect+TIMER_SOUND_SET_COUNT-1)%TIMER_SOUND_SET_COUNT;
    moveTimerSoundSelect();
    updateTimerSoundKeyHints();
}
static void timerSoundNext(void){
    if(timerSoundEditing){ adjustTimerSound(1); return; }
    timerSoundSelect=(timerSoundSelect+1)%TIMER_SOUND_SET_COUNT;
    moveTimerSoundSelect();
    updateTimerSoundKeyHints();
}
static void timerSoundConfirm(void){
    timerSoundEditing=!timerSoundEditing;
    updateTimerSoundKeyHints();
}
static void leaveTimerSoundSettings(void){
    saveTimerSoundSettingsIfDirty();
    clockCountPageInit();
}
static void timerSoundSettingsEx(void){
    updateTimerSoundKeyHints();
    rendWin(0,0,winW,winH,BGColor);
}
static void clockCountSettingsInit(void){
    pageNow=PAGE_CLOCKCOUNT_SET;
    BGColor=0xFFFF;
    resetPageView(&timerSoundSettingsEx);
    timerSoundSelect=0;
    timerSoundEditing=0;
    timerSoundDirty=0;
    updateTimerSoundValues();
    timerSoundBgObj=newRoundRectDisObj(0x9efc,164,20,78,TIMER_SOUND_SET_TOP,8,1);
    addChild(0,timerSoundBgObj);
    drawPageUITxt("上一项","编辑","下一项");
    const char *labels[TIMER_SOUND_SET_COUNT]={"计时器铃声","计时器循环"};
    char *values[TIMER_SOUND_SET_COUNT]={timerRingText,timerRepeatText};
    for(u8 i=0;i<TIMER_SOUND_SET_COUNT;++i){
        short y=TIMER_SOUND_SET_TOP+i*TIMER_SOUND_SET_PITCH;
        addChild(0,newChar(12,labels[i],0,80,y));
        timerSoundValues[i]=newChar(12,values[i],0,177,y);
        addChild(0,timerSoundValues[i]);
    }
    moveTimerSoundSelect();
    updateTimerSoundKeyHints();
    setKeyFun(KML,&leaveTimerSoundSettings);
    setKeyFun(KMR,&timerSoundNext);
    setKeyFun(KM1,&timerSoundNext);
    setKeyFun(KM2,&timerSoundConfirm);
    setKeyFun(KM3,&timerSoundPrevious);
}

void clockCountEx(void){
    setPageUIKeyText(0,CCTimeHd ? "" : "分钟+1");
    setPageUIMiddleText(!CCTimeHd ? "启动" : CCTimeHd==1 ? "暂停" : "继续");
    setPageUIKeyText(2,CCTimeHd ? "" : "秒数+1");
    setPageUILongKeyText("返回",CCTimeHd ? "停止" :
        CCTimeTemp || CCTimeMin || CCTimeSec ? "清零" : "声音设置");
    rendWin(0,0,winW,winH,BGColor);
    
    if(!CCTimeHd){
        if(KEY1HOLD){
            CC_KeyDownM1();
        }else if(KEY3HOLD){
            CC_KeyDownM3();
        }
    }else if(CCTimeHd == 1){
        if(CCMode){
            //倒计时
            if(CCTimeTemp){
                if(localeTime != localeTimeTemp){
                        localeTimeTemp = localeTime;
                        CCTimeTemp --;
                        updateTTime(CCTimeTemp);
                }
            }else{
                //停止
                CCstop();
                //播放闹铃
                alarmClockNow = 101;//专注结束铃声标注(铃声根据这个判断)
                aClockTimes = dataSaveInfo.clockCountTimes?dataSaveInfo.clockCountTimes+1:101;//响铃5次
                // aClockTimes = 2;//响铃1次
            }
        }else{
            //顺计时
            if(localeTime != localeTimeTemp){
                localeTimeTemp = localeTime;
                CCTimeTemp ++;
                updateTTime(CCTimeTemp);
            }
        }
    }
}

//计时界面
void clockCountPageInit(void){
    pageNow = PAGE_CLOCKCOUNT;
    BGColor = 0xFFFF;
    resetPageView(&clockCountEx);
    CCTimeHd = 0;//停止计时
    drawPageUITxt("分钟+1","启动","秒数+1");//启动 清零
    
    TNumM2 = newWSTimerDigit(&FN0,146,78);
    TNumM1 = newWSTimerDigit(&FN0,208,78);
    TNumM = newWSTimerDigit(&FNM,157,62);
    TNumS2 = newWSTimerDigit(&FN0,146,176);
    TNumS1 = newWSTimerDigit(&FN0,208,176);

    updateTTime(CCTimeTemp);
    //------------按钮底和文字---------

    addChild(4,TNumM2);
    addChild(4,TNumM1);
    addChild(4,TNumM);
    addChild(4,TNumS2);
    addChild(4,TNumS1);
    TNumM->visible = 0;
    addChild(1,newChar(12,"分",GRAY,70,72));
    addChild(1,newChar(12,"秒",GRAY,70,170));

    setTClockColor(0);


    setKeyFun(KM1,CC_KeyDownM1);//拨轮左
    setKeyFun(KM2,CC_KeyDownM2);//拨轮中
    setKeyFun(KM3,CC_KeyDownM3);//拨轮右
    setKeyFun(KML,CC_KeyDownML);//左肩键
    setKeyFun(KMR,CC_KeyDownMR);//右肩键
}