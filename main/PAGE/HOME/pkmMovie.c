
#include "./pkmMovie.h"


float timeOffset;
pkmMovieProperty pkmMvProp[5];

//{"不跟随","喷火龙","风速狗","拉蒂奥斯","拉蒂亚斯","烈焰马","鬼斯","炎帝"};
u8 checkMonsterType(u8 index){
    if(index == 2||index == 5||index == 7||index == 9){
        //陆地
        return 1;
    }
    //飞行
    return 0;
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
    //陆地精灵动画处理
    if(pkmMvProp[index].nextTime>0){
        pkmMvProp[index].nextTime--;
    }else{
        timeOffset = (float)(random()%30)/10;
        short middleX = (Ex1Max + Ex1Min) /2;

        u8 wz = random()%10;
        short targetX = 0;
        if(wz>6){
            targetX = Ex1Max;
        }else if(wz>4){
            targetX = (Ex1Max + Ex1Min)/4*3;
        }else if(wz>2){
            targetX = (Ex1Max + Ex1Min)/4;
        }else{
            targetX = Ex1Min;
        }
        //限制单次移动范围
        if(pkmMvProp[index].disOb->x > targetX){
            if(pkmMvProp[index].disOb->x - targetX > 80)targetX = pkmMvProp[index].disOb->x-80;
        }else{
            if(targetX - pkmMvProp[index].disOb->x > 80)targetX = pkmMvProp[index].disOb->x+80;
        }
        Tween(&pkmMvProp[index].disOb->x,2+timeOffset,random()%50-25+targetX,0,fs_Sine_EaseInOut);
        pkmMvProp[index].nextTime = fps*(2+timeOffset);
    }
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