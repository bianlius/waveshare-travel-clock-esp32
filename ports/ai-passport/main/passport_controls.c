#include "config.h"
#include "GVIEW/gview.h"
#include "rom.h"
#include "sound.h"
#include "PAGE/pageDraw.h"
#include "passport_input.h"
extern void doKeyFun(KeyName);
void passport_dispatch(int action){
    if(action<0 || action>5)return;
    backHomeTime=120;
    if(aClockTimes){aClockTimes=0;soundStopAll();hideTips();return;}
    bool held=action>=3;unsigned slot=action%3;
    if(slot==1 && held){renderPowerMenuInit();return;}
    int key=slot==0?2:slot==2?0:1;
    if(held)key=slot==0?3:4;
    else if(softKeyHeaderVisible && slot!=1)key=pageUIKeyAction(slot);
    static const KeyName mapping[]={KM1,KM2,KM3,KML,KMR};
    if(key>=0 && key<5)doKeyFun(mapping[key]);
}
