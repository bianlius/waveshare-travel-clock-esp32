#include "clockSetting.h"
#include "../../rom.h"
#include "../../LCD/backLight.h"

void clockSettingTimeInit(u8 $type);
u8 clockTimeHourTemp[5];//设置界面小时
u8 clockTimeMinTemp[5];//设置界面分
char clockListtime[5][2][4];
DisplayObject *clockHourNumObj;//设置界面的显示对象
DisplayObject *clockMinNumObj;//设置界面的显示对象
DisplayObject *clockSWBtn[5];//开启按钮
DisplayObject *clockHourBtn[5];//开启按钮
DisplayObject *clockMinBtn[5];//开启按钮
char clockSWTxt[5][24];
DisplayObject *clockSeleteBg;
DisplayObject *csTipsObj;//提示
u8 ClockNum;//闹钟总数
u8 clockSeleteNow;//当前选择的闹钟
u8 clockEditHd;//编辑状态

enum { CLOCK_ROWS_TOP=28, ALARM_ROW_HEIGHT=38,
       LIGHT_ROW_PITCH=35, LIGHT_ROW_HEIGHT=32, CLOCK_TEXT_HEIGHT=16 };

static short clockSettingRowTop(u8 row){
    short pitch=pageNow==PAGE_CLOCK_SET ? ALARM_ROW_HEIGHT : LIGHT_ROW_PITCH;
    return CLOCK_ROWS_TOP+row*pitch;
}

static void setClockRingtoneText(u8 row,u8 number){
    if(number>=1 && number<=SOUND_TRACK_COUNT)
        formatSoundTrackTitle(clockSWTxt[row],sizeof(clockSWTxt[row]),number-1,12);
    else
        snprintf(clockSWTxt[row],sizeof(clockSWTxt[row]),"关闭");
}


//--------闹钟设置界面----------
void setClockTime(char *txt,u8 num){
    sprintf(txt,"%02d",num);
}

u8 getClockTimeHour(u16 $time){
    u16 tTime =  ($time>>1)&0x3FFF;
    return tTime/60;
}

u8 getClockTimeMin(u16 $time){
    u16 tTime =  ($time>>1)&0x3FFF;
    return tTime%60;
}


void refrushClockSettingTime(void){
    setClockTime(clockListtime[clockSeleteNow][0],clockTimeHourTemp[clockSeleteNow]);
    setClockTime(clockListtime[clockSeleteNow][1],clockTimeMinTemp[clockSeleteNow]);
}
//-------------------------




void setClockBtnColor(u8 $num){
    clockSWBtn[$num]->color = 0;//开启按钮
    clockHourBtn[$num]->color = 0;//开启按钮
    clockMinBtn[$num]->color = 0;//开启按钮

    if(pageNow == PAGE_CLOCK_SET){
        for(u8 i=0;i<7;i++){
            getChildAt(&stage.movieclipArr[1].disObj,i+$num*7)->color = 0xFFFF;
        }
        if(clockEditHd > 3){
            //闹钟星期设置
            getChildAt(&stage.movieclipArr[1].disObj,clockEditHd-4+$num*7)->color = GREEN;
        }
    }
    
    if(clockEditHd == 3)clockSWBtn[$num]->color = ORANGE;
    else if(clockEditHd == 1)clockHourBtn[$num]->color = ORANGE;
    else if(clockEditHd == 2)clockMinBtn[$num]->color = ORANGE;
}

//按键操作回调函数
static void KeyDownM3(void){
    if(clockEditHd){
        if(pageNow == PAGE_CLOCK_SET){
            //编辑状态
            switch (clockEditHd)
            {
                case 1:
                    /* 时设置 */
                    if(clockTimeHourTemp[clockSeleteNow]>0)clockTimeHourTemp[clockSeleteNow]--;
                    else clockTimeHourTemp[clockSeleteNow] = 23;
                    refrushClockSettingTime(); 
                    break;
                case 2:
                    /* 分设置 */
                    if(clockTimeMinTemp[clockSeleteNow]>0)clockTimeMinTemp[clockSeleteNow]--;
                    else clockTimeMinTemp[clockSeleteNow] = 59;
                    refrushClockSettingTime(); 
                    break;
                case 3:{
                    /* 铃声设置 */
                    u8 clockNum = dataSaveInfo.alarmClock[clockSeleteNow]&0x0F;
                    if(clockNum>0)clockNum --;
                    else clockNum = 10;
                    setClockRingtoneText(clockSeleteNow,clockNum);
                    dataSaveInfo.alarmClock[clockSeleteNow] = (dataSaveInfo.alarmClock[clockSeleteNow] & 0xFFF0) | clockNum;
                    }
                    break;
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:{
                    DisplayObject *obj = getChildAt(&stage.movieclipArr[1].disObj,clockEditHd-4+clockSeleteNow*7);
                    u8 hd = (dataSaveInfo.alarmClockWeek[clockSeleteNow] >> (clockEditHd-4)) & 0x01;
                    if(hd){
                        getChildAt(obj,0)->color = GRAY;
                        dataSaveInfo.alarmClockWeek[clockSeleteNow] &= ~(1<<(clockEditHd-4));
                    }else{
                        getChildAt(obj,0)->color = ORANGE;
                        dataSaveInfo.alarmClockWeek[clockSeleteNow] |= 1<<(clockEditHd-4);
                    }
                    }
                    break;
                default:
                    break;
            }
        }else{
             //编辑状态
            switch (clockEditHd)
            {
                case 1:
                    /* 时设置 */
                    if(clockTimeHourTemp[clockSeleteNow]>0)clockTimeHourTemp[clockSeleteNow]--;
                    else clockTimeHourTemp[clockSeleteNow] = 23;
                    refrushClockSettingTime(); 
                    break;
                case 2:
                    /* 时设置 */
                    if(clockTimeMinTemp[clockSeleteNow]>0)clockTimeMinTemp[clockSeleteNow]--;
                    else clockTimeMinTemp[clockSeleteNow] = 59;
                    refrushClockSettingTime();
                    break;
                case 3:{
                    u8 lightNum = dataSaveInfo.brightnessInfo[clockSeleteNow]&0x0F;
                    if(lightNum>1)lightNum --;
                    else lightNum = 10;
                    sprintf(clockSWTxt[clockSeleteNow],"亮度:%d",lightNum);
                    dataSaveInfo.brightnessInfo[clockSeleteNow] = (dataSaveInfo.brightnessInfo[clockSeleteNow] & 0xFFF0) | lightNum;
                    setBackLightVal(lightNum);
                    }
                    break;
                    
                default:
                    break;
            }
        }
    }else{
        if(clockSeleteNow>0){
            clockSeleteNow --;            
        }else{
            //5个闹钟
            clockSeleteNow = 4;
        }
        Tween(&clockSeleteBg->y,.5,clockSettingRowTop(clockSeleteNow),0,fs_Cubic_EaseOut);        
    }
}
static void KeyDownM2(void){
    if(!clockEditHd){
        clockEditHd = 1;
        clockSeleteBg->visible = false;
        if(pageNow != PAGE_CLOCK_SET)
        setBackLightVal(dataSaveInfo.brightnessInfo[clockSeleteNow]&0x0F);
    }else{
        clockEditHd ++;
        if(clockEditHd > (pageNow == PAGE_CLOCK_SET?10:3)){
            clockEditHd = 0;
            clockSeleteBg->visible = true;
        }
    }
    const char *middleAction = !clockEditHd ? "编辑" :
        clockEditHd == (pageNow == PAGE_CLOCK_SET ? 10 : 3) ? "完成" : "下一项";
    drawPageUITxt(clockEditHd>=4 ? "切换" : clockEditHd ? "减少" : "上一项", middleAction,
                  clockEditHd>=4 ? "切换" : clockEditHd ? "增加" : "下一项");
    setClockBtnColor(clockSeleteNow);
}

static void KeyDownM1(void){
    if(clockEditHd){
        //编辑状态
        if(pageNow == PAGE_CLOCK_SET){
            switch (clockEditHd)
            {
                case 1:
                    /* 时设置 */
                    if(clockTimeHourTemp[clockSeleteNow]<23)clockTimeHourTemp[clockSeleteNow]++;
                    else clockTimeHourTemp[clockSeleteNow] = 0;
                    refrushClockSettingTime(); 
                    break;
                case 2:
                    /* 时设置 */
                    if(clockTimeMinTemp[clockSeleteNow]<59)clockTimeMinTemp[clockSeleteNow]++;
                    else clockTimeMinTemp[clockSeleteNow] = 0;
                    refrushClockSettingTime(); 
                    break;
                case 3:{
                    u8 clockNum = dataSaveInfo.alarmClock[clockSeleteNow]&0x0F;
                    if(clockNum<10)clockNum ++;
                    else clockNum = 0;
                    setClockRingtoneText(clockSeleteNow,clockNum);
                    dataSaveInfo.alarmClock[clockSeleteNow] = (dataSaveInfo.alarmClock[clockSeleteNow] & 0xFFF0) | clockNum;
                    }
                    break;
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:{
                    DisplayObject *obj = getChildAt(&stage.movieclipArr[1].disObj,clockEditHd-4+clockSeleteNow*7);
                    u8 hd = (dataSaveInfo.alarmClockWeek[clockSeleteNow] >> (clockEditHd-4)) & 0x01;
                    if(hd){
                        getChildAt(obj,0)->color = GRAY;
                        dataSaveInfo.alarmClockWeek[clockSeleteNow] &= ~(1<<(clockEditHd-4));
                    }else{
                        getChildAt(obj,0)->color = ORANGE;
                        dataSaveInfo.alarmClockWeek[clockSeleteNow] |= 1<<(clockEditHd-4);
                    }
                    }
                    break;
                default:
                    break;
            }
        }else{
            switch (clockEditHd)
            {
                case 1:
                    /* 时设置 */
                    if(clockTimeHourTemp[clockSeleteNow]<23)clockTimeHourTemp[clockSeleteNow]++;
                    else clockTimeHourTemp[clockSeleteNow] = 0;
                    refrushClockSettingTime(); 
                    break;
                case 2:
                    /* 时设置 */
                    if(clockTimeMinTemp[clockSeleteNow]<59)clockTimeMinTemp[clockSeleteNow]++;
                    else clockTimeMinTemp[clockSeleteNow] = 0;
                    refrushClockSettingTime(); 
                    break;
                case 3:{
                    u8 lightNum = dataSaveInfo.brightnessInfo[clockSeleteNow]&0x0F;
                    if(lightNum<10)lightNum ++;
                    else lightNum = 1;
                    sprintf(clockSWTxt[clockSeleteNow],"亮度:%d",lightNum);
                    dataSaveInfo.brightnessInfo[clockSeleteNow] = (dataSaveInfo.brightnessInfo[clockSeleteNow] & 0xFFF0) | lightNum;   
                    setBackLightVal(lightNum);                 
                    }
                    break;
                default:
                    break;
            }
        }
    }else{
        if(clockSeleteNow < 4){
            clockSeleteNow ++;
        }else{
            clockSeleteNow = 0;
        }
        Tween(&clockSeleteBg->y,.5,clockSettingRowTop(clockSeleteNow),0,fs_Cubic_EaseOut);
    }
}

// Both alarm and brightness pages use BOOT/PLUS to move or adjust,
// PWR to enter the next field, and a long BOOT press to save and leave.
void leaveClockSettings(void){
    if(pageNow == PAGE_CLOCK_SET){
        for(u8 i=0;i<5;i++){
            dataSaveInfo.alarmClock[i] =
                ((clockTimeHourTemp[i]*60+clockTimeMinTemp[i])<<4) |
                (dataSaveInfo.alarmClock[i] & 0x0F);
        }
    }else if(pageNow == PAGE_BGLIGHT_SCHEDULE){
        for(u8 i=0;i<5;i++){
            dataSaveInfo.brightnessInfo[i] =
                ((clockTimeHourTemp[i]*60+clockTimeMinTemp[i])<<4) |
                (dataSaveInfo.brightnessInfo[i] & 0x0F);
        }
        resetBackLightVal();
    }
    writeData();
    renderMenuInit();
}

//-----------------------------------------------------


static const char *numStr[5] = {"闹钟1","闹钟2","闹钟3","闹钟4","闹钟5"};
static const char *numLStr[7] = {"1","2","3","4","5","6","7"};
static const char *numCStr[7] = {"一","二","三","四","五","六","日"};
static void addRowNumber(u8 num,short y){
    DisplayObject *number=newChar(12,numLStr[num],0xFFFF,60,y);
    addChildToObj(number,newRoundRectDisObj(ORANGE,16,18,-4,-1,3,1));
    addChild(0,number);
}

void addNewClock(u16 $time,u8 $num){
    u8 swHd = $time&0x0F;
    u16 tTime =  $time>>4;
    u8 tHour = tTime/60;
    u8 tMin = tTime%60;

    clockTimeHourTemp[$num] = tHour;//设置界面小时
    clockTimeMinTemp[$num] = tMin;//设置界面分

    short rowTop=clockSettingRowTop($num);
    short cy=rowTop+2;
    addRowNumber($num,rowTop+(ALARM_ROW_HEIGHT-CLOCK_TEXT_HEIGHT)/2);
    // addChild(0,newChar(16,numStr[$num],0x0,4,cy));
    setClockRingtoneText($num,swHd);
    clockSWBtn[$num] = newChar(12,clockSWTxt[$num],0,177,cy);
    addChild(0,clockSWBtn[$num]);

    sprintf(clockListtime[$num][0],"%02d",tHour);
    sprintf(clockListtime[$num][1],"%02d",tMin);
    clockHourBtn[$num] = newChar(12,clockListtime[$num][0],0,105,cy);
    clockMinBtn[$num] = newChar(12,clockListtime[$num][1],0,130,cy);
    addChild(0,newChar(12,":",0,123,cy));
    addChild(0,clockHourBtn[$num]);
    addChild(0,clockMinBtn[$num]);

    for(u8 i=0;i<7;i++){
        u8 hd = (dataSaveInfo.alarmClockWeek[$num] >> i) & 0x01;
        DisplayObject *weekday=newChar(12,numCStr[i],0xFFFF,84+i*22,cy+17);
        addChildToObj(weekday,newRoundRectDisObj(hd?ORANGE:GRAY,18,18,-1,-1,3,1));
        addChild(1,weekday);
    }
}

void addNewLight(u16 $time,u8 $num){
    u8 swHd = $time&0x0F;
    if(!swHd)swHd = 1;
    if(swHd>10)swHd = 10;
    u16 tTime =  $time>>4;
    u8 tHour = tTime/60;
    u8 tMin = tTime%60;
    
    clockTimeHourTemp[$num] = tHour;//设置界面小时
    clockTimeMinTemp[$num] = tMin;//设置界面分

    short cy=clockSettingRowTop($num)+(LIGHT_ROW_HEIGHT-CLOCK_TEXT_HEIGHT)/2;
    addRowNumber($num,cy);
    
    sprintf(clockSWTxt[$num],"亮度:%d",swHd);
    clockSWBtn[$num] = newChar(12,clockSWTxt[$num],0,180,cy);
    addChild(0,clockSWBtn[$num]);

    //时间
    sprintf(clockListtime[$num][0],"%02d",tHour);
    sprintf(clockListtime[$num][1],"%02d",tMin);
    clockHourBtn[$num] = newChar(12,clockListtime[$num][0],0,105,cy);
    clockMinBtn[$num] = newChar(12,clockListtime[$num][1],0,130,cy);
    addChild(0,clockHourBtn[$num]);
    addChild(0,newChar(12,":",0,123,cy));
    addChild(0,clockMinBtn[$num]);
}


void clockSettingEx(void){
    rendWin(0,0,winW,winH,BGColor);
    
    if(clockEditHd>0&&clockEditHd<3){
        if(KEY1HOLD){
            KeyDownM1();
        }else if(KEY3HOLD){
            KeyDownM3();
        }
    }
}

void clockSettingInit(void){

    pageNow = PAGE_CLOCK_SET;
    resetPageView(&clockSettingEx);
    BGColor = 0xFFFF;

    clockSeleteNow = 0;
    clockEditHd = 0;
    clockSeleteBg = newRect(0xA73C,WS_UI_WIDTH,ALARM_ROW_HEIGHT,
                             WS_UI_LEFT,clockSettingRowTop(0));
    addChild(0,clockSeleteBg);
    //------------按钮底和文字---------
    drawPageUITxt("上一项", "编辑", "下一项");

    for(u8 i=0;i<5;i++){
        addNewClock(dataSaveInfo.alarmClock[i],i);            
    }

    setKeyFun(KML,&leaveClockSettings);
    setKeyFun(KMR,&KeyDownM1);
    setKeyFun(KM1,&KeyDownM1);
    setKeyFun(KM2,&KeyDownM2);
    setKeyFun(KM3,&KeyDownM3);
}




//-----------------亮度设置---------------------
void SettingLightEx(void){
    rendWin(0,0,winW,winH,BGColor);

    if(clockEditHd>0&&clockEditHd<3){
        if(KEY1HOLD){
            KeyDownM1();
        }else if(KEY3HOLD){
            KeyDownM3();
        }
    }
}

void setBGLightScheduleInit(void){
    pageNow = PAGE_BGLIGHT_SCHEDULE;

    resetPageView(&SettingLightEx);
    BGColor = 0xFFFF;

    clockSeleteNow = 0;
    clockEditHd = 0;
    clockSeleteBg = newRect(0xA73C,WS_UI_WIDTH,LIGHT_ROW_HEIGHT,
                             WS_UI_LEFT,clockSettingRowTop(0));
    addChild(0,clockSeleteBg);
    //------------按钮底和文字---------
    drawPageUITxt("上一项", "编辑", "下一项");

    for(u8 i=0;i<5;i++){
        addNewLight(dataSaveInfo.brightnessInfo[i],i);            
    }

    setKeyFun(KML,&leaveClockSettings);
    setKeyFun(KMR,&KeyDownM1);
    setKeyFun(KM1,&KeyDownM1);
    setKeyFun(KM2,&KeyDownM2);
    setKeyFun(KM3,&KeyDownM3);
}