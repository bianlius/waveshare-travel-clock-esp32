#ifndef __BACKLIGHT_H_
#define __BACKLIGHT_H_

#include "stdint.h"
#include "../config.h"

extern u8 backLightNum;//当前屏幕背光等级
void backLightInit(void);
void updateBackLight(u8 num); 
void setBackLightVal(u8 num);
void resetBackLightVal(void);
void backLightScheduleTick(void);
#endif
