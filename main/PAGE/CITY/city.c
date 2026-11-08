#include "city.h"
#include "../../config.h"
#include "GVIEW/gview.h"
#include "../../rom.h"
#include "../pageDraw.h"
#include "../../NET/RTC_Internet.h"
#include "../../NET/weather_forecast.h"

short proviceNow=0;//当前选择的省份编号
short cityNow=0;//当前选择的城市编号
u8 cityProviceNow=0;//选择省份还是城市 0省份 1城市
u8 cityLen=0;//当前省份城市数量
DisplayObject* cityProviceBg;//省份城市选择背景
#define CITY_PROVINCE_COUNT (sizeof(china_cities)/sizeof(china_cities[0]))



void changeCityRefrush(u8 provice){
    //删除旧城市列表
    removeAllLayerChild(3);
    //刷新城市列表
    cityLen = 0;
    for(u8 i=1;i<24;i++){
        if(china_cities[provice][i] == NULL){
            break;
        }else{
            DisplayObject* disObj = newChar(12,china_cities[provice][i],0x0000,170,(i-1)*24-24);  
            disObj->width = 100;          
            disObj->dir = alignMiddle;
            reTxtframeAlign(disObj,NULL);
            addChild(3,disObj);
            cityLen++;
        }
    }
    cityNow = 0;//当前选择的城市编号
    stage.movieclipArr[3].y = cityNow*-24+128;
}

static void KeyDownM3(void){
    
    //拨轮左
    if(!cityProviceNow){
        proviceNow=(proviceNow+CITY_PROVINCE_COUNT-1)%CITY_PROVINCE_COUNT;
        Tween(&(stage.movieclipArr[2].y),.3,proviceNow*-24+128,0,fs_Sine_EaseOut);
        changeCityRefrush(proviceNow);
    }else{
        if(cityLen){
            cityNow=(cityNow+cityLen-1)%cityLen;
            Tween(&(stage.movieclipArr[3].y),.3,cityNow*-24+128,0,fs_Sine_EaseOut);
        }
    }
}
static void KeyDownM2(void){
    //拨轮中
    cityProviceNow = !cityProviceNow;
    setPageUIMiddleText(cityProviceNow ? "选省份" : "选城市");
    Tween(&cityProviceBg->x,.3,cityProviceNow?164:46,0,fs_Sine_EaseOut);
}

static void KeyDownM1(void){
    //拨轮右
    if(!cityProviceNow){
        proviceNow=(proviceNow+1)%CITY_PROVINCE_COUNT;
        Tween(&(stage.movieclipArr[2].y),.3,proviceNow*-24+128,0,fs_Sine_EaseOut);
        changeCityRefrush(proviceNow);
    }else{
        if(cityLen){
            cityNow=(cityNow+1)%cityLen;
            Tween(&(stage.movieclipArr[3].y),.3,cityNow*-24+128,0,fs_Sine_EaseOut);
        }
    }
}
static void KeyDownML(void){
    //左肩键
    renderMenuInit();
}
static void KeyDownMR(void){
    //右肩键
    printf("cityProviceNow:%s\n",china_cities[proviceNow][cityNow+1]);
    dataSaveInfo.provice = proviceNow;
    dataSaveInfo.city = cityNow;
    weatherForecastRequest(1); // Refresh forecast for the selected city
    writeData();
    showTips("城市保存成功!",1);
}
static void refreshCityKeyHints(void){
    const bool hasItems=!cityProviceNow || cityLen>0;
    setPageUIKeyText(0,hasItems ? "上一项" : "");
    setPageUIMiddleText(cityProviceNow ? "选省份" : "选城市");
    setPageUIKeyText(2,hasItems ? "下一项" : "");
}
void cityEx(void){
    refreshCityKeyHints();
    rendWin(0,0,winW,winH,BGColor);
}


char* getCityTxt(u8 p,u8 c){
    return china_cities[p][c];
}
void CitySelectInit(void){
    resetPageView(&cityEx);
    pageNow = PAGE_CITY;
    BGColor = 0xFFFF;
    proviceNow = dataSaveInfo.provice<CITY_PROVINCE_COUNT ? dataSaveInfo.provice : 0;
    cityNow = dataSaveInfo.city;
    cityProviceNow = 0;
    //添加背景
    drawPageUITxt("上一项", "选城市", "下一项");
    setPageUILongKeyText("返回","保存");
    //添加省份城市选择背景
    addChild(1,newRoundRectDisObj(0x667f,110,22,46,102,4,0));
    addChild(1,newRoundRectDisObj(0x667f,110,22,164,102,4,0));
    cityProviceBg = newRoundRectDisObj(0x667f,110,22,46,102,4,1);
    addChild(1,cityProviceBg);

    //城市选择文字
    for(u8 i=0;i<CITY_PROVINCE_COUNT;i++){
        DisplayObject* disObj = newChar(12,china_cities[i][0],0x0000,52,i*24-24);
        disObj->width = 100;
        disObj->dir = alignMiddle;
        reTxtframeAlign(disObj,NULL);
        addChild(2,disObj);
        stage.movieclipArr[2].Mask = getRect(46,30,110,184);
    }

    stage.movieclipArr[3].Mask = getRect(164,30,110,184);

    changeCityRefrush(proviceNow);//初始显示第10个省份的城市列表
    cityNow = dataSaveInfo.city < cityLen ? dataSaveInfo.city : 0;
    stage.movieclipArr[2].y = proviceNow*-24+128;
    stage.movieclipArr[3].y = cityNow*-24+128;

    refreshCityKeyHints();
    setKeyFun(KM1,KeyDownM1);//拨轮左
    setKeyFun(KM2,KeyDownM2);//拨轮中
    setKeyFun(KM3,KeyDownM3);//拨轮右
    setKeyFun(KML,KeyDownML);//左肩键
    setKeyFun(KMR,KeyDownMR);//右肩键
}