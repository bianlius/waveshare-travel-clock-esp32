#include "shop.h"
#include "shop_model.h"
#include "../../rom.h"
#include "../HOME/home.h"
#include <stdio.h>

char itemInfoTxt[30];
char costTxt[8];
DisplayObject *costTxtSp;
DisplayObject *shopListObj;

const char* ShopListMonsterTxt[] = {"不跟随","喷火龙","风速狗","拉蒂奥斯","拉蒂亚斯","烈焰马","鬼斯","炎帝","沙漠蜻蜓","比卡丘"};//可选择的怪物列表
const u16 ShopListMonsterCost[] = {0,200,200,200,500,500,200,1000,800,1800};//可选择的怪物列表
const u8* momsterListArr[] = {PHL_M,FSG_M,LDYS_M,pkm_ldys2,LYM_M,GS_M,YD_M,SMQT_M,BKQ_M};//可选择的怪物列表
const pkmProperty property[] = {
    {.pic = PHL_M,.maxX = 240,.minX = 60,.style = 1,.FrontBack = 1},//喷火龙
};


const char* ShopListTentTxt[2] = {"睡袋","普通帐篷"};
const u16 ShopListTentCost[2] = {0,800};
const u8* tentListArr[] = {sleepBag,house};

const char* ShopListBikeTxt[9] = {"朱红自行车","黄绿自行车","紫色自行车","蔚蓝自行车","橙色自行车","青色自行车","粉色自行车","草绿自行车","黄色自行车"};
const u16 ShopListBikeCost[9] = {0,150,150,150,150,150,150,150,150};

const char* ShopListHeadTxt[3] = {"无","头巾","鸭舌帽"};
const u16 ShopListHeadCost[3] = {0,30,50};
const u8* headListArr[] = {head_A,head_B};

const char* ShopListShirtTxt[3] = {"无","衬衫","羽绒"};
const u16 ShopListShirtCost[3] = {0,50,280};
const u8* shirtListArr[] = {Tshirt_A,Tshirt_B};

const char* ShopListPantsTxt[3] = {"无","长裤","短裤"};
const u16 ShopListPantsCost[3] = {0,160,120};
const u8* pantsListArr[] = {Pants_A,Pants_B};

const char* ShopListShoesTxt[5] = {"无","灰色运动鞋","蓝色运动鞋","白色运动鞋","红色运动鞋"};
const u16 ShopListShoesCost[5] = {0,150,150,150,150};
const u8* shoesListArr[] = {shoes_A,shoes_A,shoes_A,shoes_A};

const char* ShopListGlovesTxt[4] = {"无","白色手套","灰色手套","红色手套"};
const u16 ShopListGlovesCost[4] = {0,80,80,80};
const u8* glovesListArr[] = {gloves_A,gloves_A,gloves_A};

const char* ShopListBagTxt[4] = {"无","普通背包","休闲背包","旅行背包"};
const u16 ShopListBagCost[4] = {0,80,180,380};
const u8* bagListArr[] = {Bag_A,Bag_B,Bag_C};

#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
typedef struct {
    const char *title;
    const char *const *names;
    const u16 *costs;
    uint64_t *owned;
    u8 *equipped;
    u8 count;
} ShopCategory;

static const ShopCategory categories[] = {
    {"宝可梦",ShopListMonsterTxt,ShopListMonsterCost,&itemSaveInfo.itemMonster,&itemSaveInfo.monsterIndex,COUNT(ShopListMonsterTxt)},
    {"帐篷",ShopListTentTxt,ShopListTentCost,&itemSaveInfo.itemTent,&itemSaveInfo.tentIndex,COUNT(ShopListTentTxt)},
    {"自行车",ShopListBikeTxt,ShopListBikeCost,&itemSaveInfo.itemBike,&itemSaveInfo.bikeIndex,COUNT(ShopListBikeTxt)},
    {"头饰",ShopListHeadTxt,ShopListHeadCost,&itemSaveInfo.itemHead,&itemSaveInfo.headIndex,COUNT(ShopListHeadTxt)},
    {"上衣",ShopListShirtTxt,ShopListShirtCost,&itemSaveInfo.itemShirt,&itemSaveInfo.shirtIndex,COUNT(ShopListShirtTxt)},
    {"裤子",ShopListPantsTxt,ShopListPantsCost,&itemSaveInfo.itemPants,&itemSaveInfo.pantsIndex,COUNT(ShopListPantsTxt)},
    {"鞋子",ShopListShoesTxt,ShopListShoesCost,&itemSaveInfo.itemShoes,&itemSaveInfo.shoesIndex,COUNT(ShopListShoesTxt)},
    {"手套",ShopListGlovesTxt,ShopListGlovesCost,&itemSaveInfo.itemGloves,&itemSaveInfo.glovesIndex,COUNT(ShopListGlovesTxt)},
    {"背包",ShopListBagTxt,ShopListBagCost,&itemSaveInfo.itemBag,&itemSaveInfo.bagIndex,COUNT(ShopListBagTxt)},
};
static u8 categoryIndex, itemIndex;
static char categoryText[32], itemText[40], priceText[24], counterText[16];
static DisplayObject *itemLabel;

u8 getShopListArrNum(u8 category){
    return category < COUNT(categories) ? categories[category].count : 0;
}
u8 checkIsUnlock(uint64_t owned,u8 item){
    return item < 64 && (owned & (UINT64_C(1) << item)) != 0;
}

static void refreshShop(void){
    const ShopCategory *category = &categories[categoryIndex];
    const int owned = checkIsUnlock(*category->owned,itemIndex);
    const int equipped = *category->equipped == itemIndex;
    hideTips();
    removeAllLayerChild(5);
    snprintf(categoryText,sizeof(categoryText),"%s %u/%u",category->title,categoryIndex+1,(unsigned)COUNT(categories));
    snprintf(counterText,sizeof(counterText),"%u/%u",itemIndex+1,category->count);
    snprintf(itemText,sizeof(itemText),"%s",category->names[itemIndex]);
    snprintf(priceText,sizeof(priceText),owned ? (equipped ? "使用中" : "已解锁") : "花费 %u",category->costs[itemIndex]);
    itemLabel->x = 160-getTxtLength(itemText)*4;
    moneyTxtSp->num = dataSaveInfo.money;
    drawPageUITxt("上一项",!owned ? (dataSaveInfo.money>=category->costs[itemIndex] ? "解锁" : "") : equipped ? "" : categoryIndex == 0 ? "跟随" : "使用","下一项");
    if(categoryIndex == 0){
        if(itemIndex){
            DisplayObject *preview = getNewMvAction(momsterListArr[itemIndex-1],0,0);
            placePixelPreview(preview,48,74,224,120);
            addChild(5,preview);
        }
    }else if(categoryIndex == 1){
        DisplayObject *preview = newPic(tentListArr[itemIndex],0,0);
        placePixelPreview(preview,48,74,224,120);
        addChild(5,preview);
    }else{
        addManBike(5,145,134,
            categoryIndex==2?itemIndex:itemSaveInfo.bikeIndex,
            categoryIndex==3?itemIndex:itemSaveInfo.headIndex,
            categoryIndex==4?itemIndex:itemSaveInfo.shirtIndex,
            categoryIndex==5?itemIndex:itemSaveInfo.pantsIndex,
            categoryIndex==6?itemIndex:itemSaveInfo.shoesIndex,
            categoryIndex==7?itemIndex:itemSaveInfo.glovesIndex,
            categoryIndex==8?itemIndex:itemSaveInfo.bagIndex);
    }
}

static void previousItem(void){
    u8 count = categories[categoryIndex].count;
    itemIndex = (itemIndex+count-1)%count;
    refreshShop();
}
static void nextItem(void){
    itemIndex = (itemIndex+1)%categories[categoryIndex].count;
    refreshShop();
}
static void nextCategory(void){
    categoryIndex = (categoryIndex+1)%COUNT(categories);
    const ShopCategory *category = &categories[categoryIndex];
    itemIndex = *category->equipped < category->count ? *category->equipped : 0;
    refreshShop();
}
static void confirmItem(void){
    const ShopCategory *category = &categories[categoryIndex];
    uint32_t balance = dataSaveInfo.money;
    ShopActionResult result = shopApply(category->owned,category->equipped,itemIndex,
                                        category->costs[itemIndex],&balance);
    if(result == SHOP_NOT_ENOUGH){ showTips("金币不足",1); return; }
    if(result == SHOP_ALREADY_OWNED){ showTips("已在使用",1); return; }
    if(result == SHOP_INVALID) return;
    if(result == SHOP_UNLOCKED) dataSaveInfo.money = balance;
    writeData();
    refreshShop();
    showTips(result == SHOP_UNLOCKED ? "解锁成功" : "设置成功",1);
}
static void leaveShop(void){ backToHomePage(); }
static void shopEx(void){
    const ShopCategory *category=&categories[categoryIndex];
    int owned=checkIsUnlock(*category->owned,itemIndex);
    setPageUIMiddleText(!owned ? (dataSaveInfo.money>=category->costs[itemIndex] ? "解锁" : "") :
        *category->equipped==itemIndex ? "" : categoryIndex==0 ? "跟随" : "使用");
    rendWin(0,0,winW,winH,BGColor);
}

void renderShopInit(void){
    pageNow = PAGE_SHOP;
    BGColor = 0xFFFF;
    resetPageView(&shopEx);
    setPageUILongKeyText("返回","分类");
    categoryIndex = 0;
    itemIndex = itemSaveInfo.monsterIndex < categories[0].count ? itemSaveInfo.monsterIndex : 0;
    addChild(1,newChar(12,categoryText,0,48,30));
    addChild(1,newChar(12,counterText,GRAY,232,30));
    addChild(1,newPic(&money,48,53));
    moneyTxtSp = newNumber(12,dataSaveInfo.money,0,0,68,52);
    addChild(1,moneyTxtSp);
    addChild(1,newChar(12,priceText,BLUE,188,52));
    itemLabel = newChar(12,itemText,0,48,200);
    addChild(1,itemLabel);
    refreshShop();
    setKeyFun(KM3,&previousItem);
    setKeyFun(KM2,&confirmItem);
    setKeyFun(KM1,&nextItem);
    setKeyFun(KML,&leaveShop);
    setKeyFun(KMR,&nextCategory);
}
