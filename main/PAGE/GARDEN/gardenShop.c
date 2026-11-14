#include "../../rom.h"
#include "../HOME/home.h"
#include "../SHOP/shop.h"
#include "../SHOP/shop_model.h"
#include "gardenShop.h"
#include <stdio.h>

const char* ShopListGardenTxt[] = {"三叶草","四叶草","菊花","铃兰","秋牡丹","三色堇","向日葵","小野菊","红玫瑰","樱花","薰衣草"};
const u16 ShopListGardenHarvest[] = {10,20,50,100,150,200,200,220,250,280,300};//收获
const u8* gardenListArr[] = {Clover1,Clover2,garden_A,garden_B,garden_C,garden_D,garden_I,garden_H,garden_F,garden_E,garden_G};
#define PLANT_COUNT (sizeof(ShopListGardenTxt)/sizeof(ShopListGardenTxt[0]))
static u8 itemIndex;
static char itemText[40], counterText[16], priceText[24], valueText[48];
static DisplayObject *itemLabel;

u8 getGardenShopListArrNum(u8 category){ return category == gardenShopNum ? PLANT_COUNT : 0; }
u16 getGardenPlantMoney(u8 type){ return type < PLANT_COUNT ? ShopListGardenHarvest[type] : 0; }
static u16 gardenPlantCost(u8 type){ return getGardenPlantMoney(type) * 10; }

static void refreshGardenShop(void){
    hideTips();
    removeAllLayerChild(5);
    int owned = checkIsUnlock(gardenSaveInfo.itemGarden,itemIndex);
    snprintf(itemText,sizeof(itemText),"%s",ShopListGardenTxt[itemIndex]);
    snprintf(counterText,sizeof(counterText),"%u/%u",itemIndex+1,(unsigned)PLANT_COUNT);
    snprintf(priceText,sizeof(priceText),owned ? "已解锁" : "花费 %u",gardenPlantCost(itemIndex));
    snprintf(valueText,sizeof(valueText),"每次收获 %u 金币",ShopListGardenHarvest[itemIndex]);
    itemLabel->x = 160-getTxtLength(itemText)*4;
    moneyTxtSp->num = dataSaveInfo.money;
    drawPageUITxt("上一项",!owned && dataSaveInfo.money>=gardenPlantCost(itemIndex) ? "解锁" : "","下一项");
    DisplayObject *plant = getNewMvAction(gardenListArr[itemIndex],0,0);
    placePixelPreview(plant,48,78,224,106);
    addChild(5,plant);
}
static void previousItem(void){ itemIndex=(itemIndex+PLANT_COUNT-1)%PLANT_COUNT; refreshGardenShop(); }
static void nextItem(void){ itemIndex=(itemIndex+1)%PLANT_COUNT; refreshGardenShop(); }
static void confirmItem(void){
    uint32_t balance = dataSaveInfo.money;
    ShopActionResult result = shopApply(&gardenSaveInfo.itemGarden,NULL,itemIndex,
                                       gardenPlantCost(itemIndex),&balance);
    if(result == SHOP_NOT_ENOUGH){ showTips("金币不足",1); return; }
    if(result == SHOP_ALREADY_OWNED){ showTips("已解锁,可在花园生长",1); return; }
    if(result != SHOP_UNLOCKED) return;
    if(result == SHOP_UNLOCKED) dataSaveInfo.money = balance;
    writeData();
    refreshGardenShop();
    showTips("解锁成功",1);
}
static void leaveGardenShop(void){ renderGardenInit(); }
static void gardenShopEx(void){
    setPageUIMiddleText(!checkIsUnlock(gardenSaveInfo.itemGarden,itemIndex) &&
        dataSaveInfo.money>=gardenPlantCost(itemIndex) ? "解锁" : "");
    rendWin(0,0,winW,winH,BGColor);
}

void renderGardenShopInit(void){
    pageNow = PAGE_GARDENSHOP;
    BGColor = 0xFFFF;
    resetPageView(&gardenShopEx);
    setPageUILongKeyText("返回",NULL);
    itemIndex = gardenSaveInfo.gardenIndex < PLANT_COUNT ? gardenSaveInfo.gardenIndex : 0;
    itemLabel = newChar(12,itemText,0,48,32);
    addChild(1,itemLabel);
    addChild(1,newChar(12,counterText,GRAY,232,32));
    addChild(1,newPic(&money,48,57));
    moneyTxtSp = newNumber(12,dataSaveInfo.money,0,0,68,56);
    addChild(1,moneyTxtSp);
    addChild(1,newChar(12,priceText,BLUE,188,56));
    addChild(1,newChar(12,valueText,0,WS_UI_LEFT+4,188));
    addChild(1,newChar(12,"解锁后随机生长",GRAY,WS_UI_LEFT+4,204));
    refreshGardenShop();
    setKeyFun(KM3,&previousItem);
    setKeyFun(KM2,&confirmItem);
    setKeyFun(KM1,&nextItem);
    setKeyFun(KML,&leaveGardenShop);
    setKeyFun(KMR,&leaveGardenShop);
}
