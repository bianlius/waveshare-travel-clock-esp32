"""Exercise the production garden NVS save, restore, and reset paths."""

from pathlib import Path
import re
import subprocess
import tempfile

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
garden = (ROOT / "main/PAGE/GARDEN/garden.c").read_bytes().decode("gb18030")


def function(signature):
    start = garden.index(signature)
    brace = garden.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (garden[end] == "{") - (garden[end] == "}")
        end += 1
    return garden[start:end] + "\n"


layout = garden[garden.index("#define GARDEN_STORAGE_MAGIC"):
                garden.index("static SemaphoreHandle_t gardenMutex;")]
spot_layout = garden[garden.index("typedef struct { short x,rootY; } GardenSpot;"):
                     garden.index("// Draw from the rear root")]
production = spot_layout + "\n" + "\n".join(function(signature) for signature in (
    "static void lockGarden(void){",
    "static void unlockGarden(void){",
    "static bool validGardenPlants(const Clover *plants,u8 count){",
    "static bool validGardenSnapshot(const GardenSnapshot *snapshot){",
    "static bool validGardenSnapshotV1(const GardenSnapshotV1 *snapshot){",
    "static void saveGardenPlantsLocked(void){",
    "void bubbleSortPointsByY(Clover* points,int size){",
    "static bool repairPlantPositionsLocked(void){",
    "void gardenRestorePlants(void){",
    "void gardenClearPlants(void){",
    "void addCloverToFloor(void){",
    "static void harvestPlant(void){",
))

prelude = r'''
#include <assert.h>
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
enum { cloverMaxNum=30, gardenShopNum=0, portMAX_DELAY=1 };
typedef struct Clover { short x,y; u8 type; } Clover;
static Clover cloverArr[cloverMaxNum];
static u8 cloverNumNow, selectedPlant;
static bool repairPlantPositionsLocked(void);
typedef int SemaphoreHandle_t;
static SemaphoreHandle_t gardenMutex;
static int lockDepth;
static int xSemaphoreCreateMutex(void){ return 1; }
static int xSemaphoreTake(int handle,int ticks){
    assert(handle==1 && ticks==portMAX_DELAY && lockDepth==0);
    ++lockDepth; return 1;
}
static int xSemaphoreGive(int handle){
    assert(handle==1 && lockDepth==1);
    --lockDepth; return 1;
}
static u8 getGardenShopListArrNum(u8 category){ assert(category==0); return 11; }
static struct { uint64_t itemGarden; } gardenSaveInfo={1};
static struct { uint32_t money; } dataSaveInfo;
typedef struct { uint32_t num; } MoneyText;
static MoneyText moneyText;
static MoneyText *moneyTxtSp=&moneyText;
enum { WS_UI_LEFT=40, WS_UI_WIDTH=240, PAGE_GARDEN=1 };
static int pageNow, refreshes, dataWrites;
static long fakeRandomValue=10;
static long fakeRandom(void){ return fakeRandomValue; }
static uint32_t esp_random(void){ return (uint32_t)fakeRandomValue; }
#define random fakeRandom
static short plantRootOffset(u8 type){ return type==8 ? 30 : 20; }
static void refrushClover(void){ ++refreshes; }
static uint16_t getGardenPlantMoney(u8 type){ (void)type; return 10; }
static void writeData(void){ ++dataWrites; }
typedef int esp_err_t;
typedef int nvs_handle_t;
enum { ESP_OK=0, ESP_ERR_NVS_NOT_FOUND=1, ESP_FAIL=2,
       NVS_READONLY=0, NVS_READWRITE=1 };
'''

backend = r'''
static GardenSnapshot stored;
static size_t storedLength=sizeof(stored);
static bool present;
static int commits;
static esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *handle){
    assert(strcmp(name,"garden")==0);
    if(mode==NVS_READONLY && !present) return ESP_ERR_NVS_NOT_FOUND;
    *handle=1; return ESP_OK;
}
static esp_err_t nvs_set_blob(nvs_handle_t handle,const char *key,
                              const void *value,size_t length){
    assert(handle==1 && strcmp(key,"plants")==0 && length==sizeof(stored));
    memcpy(&stored,value,length); storedLength=length; present=true; return ESP_OK;
}
static esp_err_t nvs_get_blob(nvs_handle_t handle,const char *key,
                              void *value,size_t *length){
    assert(handle==1 && strcmp(key,"plants")==0);
    if(!present) return ESP_ERR_NVS_NOT_FOUND;
    if(!value){ *length=storedLength; return ESP_OK; }
    assert(*length>=storedLength);
    memcpy(value,&stored,storedLength); *length=storedLength; return ESP_OK;
}
static esp_err_t nvs_erase_key(nvs_handle_t handle,const char *key){
    assert(handle==1 && strcmp(key,"plants")==0);
    if(!present) return ESP_ERR_NVS_NOT_FOUND;
    present=false; return ESP_OK;
}
static esp_err_t nvs_commit(nvs_handle_t handle){ assert(handle==1); ++commits; return ESP_OK; }
static void nvs_close(nvs_handle_t handle){ assert(handle==1); }
'''

scenarios = r'''
static int spotIndex(const Clover *plant){
    const int rootY=plant->y+plantRootOffset(plant->type);
    for(int i=0;i<cloverMaxNum;i++)
        if(plant->x==gardenSpots[i].x && rootY==gardenSpots[i].rootY) return i;
    return -1;
}
static void checkGardenSpots(void){
    bool used[cloverMaxNum]={0};
    for(int i=0;i<cloverNumNow;i++){
        int slot=spotIndex(&cloverArr[i]);
        assert(slot>=0 && !used[slot]);
        used[slot]=true;
        if(i) assert(cloverArr[i-1].y+plantRootOffset(cloverArr[i-1].type)
                     <=cloverArr[i].y+plantRootOffset(cloverArr[i].type));
    }
}
int main(void){
    for(int i=0;i<cloverMaxNum;i++){
        const int x=gardenSpots[i].x, y=gardenSpots[i].rootY;
        assert(x>=70 && x<=244 && y>=130 && y<=200);
        for(int j=0;j<i;j++){
            int px=x-gardenSpots[j].x,py=y-gardenSpots[j].rootY;
            assert(px*px+py*py>=18*18);
        }
    }
    gardenRestorePlants();
    assert(gardenMutex==1 && cloverNumNow==0);

    cloverNumNow=2;
    cloverArr[0]=(Clover){100,120,0};
    cloverArr[1]=(Clover){180,142,8};
    lockGarden(); saveGardenPlantsLocked(); unlockGarden();
    assert(present && stored.count==2 && commits==1);
    cloverNumNow=0;
    memset(cloverArr,0,sizeof(cloverArr));
    gardenRestorePlants();
    assert(cloverNumNow==2 && commits==2);
    checkGardenSpots();
    assert((cloverArr[0].type==0 && cloverArr[1].type==8) ||
           (cloverArr[0].type==8 && cloverArr[1].type==0));
    int stableCommits=commits;
    gardenRestorePlants(); assert(commits==stableCommits);
    GardenSnapshotV1 previous={.magic=GARDEN_STORAGE_MAGIC,
                               .version=GARDEN_PREVIOUS_STORAGE_VERSION,.count=2};
    memcpy(previous.plants,cloverArr,2*sizeof(Clover));
    Clover previousPlants[2]={cloverArr[0],cloverArr[1]};
    memset(&stored,0,sizeof(stored));
    memcpy(&stored,&previous,sizeof(previous));
    storedLength=sizeof(previous);
    cloverNumNow=0;
    gardenRestorePlants();
    assert(cloverNumNow==2 && commits==stableCommits+1);
    assert(storedLength==sizeof(GardenSnapshot) && stored.version==GARDEN_STORAGE_VERSION);
    assert(memcmp(cloverArr,previousPlants,sizeof(previousPlants))==0);
    checkGardenSpots();
    stableCommits=commits;
    stored.plants[1]=stored.plants[0];
    stored.plants[1].type=8;
    stored.plants[1].y+=plantRootOffset(stored.plants[0].type)-
                         plantRootOffset(stored.plants[1].type);
    gardenRestorePlants();
    assert(cloverNumNow==2 && commits==stableCommits+1);
    checkGardenSpots();
    Clover depthOrder[2]={{180,110,8},{100,115,0}};
    bubbleSortPointsByY(depthOrder,2);
    assert(depthOrder[0].type==0 && depthOrder[1].type==8);

    cloverNumNow=0;
    stored.version++;
    gardenRestorePlants(); assert(cloverNumNow==0);
    stored.version=GARDEN_STORAGE_VERSION;
    stored.count=cloverMaxNum+1;
    gardenRestorePlants(); assert(cloverNumNow==0);
    stored.count=2;
    stored.plants[1].type=11;
    gardenRestorePlants(); assert(cloverNumNow==0);

    stored.plants[1].type=8;
    gardenRestorePlants(); assert(cloverNumNow==2);
    checkGardenSpots();
    gardenClearPlants();
    assert(cloverNumNow==0 && !present);
    gardenRestorePlants(); assert(cloverNumNow==0);
    pageNow=PAGE_GARDEN;
    fakeRandomValue=10;
    addCloverToFloor();
    assert(cloverNumNow==1 && present && stored.count==1);
    assert(refreshes==1 && cloverArr[0].type==0 && spotIndex(&cloverArr[0])==10);
    harvestPlant();
    assert(cloverNumNow==0 && present && stored.count==0);
    assert(dataSaveInfo.money==10 && moneyTxtSp->num==10);
    assert(refreshes==2 && dataWrites==1);
    fakeRandomValue=0;
    addCloverToFloor();
    assert(cloverNumNow==1 && spotIndex(&cloverArr[0])==0);
    gardenClearPlants();
    fakeRandomValue=10;
    for(int i=0;i<cloverMaxNum;i++) addCloverToFloor();
    assert(cloverNumNow==cloverMaxNum);
    checkGardenSpots();
    int fullGardenCommits=commits;
    addCloverToFloor();
    assert(cloverNumNow==cloverMaxNum && commits==fullGardenCommits);
    int count=cloverNumNow;
    gardenRestorePlants();
    assert(cloverNumNow==count && commits==fullGardenCommits);
    checkGardenSpots();
    puts("PASS: thirty separated garden spots; v1 migration; random arrival; restore and reset");
}
'''

floor = Image.open(ROOT / "assets/garden/floorPicWS.png").convert("RGBA")
spots = [(int(x), int(y)) for x, y in re.findall(r"\{(\d+),(\d+)\}",
         spot_layout.split("static const GardenSpot gardenSpots[] = {", 1)[1].split("};", 1)[0])]
assert len(spots) == 30
for x, y in spots:
    assert floor.getpixel((x - 48, y - 115))[:3] in ((164, 202, 115), (180, 210, 148))

with tempfile.TemporaryDirectory(prefix="gview-garden-persistence-") as folder:
    test = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    test.write_text(prelude + layout + backend + production + scenarios, encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
