#ifndef __SHOP_H_
#define __SHOP_H_

#include "../../config.h"
#include "GVIEW/gview.h"
#include "../pageDraw.h"

#define shopListY 50
#define systemNum ((deviceHd?64:32)/8)

// #define monsterMaxNum 6 //最大可获得怪物数量
extern char itemInfoTxt[30];
extern char costTxt[8];
extern DisplayObject *costTxtSp;//花费
extern DisplayObject *shopListObj;

extern const u8* momsterListArr[];//可选择的怪物列表
extern const u8 ShopListMonsterGround[];
extern const u8* gardenListArr[];
extern const u8* tentListArr[];
extern const u8* shirtListArr[];
extern const u8* headListArr[];
extern const u8* pantsListArr[];
extern const u8* shoesListArr[];
extern const u8* bagListArr[];
extern const u8* glovesListArr[];

u16 getGardenPlantMoney(u8 type);
u8 getShopListArrNum(u8 titleListNum);
u8 checkIsUnlock(uint64_t index,u8 listNum);
#endif

