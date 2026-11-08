#ifndef __GARDEN_H_
#define __GARDEN_H_


#include "../../config.h"
#include "GVIEW/gview.h"
#include "../pageDraw.h"

extern u8 cloverNumNow; //当前生长的四叶草数量
#define cloverMaxNum 20   //最大可同时生长的四叶草数量
void addCloverToFloor(void);//花园增加四叶草
void gardenRestorePlants(void);
void gardenClearPlants(void);
#endif

