#include "stdint.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "esp_timer.h"
#include "./home.h"
#include "../SHOP/shop.h"
#include "../../rom.h"
#include "LCD/viewport.h"
#include "../../NET/weather_forecast.h"
#include "../GARDEN/garden.h"

#define WS_HOME_LEFT 40
#define WS_HOME_TOP 0
#define WS_HOME_SIZE 240
#define WS_HOME_TOP_LEFT_INSET 6
#define WS_HOME_RIGHT (WS_HOME_LEFT + WS_HOME_SIZE)
#define WS_HOME_BOTTOM (WS_HOME_TOP + WS_HOME_SIZE)
#define WS_HOME_GROUND_OFFSET 70
#define WS_HOME_CLOUD_OFFSET WS_HOME_GROUND_OFFSET
#define WS_HOME_BASE_FPS 20

// Keep the original per-second scroll speeds when the render rate changes.
static short wsHomeStep(short pixelsAtBaseFps, uint16_t *carry){
    const uint16_t total = *carry + pixelsAtBaseFps * WS_HOME_BASE_FPS;
    const short step = total / fps;
    *carry = total % fps;
    return step;
}
// Forest sprites use the native-width canopy assets.
#define WS_FOREST_BG_IMAGE senlinBTop
#define WS_FOREST_TREE0 treeF0Top
#define WS_FOREST_TREE1 treeF1Top
#define WS_FOREST_TREE2 treeF2Top
#define WS_FOREST_TREE3 treeF3Top
#define WS_FOREST_BG_TOP_SHIFT 100
#define WS_FOREST_TREE0_TOP_SHIFT 100
#define WS_FOREST_TREE123_TOP_SHIFT 99

//===========场景颜色=============
char cloudCArr[7][6] = {
    {0xa5,0x97,0x63,0x2e,0x1,0},//凌晨
    {0xe7,0xdf,0xbe,0xbc,0x1,0},//早上
    {0xff,0xff,0xdf,0x7e,0x1,0},//中午
    {0xff,0xfd,0xcd,0xf5,0x1,0},//午后
    {0xfe,0xd0,0xac,0x54,0x1,0},//傍晚
    {0xbd,0xd4,0x74,0x12,0x1,0},//夜晚
    {0x7c,0x33,0x42,0xcc,0x1,0}//深夜
};

char grassCArr[7][6] = {
    {0x3b,0x2b,0x74,0xf2,0x1a,0x7},//凌晨
    {0x4c,0xd2,0x86,0x76,0x3b,0xce},//早上
    {0x64,0xc3,0xae,0xee,0x4c,0xe7},//中午
    {0x43,0x21,0x95,0xa1,0x22,0x20},//午后
    {0x5b,0x21,0xa4,0xe1,0x32,0x20},//傍晚
    {0x42,0x63,0x53,0xc3,0x32,0xc2},//夜晚
    {0xa,0x9,0x1a,0xcb,0x1,0x27} //深夜
};
//t天空
u16 skyColor[7] = {0x63f3,0x8e1c,0x863e,0xa69c,0xad58,0x4a8d,0x10e5};//凌晨 - 深夜
//前景
u16 grassBColor[7] = {0x8e5,0x1045,0x4a2c,0x2926,0x2926,0x2126,0x1024};
u16 grassFColor[7] = {0x18e4,0x1904,0x39ea,0x1040,0x1060,0x1060,0x0021};
//树
const u16 treeColorA[] = {
    0x634c,0x94f4,0x7c31,0x63cd,0x736a,0x530e,0x18e5
};
const u16 treeColorB[] = {
    0x4269,0x424a,0x530c,0x3206,0x4ac6,0x1127,0x0082
};
//海

char seaCArr[7][6] = {
    {0xcf,0x1c,0xad,0xfa,0x95,0x7a},//凌晨
    {0xce,0xfd,0xae,0x5c,0x8d,0x3a},//早上
    {0xe7,0x9f,0xc7,0x1f,0xa6,0x5f},//中午
    {0xbf,0x3c,0xbf,0x3c,0x86,0x7c},//午后
    {0xfe,0x95,0xfe,0x14,0xd5,0x14},//傍晚
    {0xc5,0xef,0xd5,0xd2,0xa4,0x31},//夜晚
    {0x6c,0x75,0x5b,0xf3,0x3b,0x12} //深夜
};
const u16 seaColor[] = {
    0x6bd4,0x74d7,0x8d1d,0x6dfb,0x9c75,0x7bb0,0x2a8f
};
//红色色调精灵颜色表
// const char colorArr[] = {0x0,0x0,0xf9,0x86,0xfc,0xd3,0x98,0x00,0xff,0xff,0x13,0x30,0xe7,0x3c};
//=========道具颜色==============
const char BikeColorArr[9][30] = {
    {0xd0,0x41,0xa0,0x41,0x28,0xe3,0x10,0x0,0x83,0x6c,0x41,0x44,0xac,0xb1,0xde,0x16,0xf6,0xf9,0xa5,0x55,0xce,0x79,0xbd,0x32,0xff,0xff,0xf6,0xf9,0xdf,0x1d},//赤红色
    {0xce,0x60,0x62,0xc0,0x28,0xe3,0x10,0x0,0x83,0x6c,0x41,0x44,0xac,0xb1,0xde,0x16,0xf6,0xf9,0xa5,0x55,0xce,0x79,0xbd,0x32,0xff,0xff,0xf6,0xf9,0xdf,0x1d},//黄绿色
    {0x60,0xc,0x40,0x9,0x28,0xe3,0x10,0x0,0x83,0x6c,0x41,0x44,0xac,0xb1,0xde,0x16,0xf6,0xf9,0xa5,0x55,0xce,0x79,0xbd,0x32,0xff,0xff,0xf6,0xf9,0xdf,0x1d},//紫色
    {0x5,0x39,0x2,0xc,0x28,0xe3,0x10,0x0,0x83,0x6c,0x41,0x44,0xac,0xb1,0xde,0x16,0xf6,0xf9,0xa5,0x55,0xce,0x79,0xbd,0x32,0xff,0xff,0xf6,0xf9,0xdf,0x1d},//蔚蓝
    {0xfc,0x80,0x9a,0xa0,0x28,0xe3,0x10,0x0,0x83,0x6c,0x41,0x44,0xac,0xb1,0xde,0x16,0xf6,0xf9,0xa5,0x55,0xce,0x79,0xbd,0x32,0xff,0xff,0xf6,0xf9,0xdf,0x1d},//橙色
    {0x2b,0x36,0x0,0xe9,0x28,0xe3,0x10,0x0,0x83,0x6c,0x41,0x44,0xac,0xb1,0xde,0x16,0xf6,0xf9,0xa5,0x55,0xce,0x79,0xbd,0x32,0xff,0xff,0xf6,0xf9,0xdf,0x1d},//青色
    {0xe3,0x9d,0x58,0xc,0x28,0xe3,0x10,0x0,0x83,0x6c,0x41,0x44,0xac,0xb1,0xde,0x16,0xf6,0xf9,0xa5,0x55,0xce,0x79,0xbd,0x32,0xff,0xff,0xf6,0xf9,0xdf,0x1d},//粉色
    {0x13,0x20,0x2a,0x60,0x28,0xe3,0x10,0x0,0x83,0x6c,0x41,0x44,0xac,0xb1,0xde,0x16,0xf6,0xf9,0xa5,0x55,0xce,0x79,0xbd,0x32,0xff,0xff,0xf6,0xf9,0xdf,0x1d},//草绿色
    {0xd6,0x5,0x63,0x20,0x28,0xe3,0x10,0x0,0x83,0x6c,0x41,0x44,0xac,0xb1,0xde,0x16,0xf6,0xf9,0xa5,0x55,0xce,0x79,0xbd,0x32,0xff,0xff,0xf6,0xf9,0xdf,0x1d},//黄色
};
const char ShoesColorArr[][6] = {
    {0x10,0x0,0x63,0x2c,0xc6,0x18},
    {0x0,0xe9,0x1,0xd3,0x4,0x19},
    {0x0,0x0,0xb5,0x96,0xff,0xff},
    {0x10,0x0,0x60,0x6,0xf9,0x86}
};
const char glovesColorArr[][6] = {
    {0x8,0xc3,0xce,0x79,0xff,0xff},
    {0x8,0xc3,0x84,0x51,0xc6,0x38},
    {0x8,0xc3,0x60,0x0,0xc9,0x20}
};

//森林背景
u8 SenlinBgMovieHd = 0;
DisplayObject *treef0;
DisplayObject *treef1;
DisplayObject *treef2;
DisplayObject *treef3;
DisplayObject *senlinb1a;
DisplayObject *senlinb1b;
DisplayObject *senlinb2a;
DisplayObject *senlinb2b;

//天气
DisplayObject* weatherObj;
static DisplayObject *weatherDescriptionObj;
DisplayObject* tempNetObj;
static DisplayObject *tempDegreeObj;
static DisplayObject *homeBatteryObj;
static DisplayObject *homePlantHintObj;
//草
DisplayObject *grassArr[10];
DisplayObject *grassRctObj;
DisplayObject *grassMArr[2];
DisplayObject *grassBArr[3];
DisplayObject *grassFArr[6];

DisplayObject *cloud1Obj;
DisplayObject *cloud2Obj;
DisplayObject *mountain1Obj;
DisplayObject *mountain2Obj;
DisplayObject *tree1Obj;//森林
DisplayObject *tree2Obj;
DisplayObject *tree3Obj;
DisplayObject *sunObj;
DisplayObject *moonObj;
DisplayObject *timeDotObj[2];
//海
DisplayObject* seaObj[2];
DisplayObject* seaRectObj;

DisplayObject* houseObj;
DisplayObject* fireObj;
DisplayObject* bikeman;
DisplayObject* phlPkm;
#define HOME_BIKE_PARTS 9
#define HOME_BIKE_WHEEL_COUNT 2
#define HOME_BIKE_TRAVEL 24
static DisplayObject *homeBikeParts[HOME_BIKE_PARTS];
static short homeBikeBaseX[HOME_BIKE_PARTS];
static u8 homeBikePartCount;
static uint32_t homeBikeFrame;

static void moveHomeBike(void){
    if(!homeBikePartCount || !fps) return;
    short offset=0;
    bool pedalling=false;
    if(hour>=5){
        // Ease into each ride, accelerate in the middle, then slow down
        // before reversing. Keep each sprite on integer pixel coordinates.
        const uint32_t period=(uint32_t)fps*8;
        const uint32_t half=period/2;
        const uint32_t phase=(homeBikeFrame++ + period/4)%period;
        pedalling=phase<half;
        const uint32_t travel=pedalling ? phase : period-phase;
        const uint32_t t=travel*1024/half;
        const uint32_t eased=(uint32_t)((uint64_t)t*t*(3072-2*t)/(1024*1024));
        offset=-HOME_BIKE_TRAVEL+
               (short)((2*HOME_BIKE_TRAVEL*eased+512)/1024);
    }else homeBikeFrame=0;
    for(u8 i=0;i<homeBikePartCount;i++)
        homeBikeParts[i]->x=homeBikeBaseX[i]+offset;
    // Wheels roll in both directions. The rider freewheels on the return
    // ride, so the feet and clothing stay in a fixed pose instead of pedalling backward.
    const uint8_t wheelFrame=((offset+HOME_BIKE_TRAVEL)/3)%4;
    const uint8_t riderFrame=pedalling ? wheelFrame : 0;
    for(u8 i=0;i<homeBikePartCount;i++){
        DisObjAction *action=homeBikeParts[i]->ac;
        if(action && action->totallFrame>1){
            const uint8_t frame=(i<HOME_BIKE_WHEEL_COUNT ? wheelFrame : riderFrame)%action->totallFrame;
            if(action->currentFrame!=frame) AcGotoAndStop(action,frame);
        }
    }
}

#define HOME_SCENE_DRIFT_X 4
#define HOME_UI_DRIFT_X 3
#define HOME_UI_DRIFT_Y 2
#define HOME_DRIFT_PERIOD_SECONDS (20U*60U)
static uint32_t homeDriftFrame;
static short homeSceneOffsetX;
static short homeUiOffsetX;
static short homeUiOffsetY;

static void driftHomeScene(void){
    if(!fps) return;
    // Slowly shift every illustrated layer and the status layer on integer
    // pixels. Left/down only keeps edge-aligned icons inside the viewport.
    const uint32_t period=(uint32_t)fps*HOME_DRIFT_PERIOD_SECONDS;
    const uint32_t half=period/2;
    const uint32_t phase=homeDriftFrame++%period;
    const uint32_t travel=phase<half ? phase : period-phase;
    const uint32_t t=travel*1024/half;
    const uint32_t eased=(uint32_t)((uint64_t)t*t*(3072-2*t)/(1024*1024));
    const short sceneX=-(short)((HOME_SCENE_DRIFT_X*eased+512)/1024);
    const short uiX=-(short)((HOME_UI_DRIFT_X*eased+512)/1024);
    const short uiY=(short)((HOME_UI_DRIFT_Y*eased+512)/1024);
    const short sceneChange=sceneX-homeSceneOffsetX;
    if(sceneChange){
        for(u8 layer=0;layer<moveClipMaxNum-1;layer++)
            stage.movieclipArr[layer].x+=sceneChange;
        homeSceneOffsetX=sceneX;
    }
    stage.movieclipArr[moveClipMaxNum-1].x+=uiX-homeUiOffsetX;
    stage.movieclipArr[moveClipMaxNum-1].y+=uiY-homeUiOffsetY;
    homeUiOffsetX=uiX;
    homeUiOffsetY=uiY;
}

static void addHomeBikePart(u8 deep,DisplayObject *part){
    addChild(deep,part);
    if(part->ac && part->ac->totallFrame>1) AcGotoAndStop(part->ac,0);
    if(homeBikePartCount<HOME_BIKE_PARTS){
        homeBikeParts[homeBikePartCount]=part;
        homeBikeBaseX[homeBikePartCount]=part->x;
        ++homeBikePartCount;
    }
}



//--------树--------
u16 treeShowTime = 0;
void changeTreeColor(void){
    Tween((short *)&senlinb1a->color,1,(short)treeColorA[DayTime],0,fs_EaseColor);
    Tween((short *)&senlinb1b->color,1,(short)treeColorA[DayTime],0,fs_EaseColor);
    Tween((short *)&senlinb2a->color,1,(short)treeColorB[DayTime],0,fs_EaseColor);
    Tween((short *)&senlinb2b->color,1,(short)treeColorB[DayTime],0,fs_EaseColor);
}
void startSenlinBgMovie(void){
    SenlinBgMovieHd = 1;

    treef0->visible = true;
    treef1->visible = true;
    treef2->visible = true;
    treef3->visible = true;

    treef0->x = 320;
    treef1->x = 320+100;
    treef2->x = 320+200;
    treef3->x = 320+300;

    senlinb1a->visible = true;
    senlinb1b->visible = true;
    senlinb2a->visible = true;
    senlinb2b->visible = true;

    senlinb2a->x = 320;
    senlinb2b->x = 640;
    senlinb1a->x = 370;
    senlinb1b->x = 690;

    changeTreeColor();

    SenlinBgMovieHd = 1;
    treeShowTime = random()%(fps*10)+(fps*5);
}
void stopSenlinBgMovie(void){
    treeShowTime = 0;
    SenlinBgMovieHd = 0;
    senlinb1a->visible = false;
    senlinb1b->visible = false;
    senlinb2a->visible = false;
    senlinb2b->visible = false;

    treef0->visible = false;
    treef1->visible = false;
    treef2->visible = false;
    treef3->visible = false;
}
//--------树END--------------
//--------- 海-----------
void startSeaBgMovie(void){
    Tween(&seaObj[0]->y,8,114+6+WS_HOME_GROUND_OFFSET,0,fs_Cubic_EaseOut);
    Tween(&seaObj[1]->y,8,114+6+WS_HOME_GROUND_OFFSET,0,fs_Cubic_EaseOut);
    Tween(&seaRectObj->y,8,114+2+WS_HOME_GROUND_OFFSET,0,fs_Cubic_EaseOut);

    Tween(&cloud1Obj->y,8,8+WS_HOME_CLOUD_OFFSET,0,fs_Cubic_EaseOut);
    Tween(&cloud2Obj->y,8,49+WS_HOME_CLOUD_OFFSET,0,fs_Cubic_EaseOut);
}
void stopSeaBgMovie(void){
    Tween(&seaObj[0]->y,8,144+6+WS_HOME_GROUND_OFFSET,0,fs_Cubic_EaseOut);
    Tween(&seaObj[1]->y,8,144+6+WS_HOME_GROUND_OFFSET,0,fs_Cubic_EaseOut);
    Tween(&seaRectObj->y,8,144+2+WS_HOME_GROUND_OFFSET,0,fs_Cubic_EaseOut);

    Tween(&cloud1Obj->y,8,30+WS_HOME_CLOUD_OFFSET,0,fs_Cubic_EaseOut);
    Tween(&cloud2Obj->y,8,71+WS_HOME_CLOUD_OFFSET,0,fs_Cubic_EaseOut);
}
//----------海END---------

u8 DayTime = 2;//0-凌晨 1 早上 2-白天 3 下午 4-傍晚 5-夜晚 6 深夜 

char tempTxt[10] = "27";
char waterTxt[10] = "45%";
char tempNetTxt[10] = "未知";
char weatherTxt[20] = "未知";
u8* weatherArr[40] = {weather0,weather1,weather0,weather1,weather4,weather5,weather6,weather7,weather8,weather9,weather10,weather11,weather12,weather13,weather14,weather15,weather16,weather17,weather18,weather19,weather20,weather21,weather22,weather23,weather24,weather25,weather26,weather27,weather28,weather29,weather30,weather31,weather32,weather33,weather34,weather35,weather36,weather37,weather38,weather99};
u8 weatherCode = 39;//天气图标编号

static void KeyDownM1(void){
    // holdSound(10);
    // if(backLightNum>0){
    //     setBackLightVal(backLightNum-1);
    // }
    // printf("Key M1 Down\n");

    // clockPageInit();
    // DayTime ++;
    // if(DayTime>6)DayTime = 0;
    // changeSceneColor();
    renderCalendarInit();
}
static void KeyDownM3(void){    
    // holdSound(10);
    //  if(backLightNum<10){
    //     setBackLightVal(backLightNum+1);
    // } 
    // printf("Key M3 Down\n");
    weatherInit();
}

void mainViewOpenShop(void){
    renderShopInit();
}
void mainViewOpenGarden(void){
    renderGardenInit();
}



// Match the existing battery icon's 3.4V empty / 4.2V full range.
// This is a voltage-based estimate; no fuel gauge is fitted.
static char homeBatteryText[24];
static int64_t homeBatteryLastRefreshUs = -60000000LL;
static void positionHomeBattery(void){
    if(!homeBatteryObj || pageNow != PAGE_HOME) return;
    homeBatteryObj->x = WS_HOME_RIGHT - WS_HOME_TOP_LEFT_INSET
        - getTxtLength(homeBatteryText) * 8;
    if(homePlantHintObj)
        homePlantHintObj->x = homeBatteryObj->x - homePlantHintObj->width - 4;
}
static void refreshHomeBatteryText(void){
    const unsigned voltageMv = VoltageVal;
    if(!voltageMv){
        snprintf(homeBatteryText,sizeof(homeBatteryText),"电量 --%%");
        positionHomeBattery();
        return;
    }
    const unsigned percent = voltageMv <= 3400 ? 0
        : voltageMv >= 4200 ? 100 : (voltageMv - 3400) * 100 / 800;
    snprintf(homeBatteryText,sizeof(homeBatteryText),"电量 %u%%",percent);
    positionHomeBattery();
    homeBatteryLastRefreshUs = esp_timer_get_time();
}

void doUIfor1S(void){
    if(VoltageVal && esp_timer_get_time() - homeBatteryLastRefreshUs >= 60000000LL)
        refreshHomeBatteryText();
    if(timeDotObj[0]->visible){
        timeDotObj[0]->visible = 0;
        timeDotObj[1]->visible = 0;
    }else{
        timeDotObj[0]->visible = 1;
        timeDotObj[1]->visible = 1;
    }
    

    //=======处理图标状态=========
    if(WifiConnectingState == 3){
        wifiSp->visible = 1;
    }else{
        wifiSp->visible = wifiSp->visible?0:1;
    }
    // The home garden cue blinks once per second while plants are available.
    if(homePlantHintObj)
        homePlantHintObj->visible = cloverNumNow ? !homePlantHintObj->visible : 0;
    
    //===========================
}

u8 weatherSunny;
u8 weatherRaining;
u8 weatherSnowing;
u8 rainDeep = 7;
u8 snowDeep = 8;

u8 rainPower = 0;
void rainAction(void){
    if(weatherRaining){
        if(stage.movieclipArr[rainDeep].length<rainPower){
            if(random()%100<50){
                if(random()%100<50)addChild(rainDeep,newLine(0xFFFF,3,0,0,5));
                else addChild(rainDeep,newLine(0xFFFF,2,0,0,3));
                getChildAt(&stage.movieclipArr[rainDeep].disObj,stage.movieclipArr[rainDeep].length-1)->offsetx = WS_HOME_LEFT + random()%(WS_HOME_SIZE+80);
            }
        }
        //移动雨滴
        u8 isRemove = false;
        for(u8 i=0;i<stage.movieclipArr[rainDeep].length;i++){
            DisplayObject *obj = getChildAt(&stage.movieclipArr[rainDeep].disObj,i);
            obj->offsetx -= obj->x == 2?12:16;;
            obj->offsety += obj->x == 2?16:20;
            if(obj->offsety>WS_HOME_BOTTOM){
                if(stage.movieclipArr[rainDeep].length>rainPower&&!isRemove){
                    //清除雨滴
                    removeChild(obj);
                    isRemove = true;
                    i--;
                }else{
                    obj->offsety = WS_HOME_TOP-10-random()%2;
                    obj->offsetx = WS_HOME_LEFT + random()%(WS_HOME_SIZE+80);
                }
            }
        }
    }else{
        if(stage.movieclipArr[rainDeep].length>0){
            //移动雨滴
            u8 isRemove = false;
            for(u8 i=0;i<stage.movieclipArr[rainDeep].length;i++){
                DisplayObject *obj = getChildAt(&stage.movieclipArr[rainDeep].disObj,i);
                obj->offsetx -= obj->x == 2?12:16;;
                obj->offsety += obj->x == 2?16:20;
                if(obj->offsety>WS_HOME_BOTTOM){
                    //清除雨滴
                    if(!isRemove){
                        removeChild(obj);
                        isRemove = true;
                    }else{
                        obj->offsety = WS_HOME_TOP-10-random()%2;
                        obj->offsetx = WS_HOME_LEFT + random()%(WS_HOME_SIZE+80);
                    }
                }
            }
        }
    }
}

void snowAction(void){
    if(weatherSnowing){
        if(stage.movieclipArr[snowDeep].length<10){
            if(random()%100<10){
                u8 rNum = random()%100;
                if(rNum<33)addChild(snowDeep,newRoundRectDisObj(0xFFFF,6,6,WS_HOME_LEFT + random()%(WS_HOME_SIZE+80),WS_HOME_TOP-10,3,1));
                else if(rNum<66)addChild(snowDeep,newRoundRectDisObj(0xFFFF,4,4,WS_HOME_LEFT + random()%(WS_HOME_SIZE+80),WS_HOME_TOP-10,2,1));
                else addChild(snowDeep,newRoundRectDisObj(0xFFFF,3,3,WS_HOME_LEFT + random()%(WS_HOME_SIZE+80),WS_HOME_TOP-10,1,1));
            }
        }
        //移动雪
        for(u8 i=0;i<stage.movieclipArr[snowDeep].length;i++){
            u8 moveY = 0;
            DisplayObject *obj = getChildAt(&stage.movieclipArr[snowDeep].disObj,i);
            if(obj->width == 6)moveY = 6;
            else if(obj->width == 4)moveY = 5;
            else if(obj->width == 3)moveY = 4;
            obj->x -= moveY;
            obj->y += moveY;
            if(obj->y>WS_HOME_BOTTOM){
                obj->y = WS_HOME_TOP-10-random()%2;
                obj->x = WS_HOME_LEFT + random()%(WS_HOME_SIZE+80);
            }
        }
    }else{
        if(stage.movieclipArr[snowDeep].length>0){
            //移动雪
            u8 isRemove = false;
            for(u8 i=0;i<stage.movieclipArr[snowDeep].length;i++){
                u8 moveY = 0;
                DisplayObject *obj = getChildAt(&stage.movieclipArr[snowDeep].disObj,i);
                if(obj->width == 6)moveY = 6;
                else if(obj->width == 4)moveY = 5;
                else if(obj->width == 3)moveY = 4; 
                obj->x -= moveY;
                obj->y += moveY;
                if(obj->y>WS_HOME_BOTTOM){
                    //清除雪
                    if(!isRemove){
                        removeChild(obj);
                        isRemove = true;
                    }
                }
            }
        }
    }
}

void starAction(void){
    if(weatherSunny){
        if(hour >= 19 || hour < 5){
            //如果是夜晚
            if(stage.movieclipArr[0].length<10){
                if(random()%100<10){
                    // Keep the full star within the Waveshare viewport.
                    short mx = WS_HOME_LEFT + 5 + random()%(WS_HOME_SIZE - 10);
                    short my = WS_HOME_TOP + 5 + random()%70;
                    DisplayObject* obj = getNewMvAction(&star,mx,my);
                    obj->ac->scaleX = obj->ac->scaleY = random()%100<50?30:50;
                    obj->alpha = 0;
                    addChild(0,obj);
                }
            }
        }
    }
    //星星闪
    u8 isRemove = false;
    for(u8 i=0;i<stage.movieclipArr[0].length;i++){
        // if(FrameRate%3)stage.movieclipArr[0].disObjArr[i]->x--;
        DisplayObject *obj = getChildAt(&stage.movieclipArr[0].disObj,i);
        if(!obj->dir){
            if(obj->alpha<20){
                obj->alpha += 1;
            }else{
                obj->dir = 1;
                obj->alpha -= 1;
            }
        }else{
            if(obj->alpha>0){
                obj->alpha--;
            }else if(!isRemove){
                removeChild(obj);
                isRemove = true;
            }
        }
    }
}

void sunAction(void){
    if(DayTime >= 1 && DayTime <= 4 && weatherSunny){
        sunObj->visible = true;
        weatherObj->visible = false;
    }else{
        sunObj->visible = false;
        weatherObj->visible = true;
    }
    // The sunny illustration replaces only the small icon; keep its caption.
    if(weatherDescriptionObj)
        weatherDescriptionObj->visible = weatherCode != 39;
    resetHomeTempX();
}

float cloud1ObjX;
float cloud2ObjX;

u16 skyColorTemp = 0;
static void positionWeatherDescription(void){
    if(!weatherDescriptionObj || !weatherObj) return;
    weatherObj->x = (pageNow == PAGE_HOME ? WS_HOME_RIGHT : WS_UI_RIGHT - 6)
        - weatherObj->width;
    if(pageNow == PAGE_HOME){
        // Match the date's 12px white text directly below it.
        weatherDescriptionObj->x = WS_HOME_LEFT + WS_HOME_TOP_LEFT_INSET;
        weatherDescriptionObj->y = 52;
    }else if(pageNow == PAGE_CLOCK && tempNetObj){
        // Match the bottom temperature and keep an 8px gap before it.
        weatherDescriptionObj->x = tempNetObj->x - 8
            - getTxtLength(weatherTxt) * 8;
        weatherDescriptionObj->y = tempNetObj->y;
    }
}
void refreshWeatherDisplay(void){
    static uint32_t shownVersion = UINT32_MAX;
    static u8 shownCode = 255;
    uint32_t version = weatherForecastVersion();
    if(shownVersion == version && shownCode == weatherCode) return;
    shownVersion = version;
    shownCode = weatherCode;
    if(weatherObj && weatherObj->type) updatePicData(weatherObj,weatherArr[weatherCode]);
    resetHomeTempX();
    positionWeatherDescription();
}
void homeEx(void){
    refreshWeatherDisplay();
    //处理天气
    if(weatherCode==0||weatherCode==1||weatherCode==2||weatherCode==3||weatherCode==38){
        weatherSunny = true;
    }else{
        weatherSunny = false;
    }

    if(weatherCode >=10 && weatherCode <=18){
        float bl = (float)weatherCode/8;
        rainPower = (float)weatherCode * bl * bl;
        weatherRaining = true;
    }else{
        weatherRaining = false;
    }

    if(weatherCode >=19 && weatherCode <=25){
        weatherSnowing = true;
    }else{
        weatherSnowing = false;
    }


    moveHomeBike();
    driftHomeScene();

    // Draw only the 240x240 area sent to the LCD; forest sprites are costly.
    rendWin(WS_HOME_LEFT,WS_HOME_TOP,WS_HOME_RIGHT,WS_HOME_BOTTOM,skyColorTemp);

    // pkmMovieEx(0);//精灵动画处理
    // pkmMovieEx(1);//精灵动画处理
    if(checkMonsterType(itemSaveInfo.monsterIndex)){
        pkmMovieEx1(0);//陆地精灵动画处理
    }else{
        pkmMovieEx2(0,WS_HOME_GROUND_OFFSET);//飞行精灵动画处理
    }

    rainAction();
    snowAction();
    starAction();
    sunAction();

    if(!(FrameRate%(fps/2))){
        doUIfor1S();
    }


    //------------处理场景运动----------------
    

    

    if(hour > 4){
        static uint16_t grassStageCarry, grassMiddleCarry, grassBackCarry;
        static uint16_t grassFrontCarry, treeCarry, forestBack1Carry;
        static uint16_t forestBack2Carry, seaCarry;
        const short grassStageStep = wsHomeStep(grassArr[0]->width/6, &grassStageCarry);
        const short grassMiddleStep = wsHomeStep(grassArr[0]->width/4, &grassMiddleCarry);
        const short grassBackStep = wsHomeStep(10, &grassBackCarry);
        const short grassFrontStep = wsHomeStep(16, &grassFrontCarry);
        const short treeMoveDst = wsHomeStep(8, &treeCarry);
        const short forestBack1Step = wsHomeStep(4, &forestBack1Carry);
        const short forestBack2Step = wsHomeStep(6, &forestBack2Carry);
        const short seaStep = wsHomeStep(1, &seaCarry);
        //云移动
        cloud1ObjX -= (float)WS_HOME_BASE_FPS / fps;
        cloud2ObjX -= (float)WS_HOME_BASE_FPS / fps;
        //草移动
        stage.movieclipArr[2].x -= grassStageStep;
        if(stage.movieclipArr[2].x<=-grassArr[0]->width+homeSceneOffsetX){
            stage.movieclipArr[2].x = homeSceneOffsetX;
        }

        grassMArr[0]->x -= grassMiddleStep;
        grassMArr[1]->x -= grassMiddleStep;
        if(grassMArr[0]->x < -100){
            grassMArr[0]->x = 320+100;
        }

        if(grassMArr[1]->x < -100){
            grassMArr[1]->x = 320+100;
        }

        for(u8 i=0;i<3;i++){
            grassBArr[i]->x -= grassBackStep;
            if(grassBArr[i]->x<-grassBArr[i]->width){
                grassBArr[i]->x = 320;
            }
        }
        
        for(u8 i=0;i<6;i++){
            grassFArr[i]->x -= grassFrontStep;
            if(grassFArr[i]->x<-grassFArr[i]->width){
                grassFArr[i]->x = 320;
            }
        }

        //山移动
        // mountain1Obj->x -= 2;
        // mountain2Obj->x -= 2;   
        // if(mountain1Obj->x<=-mountain1Obj->width){
        //     mountain1Obj->x = winW;
        // }
        // if(mountain2Obj->x<=-mountain2Obj->width){
        //     mountain2Obj->x = winW;
        // }

        // //树移动
        // tree1Obj->x -= 4;
        // tree2Obj->x -= 4;
        // tree3Obj->x -= 4;
        // if(tree1Obj->x<=-tree1Obj->width){
        //     if(random()%100<10){
        //         tree1Obj->x = winW + tree1Obj->width;
        //     }
        // }
        // if(tree2Obj->x<=-tree2Obj->width){
        //     if(random()%100<10){
        //         tree2Obj->x = winW + tree1Obj->width;
        //     }
        // }
        // if(tree3Obj->x<=-tree3Obj->width){
        //     if(random()%100<10){
        //         tree3Obj->x = winW + tree1Obj->width;
        //     }
        // }

        //树移动

        if(treeShowTime){
            treeShowTime --;
        }else{
            SenlinBgMovieHd = 0;
            if(!treef0->visible&&!treef1->visible&&!treef2->visible&&!treef3->visible){
                if(random()%(fps*250) == 0){
                    startSenlinBgMovie();
                }
            }
        }
        if(SenlinBgMovieHd){
            if(treef0->x > -treef0->width){
                treef0->x -= treeMoveDst;
            }else{
                if(random()%10>5)treef0->x = 320;
            }
            if(treef2->x > -treef2->width){
                treef2->x -= treeMoveDst;
            }else{
                if(random()%10>5)treef2->x = 320;
            }


            if(treef3->x > -treef3->width){
                treef3->x -= treeMoveDst;
            }else{
                if(random()%10>5)treef3->x = 320;
            }

            if(treef1->x > -treef1->width){
                treef1->x -= treeMoveDst;
            }else{
                if(random()%10>5)treef1->x = 320;
            }

            //背景1
            if(senlinb1a->x > -320){
                senlinb1a->x -= forestBack1Step;
            }else{
                senlinb1a->x = 320;
            }
            if(senlinb1b->x > -320){
                senlinb1b->x -= forestBack1Step;
            }else{
                senlinb1b->x = 320;
            }
            //背景2
            if(senlinb2a->x > -320){
                senlinb2a->x -= forestBack2Step;
            }else{
                senlinb2a->x = 320;
            }
            if(senlinb2b->x > -320){
                senlinb2b->x -= forestBack2Step;
            }else{
                senlinb2b->x = 320;
            }
        }else{

            

            //背景1
            if(senlinb1a->x > -360){
                senlinb1a->x -= forestBack1Step;
            }else{
                senlinb1a->visible = false;
            }
            if(senlinb1b->x > -360){
                senlinb1b->x -= forestBack1Step;
            }else{
                senlinb1b->visible = false;
            }


            //背景2
            if(senlinb2a->x > -320){
                senlinb2a->x -= forestBack2Step;
            }else{
                if(senlinb1a->visible||senlinb1b->visible)
                    senlinb2a->x = 320;
                else{
                    
                    if(senlinb2a->x < -senlinb2a->width){
                        senlinb2a->visible = false;
                    }else senlinb2a->x -= forestBack2Step;
                }
            }
            if(senlinb2b->x > -320){
                senlinb2b->x -= forestBack2Step;
            }else{
                if(senlinb1a->visible||senlinb1b->visible)
                    senlinb2b->x = 320;
                else{
                    if(senlinb2b->x < -senlinb2b->width){
                        senlinb2b->visible = false;
                    }else senlinb2b->x -= forestBack2Step;
                }
            }

            //前景树
            if(treef0->x > -treef0->width){
                treef0->x -= treeMoveDst;
            }else{
                if(senlinb2a->visible || senlinb2b->visible){
                    if(random()%10>5)treef0->x = 320;
                }else treef0->visible = false;
                
            }

            if(treef1->x > -treef1->width){
                treef1->x -= treeMoveDst;
            }else{
                if(senlinb2a->visible||senlinb2b->visible){
                    if(random()%10>5)treef1->x = 320;
                }else treef1->visible = false;
                
            }
            if(treef2->x > -treef2->width){
                treef2->x -= treeMoveDst;
            }else{
                if(senlinb2a->visible||senlinb2b->visible){
                    if(random()%10>5)treef2->x = 320;
                }else treef2->visible = false;
                
            }


            if(treef3->x > -treef3->width){
                treef3->x -= treeMoveDst;
            }else{
                if(senlinb2a->visible||senlinb2b->visible){
                    if(random()%10>5)treef3->x = 320;
                }else treef3->visible = false;
                
            }
            // treef0->visible = false;
            // treef1->visible = false;
            // treef2->visible = false;
            // treef3->visible = false;

            // senlinb1a->visible = false;
            // senlinb1b->visible = false;
            // senlinb2a->visible = false;
            // senlinb2b->visible = false;
        }

        if(random()%(fps*250) == 0){
            startSeaBgMovie();
        }else if(random()%(fps*250) < 9){
            stopSeaBgMovie();
        }

        seaObj[0]->x -= seaStep;
        if(seaObj[0]->x <= -320/2){
            seaObj[0]->x = 320+320/2;
        }

        seaObj[1]->x -= seaStep;
        if(seaObj[1]->x <= -320/2){
            seaObj[1]->x = 320+320/2;
        }

    }else{
        //云移动
        cloud1ObjX -= (float)(WS_HOME_BASE_FPS / 2) / fps;
        cloud2ObjX -= (float)(WS_HOME_BASE_FPS / 2) / fps;
    }

    

    cloud1Obj->x = (int)cloud1ObjX;
    cloud2Obj->x = (int)cloud2ObjX;

    if(cloud1Obj->x<=-cloud1Obj->width){
        cloud1Obj->x = winW;
        cloud1ObjX = cloud1Obj->x;
    }
    if(cloud2Obj->x<=-cloud2Obj->width){
        cloud2Obj->x = winW;
        cloud2ObjX = cloud2Obj->x;
    }

    

}

// Keep the temperature and degree glyph together as weather data changes.
void resetHomeTempX(void){
    if(!tempNetObj) return;
    if(pageNow == PAGE_HOME){
        const u8 captionVisible = weatherDescriptionObj && weatherDescriptionObj->visible;
        tempNetObj->x = WS_HOME_LEFT + WS_HOME_TOP_LEFT_INSET
            + (captionVisible ? getTxtLength(weatherTxt) * 8 + 8 : 0);
        tempNetObj->y = 52;
        // Hide the startup placeholder until a numeric temperature arrives.
        const char *number = tempNetTxt[0] == '-' ? tempNetTxt + 1 : tempNetTxt;
        tempNetObj->visible = *number >= '0' && *number <= '9';
    }else if(pageNow == PAGE_CLOCK){
        tempNetObj->x = WS_UI_RIGHT - 20 - getTxtLength(tempNetTxt) * 8 - 2;
        tempNetObj->y = 221;
        tempNetObj->visible = 1;
    }
    if(tempDegreeObj){
        tempDegreeObj->x = pageNow == PAGE_HOME
            ? tempNetObj->x + getTxtLength(tempNetTxt) * 8 + 2
            : WS_UI_RIGHT - 20;
        tempDegreeObj->y = pageNow == PAGE_HOME ? 54 : 219;
        tempDegreeObj->visible = tempNetObj->visible;
    }
}

void addTompBottomTxt(void){
    homePlantHintObj=NULL;
    homeBatteryObj=NULL;
    tempDegreeObj=NULL;
    char *cityNameTemp = getCityTxt(dataSaveInfo.provice,dataSaveInfo.city+1);
    if(pageNow == PAGE_HOME){
        addChild(moveClipMaxNum - 1,newChar(18,timeString,0xffff,WS_HOME_LEFT+WS_HOME_TOP_LEFT_INSET,8));
        addChild(moveClipMaxNum - 1,newChar(12,dateString,0xffff,WS_HOME_LEFT+WS_HOME_TOP_LEFT_INSET,32));
        refreshHomeBatteryText();
        homeBatteryObj = newChar(12,homeBatteryText,0xffff,0,221);
        addChild(moveClipMaxNum - 1,homeBatteryObj);
        timeDotObj[0] = newRect(0xFFFF,2,2,WS_HOME_LEFT+WS_HOME_TOP_LEFT_INSET+27,14);
        timeDotObj[1] = newRect(0xFFFF,2,2,WS_HOME_LEFT+WS_HOME_TOP_LEFT_INSET+27,22);
        addChild(moveClipMaxNum - 1,timeDotObj[0]);
        addChild(moveClipMaxNum - 1,timeDotObj[1]);
    }else{
        // Center the 12px date and weekday against the 24px weather icon at y=8.
        addChild(moveClipMaxNum - 1,newChar(12,dateString,0xffff,WS_UI_LEFT+6,14));
    }
    addWIFI(pageNow == PAGE_HOME ? WS_HOME_LEFT+6 : WS_UI_LEFT+6,
            pageNow == PAGE_HOME ? 222 : 222);
    addChild(moveClipMaxNum - 1,newChar(12,cityNameTemp,0xFFFF,
        pageNow == PAGE_HOME ? WS_HOME_LEFT+24 : WS_UI_LEFT+24,
        pageNow == PAGE_HOME ? 221 : 221));
    weatherObj = newNativePic(weatherArr[weatherCode],
        pageNow == PAGE_HOME ? WS_HOME_RIGHT-24 : 202,
        pageNow == PAGE_HOME ? 8 : 4);
    if(pageNow == PAGE_CLOCK) weatherObj->y = 8;
    tempNetObj = newChar(12,tempNetTxt,0xffff,105,
        pageNow == PAGE_HOME ? 52 : 221);
    addChild(moveClipMaxNum - 1,weatherObj);
    weatherDescriptionObj = newChar(12,weatherTxt,0xFFFF,0,
        pageNow == PAGE_HOME ? 52 : 221);
    if(pageNow == PAGE_HOME && weatherCode == 39)
        weatherDescriptionObj->visible = 0;
    addChild(moveClipMaxNum - 1,weatherDescriptionObj);
    addChild(moveClipMaxNum - 1,tempNetObj);
    tempDegreeObj = newNativePicSingle(
        pageNow == PAGE_HOME ? ducHome : duc,0xFFFF,0,0);
    addChild(moveClipMaxNum - 1,tempDegreeObj);
    if(pageNow == PAGE_HOME){
        // Keep the plant cue beside the battery in the bottom-right corner.
        homePlantHintObj=newNativePic(gardenHint,0,homeBatteryObj->y-3);
        homePlantHintObj->visible=0;
        addChild(moveClipMaxNum - 1,homePlantHintObj);
        positionHomeBattery();
    }
    resetHomeTempX();
    positionWeatherDescription();
}





void checkDayTime(void){
    if(hour < 4){
        DayTime = 6;
    }else if(hour < 5){
        DayTime = 0;
    }else if(hour < 9){
        DayTime = 1;
    }else if(hour < 16){
        DayTime = 2;
    }else if(hour < 17){
        DayTime = 3;
    }else if(hour < 18){
        DayTime = 4;
    }else if(hour < 19){
        DayTime = 5;
    }else{
        DayTime = 6;
    }
}

void changeSceneColor(void){
    if(pageNow == PAGE_HOME){
        checkDayTime();
        cloud1Obj->f = cloud2Obj->f = cloudCArr[DayTime];
        for(u8 i=0;i<10;i++){
            grassArr[i]->f = grassCArr[DayTime];
        }
        grassRctObj->color = (u8)grassCArr[DayTime][4]<<8|(u8)grassCArr[DayTime][5];
        grassMArr[0]->color = grassMArr[1]->color = (u8)grassCArr[DayTime][0]<<8|(u8)grassCArr[DayTime][1];
        for(u8 i=0;i<3;i++){
            grassBArr[i]->color = grassBColor[DayTime];
        }
        for(u8 i=0;i<6;i++){
            grassFArr[i]->color = grassFColor[DayTime];
        }
        Tween((short *)&skyColorTemp,1,(short)skyColor[DayTime],0,fs_EaseColor);

        //海
        for(u8 i=0;i<2;i++){
            setPicColor(seaObj[i],seaCArr[DayTime]);
        }
        seaRectObj->color = seaColor[DayTime];

        changeTreeColor();
        

        if(hour < 5){
            //帐篷睡觉
            houseObj->visible = true;
            fireObj->visible = true;
            stage.movieclipArr[6].visible = false;
            // bikeMan->visible = false;
            if(phlPkm)
            phlPkm->visible = false;
        }else{
            houseObj->visible = false;
            fireObj->visible = false;
            // bikeMan->visible = true;
            stage.movieclipArr[6].visible = true;

            if(phlPkm)
            phlPkm->visible = true;
        }
    }
}


void addManBike(u8 deep,short $x,short $y,u8 bikeNum,u8 headNum,u8 shirtNum,u8 pantsNum,u8 shoesNum,u8 glovesNum,u8 bagNum){
     //---------------添加角色----------------
    //单车
    addHomeBikePart(deep,getNewMvAction(&bikeLunSharp,$x-17+14,$y+14+11));
    addHomeBikePart(deep,getNewMvAction(&bikeLunSharp,$x+22+14,$y+14+11));
    
    //人
    bikeman = getNewMvAction(&bikeMan,$x-6,$y-27);//$x-1 + 39/2 $x-31+62/2
    bikeman->x += bikeman->width/2;
    bikeman->y += bikeman->height/2;
    addHomeBikePart(deep,bikeman);
    setPicColor(bikeman,(char *)BikeColorArr[bikeNum]);
    if(shoesNum){
        DisplayObject* shoesObj = getNewMvAction(shoesListArr[shoesNum-1],bikeman->x,bikeman->y);
        addHomeBikePart(deep,shoesObj);
        setPicColor(shoesObj,ShoesColorArr[shoesNum-1]);;
    }


    if(headNum)addHomeBikePart(deep,getNewMvAction(headListArr[headNum-1],bikeman->x,bikeman->y));//头
    if(pantsNum)addHomeBikePart(deep,getNewMvAction(pantsListArr[pantsNum-1],bikeman->x,bikeman->y));//裤子
    if(shirtNum)addHomeBikePart(deep,getNewMvAction(shirtListArr[shirtNum-1],bikeman->x,bikeman->y));//上衣
    //手套
    if(glovesNum){
        DisplayObject* glovesObj = getNewMvAction(glovesListArr[glovesNum-1],bikeman->x,bikeman->y);
        addHomeBikePart(deep,glovesObj);
        setPicColor(glovesObj,glovesColorArr[glovesNum-1]);;
    }
    if(bagNum)addHomeBikePart(deep,getNewMvAction(bagListArr[bagNum-1],bikeman->x,bikeman->y));//背包
}

char hz[3];
const u8* tentArr[2] = {sleepBag,house};

u8 treeStartFirst = 0;
void renderHomeInit(void){
    homeState = 0;
    homeBikePartCount=0;
    homeBikeFrame=0;
    homeDriftFrame=0;
    homeSceneOffsetX=0;
    homeUiOffsetX=0;
    homeUiOffsetY=0;
    resetPageView(&homeEx);
    BGColor = skyColor[DayTime];
    skyColorTemp = skyColor[DayTime];
    pageNow = PAGE_HOME;
    LCD_SetPageViewport(&pageNow,PAGE_HOME,
        WS_HOME_LEFT,WS_HOME_TOP,WS_HOME_SIZE,WS_HOME_SIZE);
    
    // Keep the sun inside the 240x240 home viewport.
    sunObj = newPic(&sun,0,0);
    sunObj->x = WS_HOME_RIGHT - sunObj->width;
    addChild(1,sunObj);
    sunObj->visible = false;
    //添加云
    cloud1Obj = newPic(&cloud1,0,30+WS_HOME_CLOUD_OFFSET);
    //  cloud1Obj = newRect(0xFFFF,10,10,0,30);
    addChild(1,cloud1Obj);
    cloud2Obj = newPic(&cloud2,162,71+WS_HOME_CLOUD_OFFSET);
    //  cloud2Obj = newRect(0xFFFF,10,10,162,71);
    addChild(1,cloud2Obj);
    cloud1ObjX = cloud1Obj->x;
    cloud2ObjX = cloud2Obj->x;


    // cloud1Obj->y = 8;
    // cloud2Obj->y = 48;
    //海
    seaObj[0] = getNewMvAction(&sea,0,144+6+WS_HOME_GROUND_OFFSET);
    seaObj[1] = getNewMvAction(&sea,320,144+6+WS_HOME_GROUND_OFFSET);
    seaRectObj = newRect(0x851f,320,44,0,144+2+WS_HOME_GROUND_OFFSET);
    addChild(1,seaRectObj);
    addChild(1,seaObj[0]);
    addChild(1,seaObj[1]);

    
    // startSeaBgMovie();

    //森林
    senlinb1a = newPic(&WS_FOREST_BG_IMAGE,60,-14+WS_HOME_GROUND_OFFSET-WS_FOREST_BG_TOP_SHIFT);
    addChild(1,senlinb1a);
    senlinb1a->color = 0x7c31;
    senlinb1b = newPic(&WS_FOREST_BG_IMAGE,60,-14+WS_HOME_GROUND_OFFSET-WS_FOREST_BG_TOP_SHIFT);
    addChild(1,senlinb1b);
    senlinb1b->color = 0x7c31;


    senlinb2a = newPic(&WS_FOREST_BG_IMAGE,0,WS_HOME_GROUND_OFFSET-WS_FOREST_BG_TOP_SHIFT);
    addChild(1,senlinb2a);
    senlinb2a->color = 0x530c;
    senlinb2b = newPic(&WS_FOREST_BG_IMAGE,0,WS_HOME_GROUND_OFFSET-WS_FOREST_BG_TOP_SHIFT);
    addChild(1,senlinb2b);
    senlinb2b->color = 0x530c;

    
    treef0 = newPic(&WS_FOREST_TREE0,320,WS_HOME_GROUND_OFFSET-WS_FOREST_TREE0_TOP_SHIFT);
    addChild(1,treef0);
    treef1 = newPic(&WS_FOREST_TREE1,320,WS_HOME_GROUND_OFFSET-WS_FOREST_TREE123_TOP_SHIFT);
    addChild(1,treef1);
    treef2 = newPic(&WS_FOREST_TREE2,320,WS_HOME_GROUND_OFFSET-WS_FOREST_TREE123_TOP_SHIFT);
    addChild(1,treef2);
    treef3 = newPic(&WS_FOREST_TREE3,320,WS_HOME_GROUND_OFFSET-WS_FOREST_TREE123_TOP_SHIFT);
    addChild(1,treef3);
    gviewPrepareFast4BitSprite(treef0);
    gviewPrepareFast4BitSprite(treef1);
    gviewPrepareFast4BitSprite(treef2);
    gviewPrepareFast4BitSprite(treef3);

    
    stopSenlinBgMovie();
    

    /*//渲染压力测试
    DisplayObject *yy = newPic(&cloud1,30,30);
    addChild(1,yy);
    yy->alpha = 7;
    addChild(1,newPic(&cloud1,130,30));
    DisplayObject *tt = getNewMvAction(&LYM_M,30,80);
    addChild(1,tt);
    tt->alpha = 8;
    DisplayObject *tt2 = getNewMvAction(&YD_M,30,80);
    addChild(1,tt2);
    tt2->alpha = 8;
     DisplayObject *tt3 = getNewMvAction(&YD_M,130,80);
    addChild(1,tt3);
    tt3->alpha = 8;
    */
    //添加山



    //添加草地
    for(u8 i=0;i<10;i++){
        grassArr[i] = newPic(&grass,42*i,136+WS_HOME_GROUND_OFFSET);
        addChild(2,grassArr[i]);
    }
    grassRctObj = newRect(0x4CE7,320,4,0,136+24+WS_HOME_GROUND_OFFSET);
    addChild(1,grassRctObj);
    //----
    grassMArr[0] = newPic(&grassM0,42,145+WS_HOME_GROUND_OFFSET);
    grassMArr[1] = newPic(&grassM1,282,145+WS_HOME_GROUND_OFFSET);
    addChild(3,grassMArr[0]);
    addChild(3,grassMArr[1]);
    grassMArr[0]->color = grassMArr[1]->color = 0x4ce7;
    //----
    grassBArr[0] = newPic(&grassB0,-10,WS_HOME_BOTTOM-grassB0[5]);
    grassBArr[1] = newPic(&grassB0,210,WS_HOME_BOTTOM-grassB0[5]);
    grassBArr[2] = newPic(&grassB0,430,WS_HOME_BOTTOM-grassB0[5]);
    for(u8 i = 0;i<3;i++){
        addChild(4,grassBArr[i]);
        grassBArr[i]->color = 0x4A2C;
    }
    //----
    grassFArr[0] = newPic(&grassF0,10,WS_HOME_BOTTOM-grassF0[5]);
    grassFArr[1] = newPic(&grassF1,80,WS_HOME_BOTTOM-grassF1[5]);
    grassFArr[2] = newPic(&grassF2,170,WS_HOME_BOTTOM-grassF2[5]);
    grassFArr[3] = newPic(&grassF3,220,WS_HOME_BOTTOM-grassF3[5]);
    grassFArr[4] = newPic(&grassF4,280,WS_HOME_BOTTOM-grassF4[5]);
    grassFArr[5] = newPic(&grassF5,300,WS_HOME_BOTTOM-grassF5[5]);
    for(u8 i = 0;i<6;i++){
        addChild(4,grassFArr[i]);
        grassFArr[i]->color = 0x39EA;
    }
    
     //---------------添加角色----------------
    addManBike(6,140,105+WS_HOME_GROUND_OFFSET,itemSaveInfo.bikeIndex,itemSaveInfo.headIndex,itemSaveInfo.shirtIndex,itemSaveInfo.pantsIndex,itemSaveInfo.shoesIndex,itemSaveInfo.glovesIndex,itemSaveInfo.bagIndex);
    
    //---------------添加角色END----------------

    //添加帐篷
    houseObj = newPic(tentArr[itemSaveInfo.tentIndex],111,75+WS_HOME_GROUND_OFFSET);
    addChild(3,houseObj);
    fireObj = getNewMvAction(&fire,146,120+WS_HOME_GROUND_OFFSET);
    fireObj->x += fireObj->width/2;
    fireObj->y += fireObj->height/2;
    addChild(3,fireObj);
    houseObj->visible = false;
    fireObj->visible = false;
    houseObj->x = 160-houseObj->width/2;
    houseObj->y = 141+WS_HOME_GROUND_OFFSET-houseObj->height;


    //添加顶部和底部文字
    addTompBottomTxt();

    refrushMonsterList();//刷新精灵列表
    //添加精灵
    if(itemSaveInfo.monsterIndex && itemSaveInfo.monsterIndex<getShopListArrNum(0)){
        phlPkm = getNewMvAction(momsterListArr[itemSaveInfo.monsterIndex - 1],-100,46+WS_HOME_GROUND_OFFSET);
        if(itemSaveInfo.monsterIndex == 9){
            addChild(6,phlPkm);//比卡丘在前面
        }else{
            addChild(1,phlPkm);
        }
        
        pkmMvProp[0].disOb = phlPkm;
        pkmMvProp[0].nextTime = 0;
        if(checkMonsterType(itemSaveInfo.monsterIndex)){
            if(itemSaveInfo.monsterIndex == 9){
                Ex1Max = 100;
                Ex1Min = 40;
                phlPkm->y = 142+4+WS_HOME_GROUND_OFFSET - phlPkm->height/2;
            }else{
                Ex1Max = 240;
                Ex1Min = 60;
                phlPkm->y = 136+4+WS_HOME_GROUND_OFFSET - phlPkm->height/2;
            }
        }
    }else{
        pkmMvProp[0].disOb = NULL;
    }

    //添加精灵

    // struct DisplayObject* laysPkm = getNewMvAction(&pkm_ldys,160,46);
    // addChild(3,laysPkm);
    // pkmMvProp[0].disOb = laysPkm;
    // pkmMvProp[0].state = Hover;
    // pkmMvProp[0].nextTime = 0;
    // laysPkm->visible = false;

    // struct DisplayObject* laysPkm2 = getNewMvAction(&pkm_ldys,160,46);
    // addChild(3,laysPkm2);
    // laysPkm2->f = colorArr;
    // pkmMvProp[1].disOb = laysPkm2;
    // pkmMvProp[1].state = Hover;
    // pkmMvProp[1].nextTime = 0;
    // laysPkm2->visible = false;

    //按键事件
    setKeyFun(KM1,mainViewOpenGarden);
    setKeyFun(KM3,mainViewOpenShop);
    setKeyFun(KML,KeyDownM3);
    setKeyFun(KMR,KeyDownM1);
    setKeyFun(KMU1,KeyDownM1); // Swipe left: next main view.
    setKeyFun(KMU3,KeyDownM3); // Swipe right: previous main view.

    checkDayTime();
    changeSceneColor();
    homeBgmResume();


}
