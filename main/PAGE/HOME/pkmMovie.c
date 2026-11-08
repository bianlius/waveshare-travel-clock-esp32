
#include "./pkmMovie.h"
#include "../SHOP/shop.h"


float timeOffset;
pkmMovieProperty pkmMvProp[5];

//{"不跟随","喷火龙","风速狗","拉蒂奥斯","拉蒂亚斯","烈焰马","鬼斯","炎帝"};
u8 checkMonsterType(u8 index){
    return index<getShopListArrNum(0) ? ShopListMonsterGround[index] : 0;
}

void pkmMovieEx(u8 index){
    //精灵移动
    if(pkmMvProp[index].nextTime>0){
        pkmMvProp[index].nextTime--;
    }else{
        timeOffset = (float)(random()%30)/10;
        if(pkmMvProp[index].disOb->x>winW/2){
            Tween(&pkmMvProp[index].disOb->x,2+timeOffset,random()%50-50+160,0,fs_Sine_EaseInOut);                
        }else{
            Tween(&pkmMvProp[index].disOb->x,2+timeOffset,random()%50+160,0,fs_Sine_EaseInOut);
        }
        Tween(&pkmMvProp[index].disOb->y,2+timeOffset,random()%50+46,0,fs_Sine_EaseInOut);
        pkmMvProp[index].nextTime = fps*(2+timeOffset);
    }
}
short Ex1Max;
short Ex1Min;
void pkmMovieEx1(u8 index){
    if(index>=5 || !pkmMvProp[index].disOb) return;
    if(pkmMvProp[index].nextTime>0){
        --pkmMvProp[index].nextTime;
        return;
    }
    DisplayObject *follower=pkmMvProp[index].disOb;
    // Leave four pixels for the home's slow horizontal scene drift.
    const short halfWidth=(follower->width+1)/2;
    short minX=Ex1Min, maxX=Ex1Max;
    if(minX<WS_UI_LEFT+4+halfWidth) minX=WS_UI_LEFT+4+halfWidth;
    if(maxX>WS_UI_RIGHT-4-halfWidth) maxX=WS_UI_RIGHT-4-halfWidth;
    if(minX>maxX) minX=maxX=WS_UI_MID;
    short targetX=minX+random()%(maxX-minX+1);
    if(targetX>follower->x+80) targetX=follower->x+80;
    if(targetX<follower->x-80) targetX=follower->x-80;
    if(targetX<minX) targetX=minX;
    if(targetX>maxX) targetX=maxX;
    timeOffset=(float)(random()%30)/10;
    Tween(&follower->x,2+timeOffset,targetX,0,fs_Sine_EaseInOut);
    pkmMvProp[index].nextTime=fps*(2+timeOffset);
}

void pkmMovieEx2(u8 index,short yOffset){
    //飞行精灵动画处理
    if(pkmMvProp->disOb==NULL)return;
    if(pkmMvProp[index].nextTime>0){
        pkmMvProp[index].nextTime--;
    }else{
        timeOffset = (float)(random()%30)/10;
        short moveRange;
        if(pkmMvProp[index].disOb->x>winW/2){
            moveRange = random()%100-100+160;
            Tween(&pkmMvProp[index].disOb->x,2+timeOffset,moveRange,0,fs_Sine_EaseInOut);                
        }else{
            moveRange = random()%100+160;
            Tween(&pkmMvProp[index].disOb->x,2+timeOffset,moveRange,0,fs_Sine_EaseInOut);
        }
        if(pkmMvProp[index].disOb->x>moveRange){
            //向左飞
            pkmMvProp[index].disOb->ac->frameMultiple = 1;
        }else{
            //向右飞
            pkmMvProp[index].disOb->ac->frameMultiple = 0.5;
        }
        // Translate flight targets with the character and ground, as at creation.
        Tween(&pkmMvProp[index].disOb->y,2+timeOffset,random()%100-20+yOffset,0,fs_Sine_EaseInOut);
        pkmMvProp[index].nextTime = fps*(2+timeOffset);//下次移动时间
    }
}