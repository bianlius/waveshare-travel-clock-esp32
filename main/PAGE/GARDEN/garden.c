#include "garden.h"
#include "../SHOP/shop.h"
#include "gardenShop.h"
#include "../../rom.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "esp_random.h"
#include "freertos/semphr.h"
#include "nvs.h"

typedef struct Clover { short x,y; u8 type; } Clover;
u8 cloverNumNow;
Clover cloverArr[cloverMaxNum];
static u8 selectedPlant;
static DisplayObject *selection;
static char plantInfo[48];
enum { GARDEN_FLOOR_WIDTH=224, GARDEN_FLOOR_HEIGHT=98, GARDEN_FLOOR_TOP=115 };

#define GARDEN_STORAGE_MAGIC 0x4752444Eu
#define GARDEN_STORAGE_VERSION 2
#define GARDEN_PREVIOUS_STORAGE_VERSION 1
#define GARDEN_PREVIOUS_MAX_PLANTS 20
#define GARDEN_NVS_NAMESPACE "garden"
#define GARDEN_NVS_KEY "plants"

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t count;
    Clover plants[cloverMaxNum];
} GardenSnapshot;
typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t count;
    Clover plants[GARDEN_PREVIOUS_MAX_PLANTS];
} GardenSnapshotV1;

static SemaphoreHandle_t gardenMutex;
static bool repairPlantPositionsLocked(void);

static void lockGarden(void){
    if(gardenMutex) xSemaphoreTake(gardenMutex,portMAX_DELAY);
}
static void unlockGarden(void){
    if(gardenMutex) xSemaphoreGive(gardenMutex);
}

static bool validGardenPlants(const Clover *plants,u8 count){
    const u8 typeCount=getGardenShopListArrNum(gardenShopNum);
    for(u8 i=0;i<count;++i){
        const Clover *plant=&plants[i];
        if(plant->type>=typeCount || plant->x<0 || plant->x>=320 ||
           plant->y<-128 || plant->y>=240) return false;
    }
    return true;
}
static bool validGardenSnapshot(const GardenSnapshot *snapshot){
    return snapshot->magic==GARDEN_STORAGE_MAGIC &&
           snapshot->version==GARDEN_STORAGE_VERSION &&
           snapshot->count<=cloverMaxNum &&
           validGardenPlants(snapshot->plants,snapshot->count);
}
static bool validGardenSnapshotV1(const GardenSnapshotV1 *snapshot){
    return snapshot->magic==GARDEN_STORAGE_MAGIC &&
           snapshot->version==GARDEN_PREVIOUS_STORAGE_VERSION &&
           snapshot->count<=GARDEN_PREVIOUS_MAX_PLANTS &&
           validGardenPlants(snapshot->plants,snapshot->count);
}

// Called with gardenMutex held so generation and harvest save in order.
static void saveGardenPlantsLocked(void){
#ifdef GVIEW_UI_AUDIT
    return;
#endif
    GardenSnapshot snapshot={.magic=GARDEN_STORAGE_MAGIC,
                             .version=GARDEN_STORAGE_VERSION,
                             .count=cloverNumNow};
    memcpy(snapshot.plants,cloverArr,cloverNumNow*sizeof(cloverArr[0]));
    nvs_handle_t handle;
    esp_err_t err=nvs_open(GARDEN_NVS_NAMESPACE,NVS_READWRITE,&handle);
    if(err!=ESP_OK){ printf("garden save open failed: %d\n",err); return; }
    err=nvs_set_blob(handle,GARDEN_NVS_KEY,&snapshot,sizeof(snapshot));
    if(err==ESP_OK) err=nvs_commit(handle);
    if(err!=ESP_OK) printf("garden save failed: %d\n",err);
    nvs_close(handle);
}

void gardenRestorePlants(void){
    if(!gardenMutex) gardenMutex=xSemaphoreCreateMutex();
    if(!gardenMutex) printf("garden mutex allocation failed\n");
#ifdef GVIEW_UI_AUDIT
    return; // Keep visual-audit scenes independent of private saved data.
#endif
    nvs_handle_t handle;
    if(nvs_open(GARDEN_NVS_NAMESPACE,NVS_READONLY,&handle)!=ESP_OK) return;
    size_t length=0;
    esp_err_t err=nvs_get_blob(handle,GARDEN_NVS_KEY,NULL,&length);
    GardenSnapshot snapshot={0};
    bool previous=false;
    if(err==ESP_OK && length==sizeof(snapshot)){
        err=nvs_get_blob(handle,GARDEN_NVS_KEY,&snapshot,&length);
        if(err!=ESP_OK || length!=sizeof(snapshot) || !validGardenSnapshot(&snapshot))
            err=ESP_FAIL;
    }else if(err==ESP_OK && length==sizeof(GardenSnapshotV1)){
        GardenSnapshotV1 oldSnapshot;
        err=nvs_get_blob(handle,GARDEN_NVS_KEY,&oldSnapshot,&length);
        if(err==ESP_OK && length==sizeof(oldSnapshot) && validGardenSnapshotV1(&oldSnapshot)){
            snapshot.count=oldSnapshot.count;
            memcpy(snapshot.plants,oldSnapshot.plants,oldSnapshot.count*sizeof(Clover));
            previous=true;
        }else err=ESP_FAIL;
    }else err=ESP_FAIL;
    nvs_close(handle);
    if(err!=ESP_OK) return;
    lockGarden();
    memcpy(cloverArr,snapshot.plants,snapshot.count*sizeof(cloverArr[0]));
    cloverNumNow=snapshot.count;
    selectedPlant=0;
    const bool repaired=repairPlantPositionsLocked();
    if(previous || repaired) saveGardenPlantsLocked();
    unlockGarden();
}

void gardenClearPlants(void){
    lockGarden();
    cloverNumNow=0;
    selectedPlant=0;
#ifdef GVIEW_UI_AUDIT
    unlockGarden();
    return;
#endif
    nvs_handle_t handle;
    esp_err_t err=nvs_open(GARDEN_NVS_NAMESPACE,NVS_READWRITE,&handle);
    if(err==ESP_OK){
        err=nvs_erase_key(handle,GARDEN_NVS_KEY);
        if(err==ESP_OK) err=nvs_commit(handle);
        if(err!=ESP_OK && err!=ESP_ERR_NVS_NOT_FOUND)
            printf("garden clear failed: %d\n",err);
        nvs_close(handle);
    }else{
        printf("garden clear open failed: %d\n",err);
    }
    unlockGarden();
}
// Plant roots stay within the original-style green planting surface.

static short plantRootOffset(u8 type){
    const u8 *pic=gardenListArr[type];
    short bottom=0;
    for(unsigned frame=0;frame<pic[1];++frame){
        unsigned offset=frame*7;
        unsigned dimensions=(pic[4+offset] || pic[5+offset]) ? offset : (pic[3+offset]-1)*7;
        short height=(pic[4+dimensions]<<8)|pic[5+dimensions];
        short edge=(short)pic[7+offset]-127+(height-1)/2;
        if(edge>bottom) bottom=edge;
    }
    return bottom;
}

void refrushClover(void){
    Clover plants[cloverMaxNum];
    lockGarden();
    const u8 count=cloverNumNow;
    memcpy(plants,cloverArr,count*sizeof(plants[0]));
    unlockGarden();
    removeAllLayerChild(6);
    for(u8 i=0;i<count;i++){
        DisplayObject *plant=getNewMvAction(gardenListArr[plants[i].type],plants[i].x,plants[i].y);
        addChild(6,plant);
        plant->ac->frameTime = random()%8;
    }
}

// Thirty root positions stay on the green lawn. The original twenty retain
// their coordinates so existing gardens do not jump during the upgrade.
typedef struct { short x,rootY; } GardenSpot;
static const GardenSpot gardenSpots[] = {
    {105,137},{127,137},{149,137},{171,137},{193,137},{215,137},
    {83,158},{105,158},{127,158},{149,158},
    {171,158},{193,158},{215,158},{237,158},
    {110,179},{130,179},{150,179},{170,179},{190,179},{210,179},
    {82,137},{236,137},{70,173},{82,187},{236,187},
    {140,197},{202,197},{120,199},{160,199},{182,199},
};
_Static_assert(sizeof(gardenSpots)/sizeof(gardenSpots[0])==cloverMaxNum,
               "Garden needs exactly 30 planting spots");

static bool gardenSpotOccupied(const Clover *plants,u8 count,u8 spot){
    for(u8 i=0;i<count;i++){
        if(plants[i].x==gardenSpots[spot].x &&
           plants[i].y+plantRootOffset(plants[i].type)==gardenSpots[spot].rootY)
            return true;
    }
    return false;
}

static bool chooseRandomFreeGardenSpot(const Clover *plants,u8 count,
                                       short *outX,short *outRootY){
    u8 freeSpots[cloverMaxNum],freeCount=0;
    for(u8 i=0;i<cloverMaxNum;i++)
        if(!gardenSpotOccupied(plants,count,i)) freeSpots[freeCount++]=i;
    if(!freeCount) return false;
    const GardenSpot spot=gardenSpots[freeSpots[esp_random()%freeCount]];
    *outX=spot.x;
    *outRootY=spot.rootY;
    return true;
}

static bool chooseNearestFreeGardenSpot(const Clover *plants,u8 count,
                                        short oldX,short oldRootY,
                                        short *outX,short *outRootY){
    int bestDistance=INT_MAX;
    bool found=false;
    for(u8 i=0;i<cloverMaxNum;i++){
        if(gardenSpotOccupied(plants,count,i)) continue;
        const int dx=oldX-gardenSpots[i].x;
        const int dy=oldRootY-gardenSpots[i].rootY;
        const int distance=dx*dx+dy*dy;
        if(distance<bestDistance){
            bestDistance=distance;
            *outX=gardenSpots[i].x;
            *outRootY=gardenSpots[i].rootY;
            found=true;
        }
    }
    return found;
}

// Draw from the rear root to the front root, regardless of sprite height.
void bubbleSortPointsByY(Clover* points,int size){
    for(int i=0;i<size-1;i++){
        bool swapped=false;
        for(int j=0;j<size-1-i;j++){
            const int frontRoot=points[j].y+plantRootOffset(points[j].type);
            const int backRoot=points[j+1].y+plantRootOffset(points[j+1].type);
            if(frontRoot>backRoot){
                Clover tmp=points[j];
                points[j]=points[j+1];
                points[j+1]=tmp;
                swapped=true;
            }
        }
        if(!swapped) break;
    }
}

// Preserve plants and types from older snapshots while spreading close roots.
static bool repairPlantPositionsLocked(void){
    Clover original[cloverMaxNum], placed[cloverMaxNum];
    memcpy(original,cloverArr,cloverNumNow*sizeof(original[0]));
    for(u8 i=0;i<cloverNumNow;i++){
        Clover plant=original[i];
        short x,rootY;
        if(chooseNearestFreeGardenSpot(placed,i,plant.x,
                                      plant.y+plantRootOffset(plant.type),
                                      &x,&rootY)){
            plant.x=x;
            plant.y=rootY-plantRootOffset(plant.type);
        }
        placed[i]=plant;
    }
    bubbleSortPointsByY(placed,cloverNumNow);
    bool changed=false;
    for(u8 i=0;i<cloverNumNow;i++){
        if(placed[i].x!=original[i].x || placed[i].y!=original[i].y ||
           placed[i].type!=original[i].type) changed=true;
    }
    if(changed) memcpy(cloverArr,placed,cloverNumNow*sizeof(cloverArr[0]));
    return changed;
}

//生长一颗新植物
void addCloverToFloor(void){
    lockGarden();
    if(cloverNumNow>=cloverMaxNum){ unlockGarden(); return; }
    short px,rootY;
    if(!chooseRandomFreeGardenSpot(cloverArr,cloverNumNow,&px,&rootY)){
        unlockGarden();
        return;
    }
    Clover plant={.x=px};
    //植物类型
    u16 typeTemp = random()%1100;

        //低概率得到珍贵花朵
        if(typeTemp < getGardenShopListArrNum(gardenShopNum)-2){
            plant.type = typeTemp + 2;
        }else
        //按一定概率开出对应的花
        if(typeTemp > 1000 && typeTemp < 1100 && (gardenSaveInfo.itemGarden >> 10&0x1)){
            plant.type = 10;
        }else if(typeTemp >900 && typeTemp < 1000 && (gardenSaveInfo.itemGarden >> 9&0x1)){
            plant.type = 9;
        }else if(typeTemp >800 && typeTemp < 900 && (gardenSaveInfo.itemGarden >> 8&0x1)){
            plant.type = 8;
        }else if(typeTemp >700 && typeTemp < 800 && (gardenSaveInfo.itemGarden >> 7&0x1)){
            plant.type = 7;
        }else if(typeTemp > 600 && typeTemp < 700 && (gardenSaveInfo.itemGarden >> 6&0x1)){
            plant.type = 6;
        }else if(typeTemp > 500 && typeTemp < 600 && (gardenSaveInfo.itemGarden >> 5&0x1)){
            plant.type = 5;
        }else if(typeTemp > 400 && typeTemp < 500 && (gardenSaveInfo.itemGarden >> 4&0x1)){
            plant.type = 4;
        }else if(typeTemp > 300 && typeTemp < 400 && (gardenSaveInfo.itemGarden >> 3&0x1)){
            plant.type = 3;
        }else if(typeTemp > 200 && typeTemp < 300 && (gardenSaveInfo.itemGarden >> 2&0x1)){
            plant.type = 2;
        }else if(typeTemp > 100 && typeTemp < 200 && (gardenSaveInfo.itemGarden >> 1&0x1)){
            plant.type = 1;
        }else{
            plant.type = random()%10>9?1:0;
        }
        
    plant.y=rootY-plantRootOffset(plant.type);
    cloverArr[cloverNumNow++]=plant;
    bubbleSortPointsByY(cloverArr,cloverNumNow);
    saveGardenPlantsLocked();
    unlockGarden();
    if(pageNow==PAGE_GARDEN) refrushClover();
}

static void updateGardenSelection(void){
    lockGarden();
    const u8 count=cloverNumNow;
    if(selectedPlant >= count) selectedPlant=count ? count-1 : 0;
    const u8 selected=selectedPlant;
    Clover plant={0};
    if(count) plant=cloverArr[selected];
    unlockGarden();
    setPageUIKeyText(0,count ? "上一株" : "");
    setPageUIKeyText(2,count ? "下一株" : "");
    if(!count){
        selection->visible=0;
        snprintf(plantInfo,sizeof(plantInfo),"植物正在生长");
        setPageUIMiddleText("");
        return;
    }
    selection->visible=1;
    selection->x=plant.x-24;
    selection->y=plant.y+plantRootOffset(plant.type)+2;
    snprintf(plantInfo,sizeof(plantInfo),"%u/%u  收获 %u 金币",selected+1,count,
             getGardenPlantMoney(plant.type));
    setPageUIMiddleText("收获");
}
static void previousPlant(void){
    lockGarden();
    if(cloverNumNow) selectedPlant=(selectedPlant+cloverNumNow-1)%cloverNumNow;
    unlockGarden();
}
static void nextPlant(void){
    lockGarden();
    if(cloverNumNow) selectedPlant=(selectedPlant+1)%cloverNumNow;
    unlockGarden();
}
static void harvestPlant(void){
    lockGarden();
    if(!cloverNumNow){ unlockGarden(); return; }
    if(selectedPlant >= cloverNumNow) selectedPlant=cloverNumNow-1;
    dataSaveInfo.money += getGardenPlantMoney(cloverArr[selectedPlant].type);
    memmove(&cloverArr[selectedPlant],&cloverArr[selectedPlant+1],
            (cloverNumNow-selectedPlant-1)*sizeof(cloverArr[0]));
    --cloverNumNow;
    saveGardenPlantsLocked();
    writeData();
    unlockGarden();
    refrushClover();
    moneyTxtSp->num=dataSaveInfo.money;
}
static void leaveGarden(void){ backToHomePage(); }
static void enterGardenShop(void){ renderGardenShopInit(); }
static void gardenEx(void){
    updateGardenSelection();
    moneyTxtSp->num=dataSaveInfo.money;
    rendWin(0,0,winW,winH,BGColor);
}
void renderGardenInit(void){
    pageNow=PAGE_GARDEN;
    BGColor=0xFFFF;
    resetPageView(&gardenEx);
    selectedPlant=0;
    drawPageUITxt("","收获","");
    setPageUILongKeyText("返回","商店");
    addChild(1,newPic(&money,48,32));
    moneyTxtSp=newNumber(12,dataSaveInfo.money,0,0,68,31);
    addChild(1,moneyTxtSp);
    // The complete original-style lawn is packed at its displayed size; no runtime scaling.
    addChild(0,newNativePic(floorPicWS,WS_UI_MID-GARDEN_FLOOR_WIDTH/2,GARDEN_FLOOR_TOP));
    selection=newRoundRectDisObj(ORANGE,48,4,0,0,2,1);
    addChild(4,selection);
    addChild(8,newChar(12,plantInfo,0,48,54));
    refrushClover();
    setKeyFun(KM3,&previousPlant);
    setKeyFun(KM2,&harvestPlant);
    setKeyFun(KM1,&nextPlant);
    setKeyFun(KML,&leaveGarden);
    setKeyFun(KMR,&enterGardenShop);
}
