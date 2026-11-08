#include "inputtxt.h"
#include "wifi_keyboard.h"
#include "LCD/viewport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEYBOARD_KEYS 32
#define KEYBOARD_LEFT 48
#define KEYBOARD_WIDTH 224
#define KEYBOARD_TOP 112
#define KEYBOARD_ROW_HEIGHT 22
#define KEYBOARD_KEY_HEIGHT 20
#define KEYBOARD_TABS_TOP 88
#define KEYBOARD_ACTIONS_TOP 200
#define KEYBOARD_ACTION_HEIGHT 20

char inputTxtTemp[72];
static void (*inputSureFun)(void);
static WifiKeyboard keyboard;
static const char *const tabNames[WIFI_KEYBOARD_PAGES]={"abc","ABC","123","符号"};
static const char *const actionNames[WIFI_KEYBOARD_ACTIONS]={"删除","空格","连接"};
static char passwordText[96], countText[12], keyText[KEYBOARD_KEYS][2];
static DisplayObject *passwordLabel;
static DisplayObject *tabBackgrounds[WIFI_KEYBOARD_PAGES], *tabLabels[WIFI_KEYBOARD_PAGES];
static DisplayObject *keyBackgrounds[KEYBOARD_KEYS], *keyLabels[KEYBOARD_KEYS];
static DisplayObject *actionBackgrounds[WIFI_KEYBOARD_ACTIONS], *actionLabels[WIFI_KEYBOARD_ACTIONS];

typedef struct { short x,y,width,height; } KeyboardRect;
static KeyboardRect keyRects[KEYBOARD_KEYS];

static int contains(const KeyboardRect *rect,int x,int y){
    return x>=rect->x && x<rect->x+rect->width && y>=rect->y && y<rect->y+rect->height;
}

static void updatePassword(void){
    unsigned length=strlen(inputTxtTemp);
    if(length){
        char raw[80];
        snprintf(raw,sizeof(raw),"密码: %s",inputTxtTemp);
        wrapPageText(passwordText,sizeof(passwordText),raw,28);
        passwordLabel->color=0x2104;
    }else{
        snprintf(passwordText,sizeof(passwordText),"请输入 Wi-Fi 密码");
        passwordLabel->color=GRAY;
    }
    snprintf(countText,sizeof(countText),"%u/%u",length,WIFI_PASSWORD_MAX);
}

static void refreshKeyboard(void){
    const char *characters=wifiKeyboardCharacters(keyboard.page);
    unsigned count=wifiKeyboardCharacterCount(keyboard.page);
    unsigned columns=keyboard.page==2 ? 3 : 8;
    unsigned pitch=KEYBOARD_WIDTH/columns;
    for(unsigned tab=0;tab<WIFI_KEYBOARD_PAGES;++tab){
        tabBackgrounds[tab]->color=tab==keyboard.page ? 0x04BE : 0xE71C;
        tabLabels[tab]->color=tab==keyboard.page ? 0xFFFF : 0x2104;
    }
    for(unsigned key=0;key<KEYBOARD_KEYS;++key){
        int visible=key<count;
        keyBackgrounds[key]->visible=keyLabels[key]->visible=visible;
        if(!visible) continue;
        unsigned row=key/columns, column=key%columns;
        unsigned rowKeys=count-row*columns;
        if(rowKeys>columns) rowKeys=columns;
        KeyboardRect *rect=&keyRects[key];
        rect->x=KEYBOARD_LEFT+(KEYBOARD_WIDTH-columns*pitch)/2+
                (columns-rowKeys)*pitch/2+column*pitch;
        rect->y=KEYBOARD_TOP+row*KEYBOARD_ROW_HEIGHT;
        rect->width=pitch-2;
        rect->height=KEYBOARD_KEY_HEIGHT;
        DisplayObject *background=keyBackgrounds[key];
        // All key backgrounds use a fixed image size per layout; width changes
        // are handled by creating the matching key once when its page changes.
        background->x=rect->x;
        background->y=rect->y;
        background->visible=1;
        background->color=key==keyboard.focus ? 0x04BE : 0xE71C;
        keyText[key][0]=characters[key];
        keyText[key][1]=0;
        keyLabels[key]->x=rect->x+(rect->width-8)/2;
        keyLabels[key]->y=rect->y+2;
        keyLabels[key]->color=key==keyboard.focus ? 0xFFFF : 0x2104;
    }
    for(unsigned action=0;action<WIFI_KEYBOARD_ACTIONS;++action){
        int selected=keyboard.focus==count+action;
        actionBackgrounds[action]->color=selected ? 0x04BE : action==2 ? 0x0635 : 0xE71C;
        actionLabels[action]->color=selected || action==2 ? 0xFFFF : 0x2104;
    }
    WifiKeyboardAction action=wifiKeyboardAction(&keyboard);
    if(!wifiKeyboardCanActivate(&keyboard,strlen(inputTxtTemp),inputSureFun!=NULL))
        setPageUIMiddleText("");
    else setPageUIMiddleText(action==WIFI_KEY_DELETE ? "删除" :
        action==WIFI_KEY_SPACE ? "空格" : action==WIFI_KEY_CONNECT ? "连接" : "输入");
    updatePassword();
}

static void buildKeyBackgrounds(void){
    removeAllLayerChild(4);
    unsigned width=keyboard.page==2 ? 72 : 26;
    for(unsigned key=0;key<KEYBOARD_KEYS;++key){
        keyBackgrounds[key]=newRoundRectDisObj(0xE71C,width,KEYBOARD_KEY_HEIGHT,0,0,4,1);
        addChild(4,keyBackgrounds[key]);
    }
}

static void switchCategory(int direction){
    hideTips();
    wifiKeyboardSwitch(&keyboard,direction);
    buildKeyBackgrounds();
    refreshKeyboard();
}

static void previousKey(void){
    wifiKeyboardMove(&keyboard,-1);
    refreshKeyboard();
}
static void nextKey(void){
    wifiKeyboardMove(&keyboard,1);
    refreshKeyboard();
}
static void activateKey(void){
    WifiKeyboardAction action=wifiKeyboardAction(&keyboard);
    if(action==WIFI_KEY_CONNECT){
        if(inputSureFun) inputSureFun();
        return;
    }
    if(action==WIFI_KEY_DELETE){
        wifiPasswordDelete(inputTxtTemp);
    }else if(action==WIFI_KEY_CHARACTER || action==WIFI_KEY_SPACE){
        char character=action==WIFI_KEY_SPACE ? ' ' : wifiKeyboardCharacter(&keyboard);
        if(!wifiPasswordAppend(inputTxtTemp,sizeof(inputTxtTemp),character))
            showTips("密码最多63位",1);
    }
    refreshKeyboard();
}
static void leaveInput(void){ wifiScanPageInit(); }
static void nextCategory(void){ switchCategory(1); }
static void keyboardEx(void){ rendWin(0,0,winW,winH,BGColor); }

void inputPageGesture(int startX,int startY,int endX,int endY){
    if(pageNow!=PAGE_WIFI_PW) return;
    int dx=endX-startX, dy=endY-startY;
    if(startY>=KEYBOARD_TABS_TOP && startY<KEYBOARD_ACTIONS_TOP &&
       abs(dx)>30 && abs(dx)>2*abs(dy)){
        switchCategory(dx<0 ? 1 : -1);
        return;
    }
    if(abs(dx)>12 || abs(dy)>12) return;
    // Touch coordinates are physical LCD pixels; the page uses x=40..279.
    int x=startX+WS_UI_LEFT, y=startY;
    if(y>=KEYBOARD_TABS_TOP && y<KEYBOARD_TABS_TOP+22 &&
       x>=KEYBOARD_LEFT && x<KEYBOARD_LEFT+KEYBOARD_WIDTH){
        unsigned page=(x-KEYBOARD_LEFT)/56;
        if(page!=keyboard.page){
            hideTips();
            keyboard.page=page;
            keyboard.focus=0;
            buildKeyBackgrounds();
            refreshKeyboard();
        }
        return;
    }
    unsigned count=wifiKeyboardCharacterCount(keyboard.page);
    for(unsigned key=0;key<count;++key){
        if(contains(&keyRects[key],x,y)){
            keyboard.focus=key;
            activateKey();
            return;
        }
    }
    if(y>=KEYBOARD_ACTIONS_TOP && y<KEYBOARD_ACTIONS_TOP+KEYBOARD_ACTION_HEIGHT && x>=WS_UI_LEFT && x<WS_UI_RIGHT){
        keyboard.focus=count+(x-WS_UI_LEFT)/WS_UI_KEY_WIDTH;
        activateKey();
    }
}

void inputPageInit(void (*fun)(void),u8 cnEnable){
    (void)cnEnable; // All callers use this page to enter a Wi-Fi password.
    pageNow=PAGE_WIFI_PW;
    BGColor=0xFFFF;
    resetPageView(&keyboardEx);
    LCD_SetPageViewport(&pageNow,PAGE_WIFI_PW,40,0,240,240);
    inputSureFun=fun;
    keyboard=(WifiKeyboard){0,0};
    inputTxtTemp[WIFI_PASSWORD_MAX]=0;
    drawPageUITxt("上一键","输入","下一键");
    setPageUILongKeyText("返回","切类");
    addChild(0,newRoundRectDisObj(0xF7BE,224,60,48,26,6,1));
    passwordLabel=newChar(12,passwordText,0x2104,48,28);
    addChild(8,passwordLabel);
    addChild(8,newChar(12,countText,GRAY,232,68));
    for(unsigned tab=0;tab<WIFI_KEYBOARD_PAGES;++tab){
        tabBackgrounds[tab]=newRoundRectDisObj(0xE71C,54,22,48+tab*56,KEYBOARD_TABS_TOP,4,1);
        addChild(2,tabBackgrounds[tab]);
        tabLabels[tab]=newChar(12,tabNames[tab],0x2104,
            48+tab*56+27-getTxtLength(tabNames[tab])*4,KEYBOARD_TABS_TOP+3);
        addChild(3,tabLabels[tab]);
    }
    for(unsigned key=0;key<KEYBOARD_KEYS;++key){
        keyLabels[key]=newChar(12,keyText[key],0x2104,0,0);
        addChild(5,keyLabels[key]);
    }
    for(unsigned action=0;action<WIFI_KEYBOARD_ACTIONS;++action){
        actionBackgrounds[action]=newRoundRectDisObj(0xE71C,78,KEYBOARD_ACTION_HEIGHT,
            WS_UI_LEFT+action*WS_UI_KEY_WIDTH+1,KEYBOARD_ACTIONS_TOP,4,1);
        addChild(6,actionBackgrounds[action]);
        actionLabels[action]=newChar(12,actionNames[action],0x2104,
            WS_UI_LEFT+action*WS_UI_KEY_WIDTH+40-getTxtLength(actionNames[action])*4,
            KEYBOARD_ACTIONS_TOP+2);
        addChild(7,actionLabels[action]);
    }
    buildKeyBackgrounds();
    refreshKeyboard();
    setKeyFun(KM3,&previousKey);
    setKeyFun(KM2,&activateKey);
    setKeyFun(KM1,&nextKey);
    setKeyFun(KML,&leaveInput);
    setKeyFun(KMR,&nextCategory);
}

void renderInputtxtInit(void){ inputPageInit(NULL,0); }
