#include "LCD/viewport.h"
#include "calendar.h"
#include "../../NET/RTC_Internet.h"
#include "../HOME/home.h"
#include "../../rom.h"
#include "GVIEW/gview.h"
#include <stdio.h>

static void KeyDownM1(void){
    clockPageInit();
}
static void KeyDownM3(void){
    renderHomeInit();
}



//-----------------------------------------------------

void calendarEx(void){
    rendWin(0,0,winW,winH,BGColor);
}



const char* Darr[7] = {"日","一","二","三","四","五","六"};
static char calendarTitle[20];
void renderCalendarInit(void){
    homeState = 3;
    BGColor = 0x9ebf; // Original calendar blue.
    pageNow = PAGE_CALENDAR;
    resetPageView(&calendarEx);
    LCD_SetPageViewport(&pageNow, PAGE_CALENDAR, 40, 0, 240, 240);

    snprintf(calendarTitle,sizeof(calendarTitle),"%04d年%02d月",year,month);
    addChild(9,newChar(12,calendarTitle,0x0000,
                       WS_UI_MID-getTxtLength(calendarTitle)*4,22));

    u16 weekDaysNum = get_month_info(year,month);
    u8 weekNum = weekDaysNum >> 8;
    u8 weekY = 0;
    addChild(4,newRoundRectDisObj(0xFFFF,224,24,48,52,4,1));
    for(u8 i=0;i<7;i++)
        addChild(4,newChar(12,Darr[i],(i==0||i==6)?ORANGE:0,56+i*32,56));
    for(u8 i=0;i<(weekDaysNum&0xFF);i++){
        const short x = 54 + weekNum * 32;
        const short y = 88 + weekY * 24;
        if(day-1 == i)
            addChild(0,newRoundRectDisObj(ORANGE,28,22,x-2,y-3,3,1));
        addChild(1,newNumber(12,i+1,0,day-1 == i ? 0xFFFF : 0,x+(i<9?7:3),y));
        if(++weekNum > 6){ weekNum = 0; ++weekY; }
    }

    setKeyFun(KM1,&mainViewOpenGarden);
    setKeyFun(KM3,&mainViewOpenShop);
    setKeyFun(KML,&KeyDownM3);
    setKeyFun(KMR,&KeyDownM1);
    setKeyFun(KMU1,&KeyDownM1);
    setKeyFun(KMU3,&KeyDownM3);
}
