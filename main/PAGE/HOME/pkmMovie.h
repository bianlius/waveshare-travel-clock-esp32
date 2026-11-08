#ifndef __PKMMOVIE_H_
#define __PKMMOVIE_H_

#include "../../config.h"
#include "GVIEW/gview.h"

/*----飞行状态----
*悬浮态
*向前飞行态
*向后飞行态
*/
typedef struct pkmProperty
{
    char *name;
    u8 *pic;
    short maxX;
    short minX;
    u8 style;//0 飞行 1陆地
    u8 FrontBack;//在前面还是后面    
}pkmProperty;


typedef struct pkmMovieProperty{
    struct DisplayObject *disOb;
    short targetx;
    short targety;
    u16 nextTime;//下一次移动时间
    pkmProperty *property;
}pkmMovieProperty;

extern pkmMovieProperty pkmMvProp[5];
void pkmMovieEx(u8 index);
void pkmMovieEx1(u8 index);
void pkmMovieEx2(u8 index,short yOffset);
u8 checkMonsterType(u8 index);

extern short Ex1Max;
extern short Ex1Min;

#endif