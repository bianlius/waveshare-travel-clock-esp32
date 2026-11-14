
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
// Use every frame and its anchor so large wings stay inside the viewport.
static void followerExtents(DisplayObject *follower,short *left,short *right,short *top,short *bottom){
    *left=*right=(follower->width+1)/2;
    *top=*bottom=(follower->height+1)/2;
    if(!follower->ac || !follower->ac->p) return;
    const u8 *p=follower->ac->p;
    for(u8 frame=0;frame<p[1];frame++){
        const u8 *desc=p+2+frame*7;
        const short w=(desc[0]<<8)|desc[1],h=(desc[2]<<8)|desc[3];
        if(!h) continue; // Alias frames share another frame's dimensions/anchor.
        const short dx=(short)desc[4]-127,dy=(short)desc[5]-127;
        const short l=(w+1)/2-dx,r=(w+1)/2+dx;
        const short t=(h+1)/2-dy,b=(h+1)/2+dy;
        if(l>*left) *left=l;
        if(r>*right) *right=r;
        if(t>*top) *top=t;
        if(b>*bottom) *bottom=b;
    }
}

void pkmMovieEx1(u8 index){
    if(index>=5 || !pkmMvProp[index].disOb) return;
    if(pkmMvProp[index].nextTime>0){
        --pkmMvProp[index].nextTime;
        return;
    }
    DisplayObject *follower=pkmMvProp[index].disOb;
    // Leave four pixels for the home's slow horizontal scene drift.
    short left,right,top,bottom;
    followerExtents(follower,&left,&right,&top,&bottom);
    short minX=Ex1Min, maxX=Ex1Max;
    if(minX<WS_UI_LEFT+4+left) minX=WS_UI_LEFT+4+left;
    if(maxX>WS_UI_RIGHT-4-right) maxX=WS_UI_RIGHT-4-right;
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
    if(index>=5 || !pkmMvProp[index].disOb) return;
    if(pkmMvProp[index].nextTime>0){
        pkmMvProp[index].nextTime--;
    }else{
        timeOffset = (float)(random()%30)/10;
        short moveRange;
        if(pkmMvProp[index].disOb->x>winW/2){
            moveRange = random()%100-100+160;
        }else{
            moveRange = random()%100+160;
        }
        DisplayObject *follower=pkmMvProp[index].disOb;
        short left,right,top,bottom;
        followerExtents(follower,&left,&right,&top,&bottom);
        const short minX=WS_UI_LEFT+4+left;
        const short maxX=WS_UI_RIGHT-4-right;
        if(minX>maxX) moveRange=WS_UI_MID;
        else{
            if(moveRange<minX) moveRange=minX;
            if(moveRange>maxX) moveRange=maxX;
        }
        // These flight loops keep a steady cadence in both directions.
        if(follower->ac->p==followerWingull || follower->ac->p==followerMetang || follower->ac->p==followerTropius || follower->x>moveRange){
            follower->ac->frameMultiple = 1;
        }else{
            follower->ac->frameMultiple = 0.5;
        }
        Tween(&follower->x,2+timeOffset,moveRange,0,fs_Sine_EaseInOut);
        // Translate flight targets with the character and ground, as at creation.
        short targetY=random()%100-20+yOffset;
        const short minY=4+top,maxY=WS_UI_FOOTER_TOP-4-bottom;
        if(minY>maxY) targetY=WS_UI_FOOTER_TOP/2;
        else{
            if(targetY<minY) targetY=minY;
            if(targetY>maxY) targetY=maxY;
        }
        Tween(&follower->y,2+timeOffset,targetY,0,fs_Sine_EaseInOut);
        pkmMvProp[index].nextTime = fps*(2+timeOffset);//下次移动时间
    }
}