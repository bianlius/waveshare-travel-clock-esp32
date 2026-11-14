
#ifndef __PAGEDRAW_H_
#define __PAGEDRAW_H_

#include "../config.h"
#define WS_UI_LEFT 40
#define WS_UI_RIGHT 280
#define WS_UI_WIDTH 240
#define WS_UI_MID 160
#define WS_UI_HEADER_HEIGHT 24
#define WS_UI_FOOTER_TOP 220
#define WS_UI_KEY_WIDTH (WS_UI_WIDTH / 3)
extern const char stra_z[26][2];
extern const char strA_Z[26][2];
extern const char strFH[28][2];
extern const char strNB[10][2];

extern struct DisplayObject* wifiSp;//WIFI图标
extern struct DisplayObject* backSp;//返回按钮
extern struct DisplayObject* sureSp;//确认按钮

extern struct DisplayObject *moneyTxtSp;//
extern struct DisplayObject *moneyIcoSp;
extern struct DisplayObject* waveObj[6];
extern struct DisplayObject *fishingCloud[5];

void addBattry(u16 $x,u16 $y);//添加电量显示
void addFrame(u16 $x,u16 $y);//添加帧率显示
void refreshBattry(u8 num); 
void refreshResource(void);
void addMoney(u8 $x,u8 $y);
void addGemstone(u8 $x,u8 $y);
void addWIFI(u16 $x,u16 $y);//添加wifi图标

// void addWave(u8 deep,u8 wy);
// void waveAction(void);
// void moveCloud(void);
// void addNewPaopaoRandom(u16 Probability,short sx);
// void addNewPaopao(short sx);
// void movePaopao(void);

void showTips(const char* txt,float time);
void hideTips(void);
void resetPageView(void (*fun)(void));
u8 pageReturnHintVisible(void);
void enablePageReturnHint(void);
void drawPageReturnHint(void);
void setPageUILongKeyText(const char* left,const char* right);
void setPageUIHoldPromotion(u8 rightSide,u8 enabled);

void middleTxtX(struct DisplayObject *txtSp,u8 txtSize);//文本居中屏幕
void drawPageUIBg(u8 TextL, u8 TextM, u8 TextR);//快速添加导航条
void drawPageUITxt(const char* TextL, const char* TextM, const char* TextR);
void setPageUIMiddleText(const char* text);
void setPageUIKeyText(u8 cell,const char* text);
u8 pageUIKeyEnabled(u8 cell);
int pageUIKeyAction(u8 cell);
void placePixelPreview(struct DisplayObject* obj, short left, short top, short width, short height);
unsigned wrapPageText(char* dst, unsigned capacity, const char* src, unsigned columns);//快速添加导航条字样
void updatePageUIBg(u8 TextL,u8 TextM,u8 TextR);//跟新导航条字样
extern u8 tipsDeep;
extern u8 paopaoDeep;
extern int scoreNum;

#endif
