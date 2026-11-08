#include "LCD/lcd_init.h"
#include "LCD/viewport.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "pageDraw.h"
#include "../sound.h"
#include "GVIEW/gview.h"
// #include "../LCD/lcd.h"

// int scoreNum = 0;

const char stra_z[26][2] = {"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z"};
const char strA_Z[26][2] = {"A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z"};
const char strFH[28][2]  = {"~","!","@","#","$","^","&","*","(",")","_","-","+","=","[","]","{","}","|",":",";","'","<",">",",",".","?","/"};
const char strNB[10][2]  = {"0","1","2","3","4","5","6","7","8","9"};

struct DisplayObject* backSp;//返回按钮
struct DisplayObject* sureSp;//确认按钮

struct DisplayObject* waveObj[6];
struct DisplayObject *fishingCloud[5];

struct DisplayObject *batterySp;//电池电压
struct DisplayObject *batterySpTxt;//电池电压


struct DisplayObject *moneyIcoSp;//
struct DisplayObject *moneyTxtSp;//


struct DisplayObject *batterySpVal;//电池电压
struct DisplayObject *wifiSp;//电池电压
struct DisplayObject *frameSp;//帧率


//
void middleTxtX(struct DisplayObject *txtSp,u8 txtSize){
    u8 txtlen = getTxtLength((char *)txtSp->f);
    txtSp->x = (winW - txtlen*txtSize/2)/2;
}
void addBattry(u16 $x,u16 $y){
    batterySpTxt = newNumber(12,VoltageVal/100,0,0,$x+26,$y+2);
    addChild(moveClipMaxNum - 1,batterySpTxt);
    batterySp = newNativePic(&batPic,$x,$y);
    batterySp->color = 0x2945;
    addChild(moveClipMaxNum - 1,batterySp);
    batterySpVal = newRect(GREEN,3,8,$x+3,$y+4);
    addChild(moveClipMaxNum - 1,batterySpVal);
}

void refreshBattry(u8 num){
    if(batterySpVal == NULL || batterySpTxt == NULL)return;
    if(num>=42){
        if(num>=100)num = 100;
        batterySpVal->height = 8;
        batterySpVal->y = batterySp->y+4;
    }else if(num<=34){
        batterySpVal->height = 1;
        batterySpVal->y = batterySp->y+4+7;
    }else{
        batterySpVal->height = num - 34;
        batterySpVal->y = batterySp->y+4+(8-batterySpVal->height);
    }
    batterySpTxt->num = num;
    batterySpTxt->x = num>=100?batterySpVal->x+30:batterySpVal->x+26;
}

void addWIFI(u16 $x,u16 $y){
    wifiSp = newNativePic(&wifiPic,$x,$y);
    addChild(moveClipMaxNum - 1,wifiSp);
}


//显示系统帧率
void addFrame(u16 $x,u16 $y){
    frameSp = newNumber(12,FrameRate,0,0,$x,$y);
    addChild(moveClipMaxNum - 1,frameSp);
}






//添加圆角描边框 deep加载到哪一层
// x坐标
// y坐标
// w宽度*10pix
// h高度*10pix
// void addNewKuan(u8 $deep,u8 x,u8 y,u8 w,u8 h){
//     DisplayObject *J1 = newPic(&kuanA,x,y);
//     DisplayObject *J2 = newPic(&kuanA,x+w*10,y);
//     DisplayObject *J3 = newPic(&kuanA,x,y+h*10);
//     DisplayObject *J4 = newPic(&kuanA,x+w*10,y+h*10);
//     for(u8 i=0;i<w-1;i++){
//         DisplayObject *KU = newPic(&kuanC,x+10+i*10,y);
//         DisplayObject *KD = newPic(&kuanC,x+10+i*10,y+h*10);
//         KD->dir = 0xF0;
//         addChild($deep,KU);
//         addChild($deep,KD);
//     }
//     for(u8 i=0;i<h-1;i++){
//         DisplayObject *KL = newPic(&kuanB,x,y+10+i*10);
//         DisplayObject *KR = newPic(&kuanB,x+w*10,y+10+i*10);
//         KR->dir = 0x0F;
//         addChild($deep,KL);
//         addChild($deep,KR);
//     }
//     J2->dir = 0xF;
//     J3->dir = 0xF0;
//     J4->dir = 0xFF;
//     addChild($deep,J1);
//     addChild($deep,J2);
//     addChild($deep,J3);
//     addChild($deep,J4);

//     addChild($deep,newRect(COLOR256_16[0x3c]>>8|COLOR256_16[0x3c]<<8,(w-1)*10,(h-1)*10,x+10,y+10));    
// }

//添加圆角单色框 deep加载到哪一层
// x坐标
// y坐标
// w宽度*4pix
// h高度*4pix
// void addNewKuanSingle(u8 $deep,u16 $color,u8 x,u8 y,u8 w,u8 h){
//     u8 kW = 4;

//     DisplayObject *J1 = newPicSingle(&kuan4A,$color,x,y);
//     DisplayObject *J2 = newPicSingle(&kuan4A,$color,x+w*kW,y);
//     DisplayObject *J3 = newPicSingle(&kuan4A,$color,x,y+h*kW);
//     DisplayObject *J4 = newPicSingle(&kuan4A,$color,x+w*kW,y+h*kW);

//     DisplayObject *KU = newRect($color,kW*(w-1),kW,x+kW,y);
//     DisplayObject *KD = newRect($color,kW*(w-1),kW,x+kW,y+h*kW);
//     KD->dir = 0xF0;
//     addChild($deep,KU);
//     addChild($deep,KD);
    
//     DisplayObject *KL = newRect($color,kW,kW*(h-1),x,y+kW);
//     DisplayObject *KR = newRect($color,kW,kW*(h-1),x+w*kW,y+kW);
//     KR->dir = 0x0F;
//     addChild($deep,KL);
//     addChild($deep,KR);
        
//     J2->dir = 0xF;
//     J3->dir = 0xF0;
//     J4->dir = 0xFF;
//     addChild($deep,J1);
//     addChild($deep,J2);
//     addChild($deep,J3);
//     addChild($deep,J4);

//     addChild($deep,newRect($color,(w-1)*kW,(h-1)*kW,x+kW,y+kW));
// }



//===============提示START================

static u8 extraPageReturnHint;
static const char *longKeyText[2];
static u8 holdPromotionEnabled[2] = {1,1};
static const char *shortKeyText[3];
static int headerKeyActions[3];
static void refreshHeaderLabels(void);

static const char *pageHoldKeyText(unsigned side){
    if(longKeyText[side]) return longKeyText[side];
    if(side || !pageReturnHintVisible()) return "";
    return pageNow==PAGE_MENU && !softKeyHeaderVisible ? "退出" : "返回";
}

struct DisplayObject *tipsBG;//
struct DisplayObject *tipsTxt;

void hideTips(void){
    removeChild(tipsTxt);
    removeChild(tipsBG);
    tipsTxt = NULL;
    tipsBG = NULL;
}

void resetPageView(void (*fun)(void)){
    homeBgmSuspend();
    extraPageReturnHint=0;
    longKeyText[0]=longKeyText[1]=NULL;
    holdPromotionEnabled[0]=holdPromotionEnabled[1]=1;
    memset(shortKeyText,0,sizeof(shortKeyText));
    hideTips();
    clearDelayFunction(&hideTips);
    resetView(fun);
}

u8 pageReturnHintVisible(void){
    if(extraPageReturnHint) return 1;
    switch(pageNow){
        case PAGE_MENU:
        case PAGE_WIFIHOME:
        case PAGE_SHOP:
        case PAGE_GARDEN:
        case PAGE_GARDENSHOP:
        case PAGE_CITY:
        case PAGE_SOUND_SET:
        case PAGE_SYSTEM_SET:
        case PAGE_BGM_SET:
        case PAGE_CLOCK_SET:
        case PAGE_BGLIGHT_SCHEDULE:
        case PAGE_CLOCKCOUNT:
        case PAGE_CLOCKCOUNT_SET:
        case PAGE_TCLCK:
        case PAGE_TCLCK_SET:
        case PAGE_WIFI_HOME:
        case PAGE_WIFI_SAVED:
        case PAGE_WIFI_SAVED_ONE:
        case PAGE_WIFI_SCAN:
        case PAGE_WIFI_PW:
        case PAGE_WIFI_CONNECT:
        case PAGE_HELP:
            return 1;
        default:
            return 0;
    }
}

void enablePageReturnHint(void){ extraPageReturnHint=1; }

void setPageUILongKeyText(const char* left,const char* right){
    longKeyText[0]=left;
    longKeyText[1]=right;
    refreshHeaderLabels();
}

void setPageUIHoldPromotion(u8 rightSide,u8 enabled){
    if(rightSide>1) return;
    holdPromotionEnabled[rightSide]=enabled;
    refreshHeaderLabels();
}

void drawPageReturnHint(void){
    if(!pageReturnHintVisible()) return;
    const char *left=pageHoldKeyText(0);
    const char *right=pageHoldKeyText(1);
    // A promoted hold action is already shown in the short-press header.
    if(softKeyHeaderVisible && headerKeyActions[0]==3) left="";
    if(softKeyHeaderVisible && headerKeyActions[2]==4) right="";
    char text[40];
    if(*left){
        snprintf(text,sizeof(text),"长按左键%s",left);
        LCD_NativeTextDraw(text,WS_UI_LEFT+4,WS_UI_FOOTER_TOP+2,GRAY,12,
                           WS_UI_LEFT,WS_UI_FOOTER_TOP,WS_UI_RIGHT,240);
    }
    if(*right){
        snprintf(text,sizeof(text),"长按右键%s",right);
        // Size 12 uses 8 pixels per ASCII byte or 16 per GB2312 character.
        short x=WS_UI_RIGHT-4-getTxtLength(text)*8;
        LCD_NativeTextDraw(text,x,WS_UI_FOOTER_TOP+2,GRAY,12,
                           WS_UI_LEFT,WS_UI_FOOTER_TOP,WS_UI_RIGHT,240);
    }
}

//显示一个提示1.5S
void showTips(const char* txt,float time){
    clearDelayFunction(&hideTips);
    if(tipsBG || tipsTxt) hideTips();
    static char wrapped[256];
    const LCDViewport viewport = LCD_GetViewport();
    // Native size 12 maps the 16x16/8x16 font bitmaps to LCD pixels 1:1.
    // All transient notices share this path, including Wi-Fi and shop results.
    unsigned rows = wrapPageText(wrapped,sizeof(wrapped),txt,28);
    const int glyphHeight=16, linePitch=20, padding=8;
    const int height=(rows-1)*linePitch+glyphHeight+padding*2;
    const int sourceHeight=(height*viewport.height+239)/240;
    const int top=viewport.top+(viewport.height-sourceHeight)/2;
    const short layerX=stage.movieclipArr[moveClipMaxNum-1].x;
    const short layerY=stage.movieclipArr[moveClipMaxNum-1].y;
    tipsBG=newRect(0,viewport.width,sourceHeight,viewport.left-layerX,top-layerY);
    tipsBG->alpha=10;
    addChild(moveClipMaxNum-1,tipsBG);
    tipsTxt=newEmpty(-layerX,-layerY);
    addChild(moveClipMaxNum-1,tipsTxt);
    unsigned row=0;
    for(char *line=wrapped;line;){
        char *next=strchr(line,'\n');
        if(next) *next++=0;
        const int width=getTxtLength(line)*8;
        const int textX=viewport.left+(viewport.width-width*viewport.width/240)/2;
        const int textY=top+(padding+row*linePitch)*viewport.height/240;
        addChildToObj(tipsTxt,newChar(12,line,0xFFFF,textX,textY));
        line=next;
        ++row;
    }
    // Keep glyphs opaque so the native LCD renderer is used for the whole notice.
    if(time != 0) DelayFunction(&hideTips,time+0.5);
}
//===============提示END================


//===============绘制圆START================

void setPColor(u8 *p,short x,short y){
    u8 w = p[2];
    u8 h = p[3];
    if(x<w&&y<h&&x>=0&&y>=0){
        p[7+x+y*w] = 253;
    }
}

//绘制UI
const char *btnName[22] = {"  ","输入","切换","退出","确定","刷新","选择","返回","删除","设置","启动","暂停","继续","停止","保存","新增","取消","清零","孵化","查看","跟随","妖怪"};
// The three header cells describe short BOOT / PWR / PLUS actions only.
enum { HEADER_TEXT_LEFT=3, HEADER_TEXT_MIDDLE=4, HEADER_TEXT_RIGHT=5 };
static u8 headerKeyEnabled[3];
// Original teal/orange sides with a coordinating slate blue centre.
static const uint16_t headerColors[3]={0x0635,0x3B50,0xFCC0};
static const uint16_t headerTextColors[3]={0xFFFF,0xFFFF,0xFFFF};

static u8 hasActionText(const char *text){
    if(!text) return 0;
    while(*text==' ' || *text=='\t') ++text;
    return *text!=0;
}

static void refreshHeaderLabel(unsigned cell){
    if(!softKeyHeaderVisible) return;
    DisplayObject *label=getChildAt(&stage.movieclipArr[moveClipMaxNum-1].disObj,HEADER_TEXT_LEFT+cell);
    if(!label) return;
    const int shortKeys[3]={2,1,0};
    const char *text=shortKeyText[cell];
    int key=shortKeys[cell];
    if(!hasActionText(text) && cell!=1 && holdPromotionEnabled[cell==2]){
        text=pageHoldKeyText(cell==2);
        key=cell==0 ? 3 : 4;
    }
    headerKeyEnabled[cell]=hasActionText(text);
    headerKeyActions[cell]=headerKeyEnabled[cell] ? key : -1;
    label->visible=headerKeyEnabled[cell];
    label->f=(char *)(headerKeyEnabled[cell] ? text : "");
    label->x=WS_UI_LEFT+cell*WS_UI_KEY_WIDTH+WS_UI_KEY_WIDTH/2-getTxtLength(label->f)*4;
}

static void refreshHeaderLabels(void){
    for(unsigned cell=0;cell<3;++cell) refreshHeaderLabel(cell);
}

static void setHeaderLabel(unsigned cell,const char *text){
    shortKeyText[cell]=text;
    refreshHeaderLabel(cell);
}

void drawPageUITxt(const char* TextL, const char* TextM, const char* TextR){
    if(tipsBG || tipsTxt){
        hideTips();
        clearDelayFunction(&hideTips);
    }
    const char *labels[3]={TextL,TextM,TextR};
    softKeyHeaderVisible=1;
    removeAllLayerChild(moveClipMaxNum-1);
    for(unsigned cell=0;cell<3;++cell){
        addChild(moveClipMaxNum-1,newRoundRectDisObj(headerColors[cell],
            WS_UI_KEY_WIDTH-2,WS_UI_HEADER_HEIGHT-2,
            WS_UI_LEFT+cell*WS_UI_KEY_WIDTH+1,1,4,1));
    }
    for(unsigned cell=0;cell<3;++cell){
        addChild(moveClipMaxNum-1,newChar(12,labels[cell] ? labels[cell] : "",
            headerTextColors[cell],0,4));
        setHeaderLabel(cell,labels[cell]);
    }
}

void drawPageUIBg(u8 TextL,u8 TextM,u8 TextR){
    drawPageUITxt(btnName[TextL],btnName[TextM],btnName[TextR]);
}

void setPageUIKeyText(u8 cell,const char* text){
    if(softKeyHeaderVisible && cell<3) setHeaderLabel(cell,text);
}

u8 pageUIKeyEnabled(u8 cell){
    return softKeyHeaderVisible && cell<3 && headerKeyEnabled[cell];
}

int pageUIKeyAction(u8 cell){
    return pageUIKeyEnabled(cell) ? headerKeyActions[cell] : -1;
}

void setPageUIMiddleText(const char* text){
    if(softKeyHeaderVisible) setHeaderLabel(1,text);
}

void updatePageUIBg(u8 TextL,u8 TextM,u8 TextR){
    if(!softKeyHeaderVisible) return;
    setHeaderLabel(0,btnName[TextL]);
    setHeaderLabel(1,btnName[TextM]);
    setHeaderLabel(2,btnName[TextR]);
}

// Include every animation frame and its origin offset when placing a preview.
void placePixelPreview(DisplayObject* obj, short left, short top, short width, short height){
    const unsigned char *p = obj->ac ? obj->ac->p : obj->p;
    int minX = 32767, minY = 32767, maxX = -32768, maxY = -32768;
    unsigned frames = obj->ac ? p[1] : 1;
    for(unsigned frame=0; frame<frames; ++frame){
        unsigned offset = frame * 7;
        unsigned dimensions = (p[4+offset] || p[5+offset]) ? offset : (p[3+offset]-1)*7;
        int w = (p[2+dimensions] << 8) | p[3+dimensions];
        int h = (p[4+dimensions] << 8) | p[5+dimensions];
        int x = (int)p[6+offset] - 127 - (obj->ac ? w/2 : 0);
        int y = (int)p[7+offset] - 127 - (obj->ac ? h/2 : 0);
        if(x < minX) minX = x;
        if(y < minY) minY = y;
        if(x+w > maxX) maxX = x+w;
        if(y+h > maxY) maxY = y+h;
    }
    obj->x = left + (width - (maxX-minX))/2 - minX;
    obj->y = top + (height - (maxY-minY))/2 - minY;
}

unsigned wrapPageText(char* dst, unsigned capacity, const char* src, unsigned columns){
    unsigned used=0, column=0, rows=1;
    if(!capacity) return 0;
    while(*src && used+1 < capacity){
        if(*src == '\n'){
            dst[used++] = *src++;
            column = 0;
            ++rows;
            continue;
        }
        unsigned step = (unsigned char)*src > 0x80 && src[1] ? 2 : 1;
        if(column+step > columns){
            if(used+2 >= capacity) break;
            dst[used++] = '\n';
            column = 0;
            ++rows;
        }
        if(used+step >= capacity) break;
        for(unsigned i=0;i<step;++i) dst[used++] = *src++;
        column += step;
    }
    dst[used] = 0;
    return rows;
}
