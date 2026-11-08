#ifndef GVIEW_H
#define GVIEW_H

#define GVIEW_NAME "GVIEW"
#define GVIEW_VERSION_MAJOR 0
#define GVIEW_VERSION_MINOR 2
#define GVIEW_VERSION_PATCH 0
#define GVIEW_VERSION "0.2.0"

#include "stdint.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "math.h"

#define true 1
#define false 0

/* 定义画笔颜色 */
#define WHITE 0xFFFF        // 白色
#define BLACK 0x0000        // 黑色
#define BLUE 0x001F         // 蓝色
#define BRED 0XF81F         // 蓝色和红色的混合色
#define GRED 0XFFE0         // 绿色和红色的混合色
#define GBLUE 0X07FF        // 绿色和蓝色的混合色
#define RED 0xF800          //红色
#define MAGENTA 0xF81F      // 品红
#define GREEN 0x07E0      // 绿色
#define CYAN 0x7FFF      // 青色
#define YELLOW 0xFFE0     // 黄色
#define BROWN 0XBC40      // 棕色
#define BRRED 0XFC07      // 棕红色
#define GRAY 0X8430       // 灰色
#define ORANGE 0xfcc0     //橙色
#define DARKBLUE 0X01CF   // 深蓝色
#define LIGHTBLUE 0X7D7C  // 浅蓝色
#define GRAYBLUE 0X5458   // 灰蓝色
#define LIGHTGREEN 0X841F // 浅绿色
#define LGRAY 0XC618      // 浅灰色(PANNEL),窗体背景色
#define LGRAYBLUE 0XA651  // 浅灰蓝色(中间层颜色)
#define LBBLUE 0X2B12     // 浅棕蓝色(选择条目的反色)
#define COLORNONE 0X1     // 该值在某些函数里被判断为无填充色
#define sleeteBGColor 0xfca0//选择条目背景色
#define defaultBGColor 0x8410//选择条默认背景色


//对齐方式
typedef enum align{alignLeft,alignMiddle,alignRight}align;

//-------按键--------
extern uint16_t KEY0Temp;
extern uint16_t KEY1Temp;
extern uint16_t KEY2Temp;
extern uint16_t KEY3Temp;
extern uint16_t KEY4Temp;
extern uint16_t KEY5Temp;
extern uint16_t KEY6Temp;
extern uint16_t KEY7Temp;
extern uint8_t KEYCLDOWNTemp;
extern uint8_t KEYCRDOWNTemp;

#define KEY1HOLD KEY0Temp>10
#define KEY2HOLD KEY1Temp>10
#define KEY3HOLD KEY2Temp>10
#define KEYLHOLD KEY3Temp>10
#define KEYRHOLD KEY4Temp>10

typedef enum KeyName{
    KM1,
    KM2,
    KM3,
    KMU1,
    KMU2,
    KMU3,
    KML,
    KMR,
    KMUL,
    KMUR,
    KCL,
    KCR
}KeyName;

#define KeyD0  keyState&1
#define KeyU0  keyState>>1&1
#define KeyD1  keyState>>2&1
#define KeyU1  keyState>>3&1
#define KeyD2  keyState>>4&1
#define KeyU2  keyState>>5&1
#define KeyD3  keyState>>6&1
#define KeyU3  keyState>>7&1
#define KeyD4  keyState>>8&1
#define KeyU4  keyState>>9&1
#define KeyD5  keyState>>10&1
#define KeyU5  keyState>>10&1
#define KeyD6  keyState>>12&1
#define KeyU6  keyState>>13&1

extern uint8_t softKeyHeaderVisible;
extern int keyState;//按键操作状
//----------------

// 裁剪矩形结构
typedef struct {
    int16_t left, top, right, bottom;
    uint8_t valid;
} ClipRect;

// 内存管理结构
typedef struct MemBlock {
    void *address;
    struct MemBlock *next;
    struct MemBlock *previous;
} MemBlock;

//缓动类型
typedef enum Easeing
{
    fs_Cubic_EaseIn,
    fs_Cubic_EaseOut,
    fs_Cubic_EaseInOut,    
    fs_Sine_EaseIn,
    fs_Sine_EaseOut,
    fs_Sine_EaseInOut,
    fs_EaseNone,
    fs_EaseColor,
}   EaseType;   

typedef struct TweenS{
    uint8_t runing;//tween enable
    short *axis;//执行Tween的值必须为short 类型
    short startValue;//Tween 起始值
    short targetValue;//Tween 目标值
    uint16_t stepNow;//
    uint16_t stepAll;
    uint16_t delay;//延时执行
    uint8_t frame;
    EaseType esae;
    void (*fun)(void);//在Tween时间为0的时候可以配置该选项，延时结束会执行
}TweenS;

//显示对象
typedef struct DisplayObject{
    uint8_t type; //0-未使用 1-char 2-Number 3-rect 4-pic 5-line      0xFA - 空对象         0xFE - 主层       0xF4 - 固定色值
    short x;
    short y;
    uint8_t visible;
    uint16_t color;//当type为pic时,设置为0x100以外的值则图像显示单色
    uint8_t size;//字体大小
    uint8_t maskEnable;//是否对上一级的遮罩生效
    //render_Number
    float num;//要显示的数字
    char dot;//小数点，如为0则没有小数点

    short alpha;//0-15  15不透明  0全透明
    //render_Char
    char *f;//字体指针

    //render_Pic
    uint8_t dir;//方向 ： 0x00 正常 0x0F左右翻转 0xF0上下翻转  0xFF上下左右翻转 
    short width;
    short height;
    
    uint8_t nativeIcon; // Render this UI sprite at one LCD pixel per source pixel.
    uint8_t picType;//图片类型 16位 8位 4位 2位 1位
    uint8_t *p;//图像 指针
    short frameOffset;//图像偏移,用于渲染动画里的不同帧
    short offsetx;//
    short offsety;
    uint16_t childrenNum;//子对象数量
    struct DisplayObject *parent;//父对象，坐标相对其父级
    struct DisObjAction *ac;
    uint16_t deep;//当前加载进MovieClip的层级
} DisplayObject;

typedef struct Rect{
    short x;
    short y;
    short width;
    short height;
}Rect;

typedef struct MovieClip{
    struct DisplayObject disObj;
    short x;
    short y;
    uint8_t visible;
    uint16_t length;
    
    Rect* Mask;//rect类型的遮罩
    uint8_t MaskFollow;//MASK是否跟随MC移动
}MovieClip;

typedef struct Stage{
    struct MovieClip *movieclipArr;
}Stage;

//------动作--------

typedef struct DisObjAction{
    struct DisplayObject* disObj;
    uint8_t *p;//旋转时把原图缓存下来;
    // uint16_t fishNextMoveTime;//下一次移动的剩余时间
    uint8_t Type;//鱼的种类
    // uint16_t HP;//金币时间
    // uint16_t HPTemp;//金币时间累计
    float x;//当前坐标
    float y;
    short width;//
    short height;
    short offsetx;//
    short offsety;
    short angleCurrent;//当前angle ,== angle时不刷新旋转图案
    short angle;
    short movedirection;//移动方向
    uint8_t dir;//左右镜像
    uint8_t totallFrame; //总帧数
    float frameMultiple;//帧率倍增率
    uint8_t frameTime;//计算播放下一帧的时间，减为0则切换下一帧
    uint8_t currentFrame;//当前帧数 取值1~totallFrame
    uint8_t playing;//是否播放动画
    uint16_t frameOffset;
    short scaleX;//缩放和旋转只能同时存在一种,只对pic类型有效
    short scaleY;//100表示原始大小,除以100为实际缩放比例
    short scaleXNow;//用于记录已经缩放处理过的值
    short scaleYNow;
}DisObjAction;




//--------------api---------------
void gviewInit(uint16_t $winW,uint16_t $winH,uint16_t $moveClipMaxNum,uint16_t $displayObjMaxNum,uint16_t $MaxTweenNum,uint16_t $MaxActionNum,uint8_t $fps);
#define viewInit gviewInit /* v0.2 向后兼容别名 */
typedef void (*GVIEWNativeTextBegin)(void);
typedef uint8_t (*GVIEWNativeTextDraw)(const char *text, int16_t x, int16_t y,
                                       uint16_t color, uint8_t size,
                                       int16_t clipLeft, int16_t clipTop,
                                       int16_t clipRight, int16_t clipBottom);
void gviewSetNativeTextHooks(GVIEWNativeTextBegin begin, GVIEWNativeTextDraw draw);
typedef void (*GVIEWNativeIconBegin)(void);
typedef uint8_t (*GVIEWNativeIconDraw)(const DisplayObject *icon, int16_t x, int16_t y);
void gviewSetNativeIconHooks(GVIEWNativeIconBegin begin, GVIEWNativeIconDraw draw);
uint16_t gviewPalette8Color(uint8_t index);
void update(void);//处理每帧逻辑
void rendWin(uint16_t sx,uint16_t sy,uint16_t ex,uint16_t ey,uint16_t bg);//渲染窗口
void addChild(uint8_t num,DisplayObject *disObj);//加载显示对象到图层
void addChildToObj(DisplayObject *disObj,DisplayObject *child);//加载显示对象到显示对象里
void addChildToObjAt(DisplayObject *disObj,DisplayObject *child,uint8_t num);//加载显示对象到显示对象里,并指定深度
void setKeyFun(KeyName keyNum,void (*keyFunTemp)(void));//按键冒泡

//基本图像
DisplayObject* newEmpty(short x,short y);//空对象,用于加载子对象
DisplayObject* newPic(const void *p,short x,short y);
// Predecode a static 4-bit sprite for faster opaque span rendering.
void gviewPrepareFast4BitSprite(const DisplayObject *sprite);
//新建图像
DisplayObject* newPic4(uint8_t *p,uint16_t extendW,uint16_t extendH,short x,short y);//新建4角合成图像
DisplayObject* newPicFrame(uint8_t *p,uint8_t frame,short x,short y);//指定某一帧图像
DisplayObject* newPicSingle(const void *p,uint16_t $color,short x,short y);
DisplayObject* newNativePic(const void *p,short x,short y);
DisplayObject* newNativePicSingle(const void *p,uint16_t color,short x,short y);//单色图像
DisplayObject* newChar(uint8_t size,const void *f,uint16_t $color,short x,short y);//文本
DisplayObject* newNumber(uint8_t size,float num,char dot,uint16_t color,short x,short y);//数字
DisplayObject* newRect(uint16_t $color,short $w,short $h,short x,short y);//矩形
DisplayObject* newLine(uint16_t $color,short sx,short sy,short ex,short ey);//直线
DisplayObject* newCurve(uint8_t *p,uint16_t $color,short x,short y,uint8_t $h);//曲线拉丝
DisplayObject* newRoundRectDisObj(uint16_t $color,uint16_t $w,uint16_t $h,short $x,short $y,uint8_t r,uint8_t filled);//圆角矩形

//插件
DisplayObject* newProgress(uint16_t $color,uint8_t $w,uint8_t $h,short $x,short $y,uint8_t $r);//添加进度条插件
void setProgress(DisplayObject *pObj,uint8_t pr);//设置进度条进度值

//---添加文本框----
DisplayObject* newTxtframe(uint8_t $size,short $txtLen,short $x,short $y,char* $text,align $align,uint16_t $txtColor,uint16_t $bgColor,uint16_t $borderColor);//新建文本框
void reTxtframeAlign(DisplayObject* $txt,char *val);//刷新文本框插件的字符，并使其对齐
void reTxtframeAlignINT(DisplayObject* $txt,int $val);//使用INT数字填充，并使其对齐
//---------------- 

//---添加菜单列表插件----
DisplayObject* addMenuList(const char* const $listTxt[],uint8_t $size,uint16_t $colorTxt,uint16_t $colorBar,uint8_t $length,uint16_t $width,short $MaskSY,short $MaskEY,uint8_t $deep);//菜单列表插件
void setMenuListY(DisplayObject *disObj,uint8_t $num);//设置菜单当前选择的列表
void setMenuListBg(DisplayObject *disObj,uint8_t $boo);//显示或隐藏选择条
//-----------------

// void updateCharTxt(uint8_t *tf,uint8_t *f);//更新DisplayObject的文字显示
void updatePicData(DisplayObject *disObj,uint8_t *p);//更新一个DisplayObject的图像指针,并更新对应属性
void resetEnterFunction(void (*fun)(void));//设置逐帧渲染函数
void clearEnterFunction(void);//清除逐帧渲染函数
void resetView(void (*fun)(void));//重置所有缓存

void deleteAllTween(void);//清除所有Tween
#define delectAllTween deleteAllTween /* 旧拼写兼容别名 */
uint8_t Tween(short *axis,float time,short value,float delay,EaseType ease);//对某个数值进行缓动出来（数值需要为short类型)
uint8_t DelayFunction(void(*fun)(void),float delay);//延时执行一个函数
void clearDelayFunction(void(*fun)(void));

struct DisplayObject* getNewMvAction(const void *pic,short x,short y);//新建一个动作
void rotaAction(DisObjAction* action,short angle);//对动作进行旋转

// void freeAllAction(void);
void removeAllLayerChild(uint8_t layer);//删除层的所有显示对象
void removeChild(DisplayObject *disObj);//删除一个对象
void removeChildAt(uint16_t layer,uint16_t deep);//从层对象中移除某个对象
DisplayObject*  getChildAt(DisplayObject *disObj,uint16_t num);//查找子对象
void removeChildFor(DisplayObject *disObj,uint16_t deep);//从子对象中移除子对象
void removeAllChildForObj(DisplayObject *disObj);
uint16_t getPicOffset(uint8_t frame,uint8_t* p);//获取某帧的图像指针
void setPicColor(DisplayObject *obj,char* color);//设置4位色和2位色图的颜色
void refrushScaleAction(DisObjAction* action);//刷新缩放

//内存管理
void free_Block(void* p);//释放动态申请的内存
void* my_malloc(size_t size);//动态申请内存，每次重载页面会回收所有使用此方法申请的内存

//-----动画控制--------
void AcStop(DisObjAction* doAc);
void AcPlay(DisObjAction* doAc);
void AcGotoAndStop(DisObjAction* doAc,uint8_t frame);
void AcGotoAndPlay(DisObjAction* doAc,uint8_t frame);
void AcNextFrame(DisObjAction* doAc);
void resetAcFrameTime(DisObjAction *ac);

//----其他-----
uint8_t getTxtLength(const char *txt);//获取文本字数 （中文占两字）
Rect* getRect(short x,short y,short $w,short $h);


//------------全局变量-------------
extern DisObjAction* Action;
extern uint16_t *picCache;

extern uint16_t rendSX, rendSY, rendEX, rendEY, rendW, rendH;

extern Stage stage;

extern uint32_t localeTime;//1秒累加1
extern uint32_t FrameRate;//1帧累加1
extern uint32_t FrameRateTemp;
extern uint8_t fps;
extern uint16_t winW;
extern uint16_t winH;
extern uint16_t moveClipMaxNum;
extern uint16_t MemBlockAllNum;

extern volatile uint8_t spiSendHd; //SPI屏幕发送标志
#endif
