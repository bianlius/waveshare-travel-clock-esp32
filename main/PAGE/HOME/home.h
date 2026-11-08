
#ifndef __HOME_H_
#define __HOME_H_

#include "../../NET/connect.h"
#include "../../NET/RTC_Internet.h"
#include "GVIEW/gview.h"
#include "../../config.h"
#include "./../pageDraw.h"
#include "../../pic_data/picGroup.h"
#include "../../sound.h"
#include "../../LCD/backLight.h"
#include "./pkmMovie.h"



void mainViewOpenShop(void);
void mainViewOpenGarden(void);

void checkDayTime(void);
void refreshWeatherDisplay(void);
void changeSceneColor(void);
void addManBike(u8 deep,short $x,short $y,u8 bikeNum,u8 headNum,u8 shirtNum,u8 pantsNum,u8 shoesNum,u8 glovesNum,u8 bagNum);
void setBikeColor(u8 num);
extern char tempNetTxt[10] ;
extern char weatherTxt[20];
extern DisplayObject* weatherObj;
extern char timeString[10];
extern u8* weatherArr[40];
extern DisplayObject *timeDotObj[2];
extern DisplayObject* bikeman;
#endif /* LCD_VIEW_H_ */
