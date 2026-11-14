"""Exercise original rider rendering, legacy saves, shop callbacks, and GVIEW composition.

Runs without a device or window. Captures use the production engine, sprites,
and native LCD text. Button chrome is stubbed; category text uses real glyphs.
"""

from pathlib import Path
import re
import subprocess
import tempfile

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/player-character-check"
OUT.mkdir(parents=True, exist_ok=True)

def read(name):
    return (ROOT / name).read_bytes().decode("gb18030")


def function(source, signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


header, rom, home = read("main/rom.h"), read("main/rom.c"), read("main/PAGE/HOME/home.c")
shop = re.sub(r'^#include[^\n]*\n', '', read("main/PAGE/SHOP/shop.c"), flags=re.M)
records = "\n".join(re.search(rf"typedef struct {name}\{{.*?\}}{name};", header, re.S).group()
                    for name in ("DATASAVEINFO", "ITEMSAVEINFO", "GARDENSAVEINFO"))
definitions = "\n".join(line for line in header.splitlines()
                        if line.startswith(("#define LEGACY_RIDER_",
                        "#define HOME_PLANT_HINT_",
                                            "#define dataInfoAdress", "#define itemInfoAdress",
                                            "#define gardenInfoAdress")))
definitions += "\n" + re.search(r"^#define HOME_STATUS_ICON_Y .+$", home, re.M).group()
palettes = home[home.index("const char BikeColorArr"):home.index("//森林背景")]
production = "\n".join(function(home, signature) for signature in (
    "static void addHomeBikePart(", "static void moveHomeBike(",
    "void addManBike(", "void addSavedBike("))
settings = read("main/PAGE/SETTING/setting.c")
settings = settings[settings.index("#define SYSTEM_SET_COUNT"):settings.index("// The brightness menu")]
indicators = "\n".join(function(home, signature) for signature in (
    "static void positionHomeBattery(", "static void refreshHomePlantHint(",
    "static u8 enabledHomeAlarmCount(", "static void refreshHomeAlarmHint("))

harness = r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "GVIEW/gview.h"
#include "main/pic_data/picGroup.h"
#include "main/PAGE/SHOP/shop_model.h"
#include "lcd_init.h"
#include "LCD/viewport.h"
extern DisplayObject *displayMc;
extern TweenS *TweenArr;
extern uint8_t *picCacheTemp;
void doKeyFun(KeyName key);
LCDViewport LCD_GetViewport(void){ return (LCDViewport){40,0,240,240}; }
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#define bool _Bool
__DEFINITIONS__
__RECORDS__
static DATASAVEINFO dataSaveInfo;
static ITEMSAVEINFO itemSaveInfo;
static GARDENSAVEINFO gardenSaveInfo;
static u8 legacyRiderSaveNeeded;
__MIGRATION__

#define ESP_OK 0
static unsigned char flashBytes[4096];
static void *Data_res=flashBytes;
static int writes;
static int esp_partition_erase_range(void *partition,size_t offset,size_t count){
    assert(partition==Data_res && offset+count<=sizeof(flashBytes));
    memset(flashBytes+offset,255,count); ++writes; return ESP_OK;
}
static int esp_partition_write(void *partition,size_t offset,const void *data,size_t count){
    assert(partition==Data_res && offset+count<=sizeof(flashBytes));
    memcpy(flashBytes+offset,data,count); return ESP_OK;
}
__WRITE_DATA__

static DisplayObject *bikeman,*moneyTxtSp;
#define HOME_BIKE_PARTS 9
#define HOME_BIKE_WHEEL_COUNT 2
#define HOME_BIKE_TRAVEL 24
static DisplayObject *homeBikeParts[HOME_BIKE_PARTS];
static short homeBikeBaseX[HOME_BIKE_PARTS];
static u8 homeBikePartCount;
static uint32_t homeBikeFrame;
static uint8_t homeWheelFrame,homeWheelTicks;
static uint8_t homeRiderFrame,homeRiderTicks;
static int hour=12;
extern const u8 *headListArr[],*shirtListArr[],*pantsListArr[],*shoesListArr[],*glovesListArr[],*bagListArr[];
__PALETTES__
__RENDERER__

typedef struct { char *name; const u8 *pic; short maxX,minX; u8 style,FrontBack; } pkmProperty;
#define WS_UI_RIGHT 280
#define PAGE_SHOP 1
#define PAGE_SYSTEM_SET 2
#define PAGE_HOME 3
#define WS_HOME_RIGHT 280
#define WS_HOME_TOP_LEFT_INSET 6
#define SOUND_TRACK_COUNT 10
static u8 pageNow;
static u16 BGColor;
static int returned;
static const char *hints[3];
static void hideTips(void){}
static void showTips(const char *text,float seconds){ (void)text; (void)seconds; }
static void drawPageUITxt(const char *left,const char *middle,const char *right){
    hints[0]=left; hints[1]=middle; hints[2]=right;
}
static void setPageUIMiddleText(const char *text){ hints[1]=text; }
static void setPageUIKeyText(u8 cell,const char *text){ hints[cell]=text; }
static void setPageUILongKeyText(const char *left,const char *right){ (void)left; (void)right; }
static void resetPageView(void (*tick)(void)){ homeBikePartCount=0; resetView(tick); }
__PLACE_PREVIEW__
static void backToHomePage(void){ ++returned; }
static void imuRotationSetEnabled(bool enabled){ (void)enabled; }
static void renderMenuInit(void){}
__SHOP__
#define WS_UI_MID 160
#define WS_HOME_GROUND_OFFSET 70
static DisplayObject *phlPkm;
static short Ex1Min,Ex1Max;
__HOME_FOLLOWER__
__SETTINGS__
static DisplayObject *homeBatteryObj,*homePlantHintObj,*homePlantCountObj;
static DisplayObject *homeAlarmHintObj,*homeAlarmCountObj;
static char homeBatteryText[24],homePlantCountText[4],homeAlarmCountText[4];
static u8 cloverNumNow;
__INDICATORS__

static void capture(const char *directory,const char *name){
    char path[1024]; snprintf(path,sizeof(path),"%s/%s.rgb565",directory,name);
    rendWin(0,0,winW,winH,0x9EBF);
    const u8 *ink=LCD_NativeTextPixels();
    const uint16_t *colors=LCD_NativeTextPalette();
    for(int y=0;y<240;y++) for(int x=0;x<240;x++){
        if(!ink[y*240+x]) continue;
        uint16_t color=colors[ink[y*240+x]];
        picCache[y*320+x+40]=(color>>8)|(color<<8);
    }
    FILE *file=fopen(path,"wb"); assert(file);
    assert(fwrite(picCache,2,winW*winH,file)==winW*winH); fclose(file);
}
static void checkCategoryLine(void){
    rendWin(0,0,winW,winH,BGColor);
    const u8 *ink=LCD_NativeTextPixels();
    const uint16_t *colors=LCD_NativeTextPalette();
    unsigned pixels=0;
    for(int y=0;y<240;y++) for(int x=0;x<240;x++){
        if(!ink[y*240+x] || colors[ink[y*240+x]]!=GRAY) continue;
        assert(y>=30 && y<46); // The category must fit in its single 16px row.
        ++pixels;
    }
    assert(pixels>0);
}
static void reload(void){
    memset(&itemSaveInfo,0,sizeof(itemSaveInfo));
    memcpy(&itemSaveInfo,flashBytes+itemInfoAdress,sizeof(itemSaveInfo));
    migrateLegacyRiderSelection();
}
int main(int argc,char **argv){
    assert(argc==2);
    assert(sizeof(ITEMSAVEINFO)==88 && offsetof(ITEMSAVEINFO,bagIndex)==80);
    assert(offsetof(ITEMSAVEINFO,legacyRiderConfigVersion)==81 && offsetof(ITEMSAVEINFO,legacyRiderIndex)==82);
    // Exercise the old serialized record, including unknown former padding.
    for(unsigned padding=0;padding<256;padding++){
        memset(&itemSaveInfo,padding,sizeof(itemSaveInfo));
        itemSaveInfo.itemHead=7; itemSaveInfo.itemShirt=3;
        itemSaveInfo.headIndex=2; itemSaveInfo.shirtIndex=1;
        unsigned char before[81]; memcpy(before,&itemSaveInfo,sizeof(before));
        itemSaveInfo.legacyRiderConfigVersion=0; legacyRiderSaveNeeded=0;
        migrateLegacyRiderSelection();
        assert(itemSaveInfo.legacyRiderIndex==0 && legacyRiderSaveNeeded);
        assert(memcmp(before,&itemSaveInfo,sizeof(before))==0);
    }
    memset(&itemSaveInfo,0,sizeof(itemSaveInfo));
    itemSaveInfo.itemHead=7; itemSaveInfo.itemShirt=3;
    itemSaveInfo.headIndex=2; itemSaveInfo.shirtIndex=1;
    migrateLegacyRiderSelection();
    legacyRiderSaveNeeded=0; migrateLegacyRiderSelection(); assert(!legacyRiderSaveNeeded);
    unsigned char wardrobe[81]; memcpy(wardrobe,&itemSaveInfo,sizeof(wardrobe));
    dataSaveInfo.money=0; gardenSaveInfo.itemGarden=19;

    viewInit(320,240,10,400,50,40,25);
    gviewSetNativeTextHooks(LCD_NativeTextBegin,LCD_NativeTextDraw);
    renderShopInit();
    assert(COUNT(categories)==9 && getShopListArrNum(9)==0);
    const char *expectedCategories[]={"宝可梦","帐篷","自行车","头饰","上衣","裤子","鞋子","手套","背包"};
    for(unsigned category=0;category<COUNT(categories);category++){
        assert(categoryIndex==category && strcmp(categories[category].title,expectedCategories[category])==0);
        checkCategoryLine();
        if(category==1)assert(categoryIndex==SHOP_TENT_CATEGORY);
        if(category==2){assert(bikeman->ac->p==bikeMan);capture(argv[1],"shop-bike");}
        if(category==3){assert(bikeman->ac->p==bikeMan);capture(argv[1],"shop-headgear");}
        doKeyFun(KMR);
    }
    assert(categoryIndex==0 && dataSaveInfo.money==0 && writes==0);
    assert(memcmp(wardrobe,&itemSaveInfo,sizeof(wardrobe))==0);
    // A valid old save selecting the removed character returns to the original.
    itemSaveInfo.legacyRiderConfigVersion=LEGACY_RIDER_CONFIG_VERSION;
    itemSaveInfo.legacyRiderIndex=1;legacyRiderSaveNeeded=0;
    migrateLegacyRiderSelection();
    assert(itemSaveInfo.legacyRiderIndex==0 && legacyRiderSaveNeeded);
    assert(memcmp(wardrobe,&itemSaveInfo,sizeof(wardrobe))==0);
    writeData();legacyRiderSaveNeeded=0;reload();
    assert(!legacyRiderSaveNeeded && itemSaveInfo.legacyRiderIndex==0);
    assert(memcmp(flashBytes+dataInfoAdress,&dataSaveInfo,sizeof(dataSaveInfo))==0);
    assert(memcmp(flashBytes+gardenInfoAdress,&gardenSaveInfo,sizeof(gardenSaveInfo))==0);
    doKeyFun(KML);assert(returned==1);

    // Render every new companion frame with the real GVIEW decoder and text.
    const char *followerNames[]={"taillow","wingull","volbeat","ninjask","tropius",
                               "meowth","lugia","ho-oh","pidgeot",
                               "aron","metang","swablu","shedinja"};
    for(u8 id=15;id<28;id++){
        itemSaveInfo.monsterIndex=id;
        renderShopInit();
        assert(itemIndex==id && shopItemCount()==27);
        assert(stage.movieclipArr[5].disObj.childrenNum==1);
        DisplayObject *preview=getChildAt(&stage.movieclipArr[5].disObj,0);
        assert(preview && preview->ac && preview->ac->p==momsterListArr[id-1]);
        for(u8 frame=0;frame<preview->ac->totallFrame;frame++){
            AcGotoAndStop(preview->ac,frame);
            assert(preview->frameOffset==getPicOffset(frame,preview->ac->p));
            if(id==19){
                assert(preview->ac->totallFrame==12);
                uint32_t expected=2+7*12+frame*(30+190*70/2);
                assert(preview->frameOffset==expected && preview->ac->frameOffset==expected);
                if(frame>=10)assert(expected>65535);
            }
            char name[80];
            snprintf(name,sizeof(name),"shop-%s-frame-%u",followerNames[id-15],frame);
            capture(argv[1],name);
            checkCategoryLine();
        }
    }

    // Newly appended IDs unlock and equip through real callbacks and raw saves.
    itemSaveInfo.itemMonster|=(UINT64_C(1)<<9)|(UINT64_C(1)<<22);
    for(u8 id=24;id<28;id++){
        const uint64_t previous=itemSaveInfo.itemMonster;
        itemSaveInfo.monsterIndex=9;
        dataSaveInfo.money=ShopListMonsterCost[id]+37;
        renderShopInit();itemIndex=id;refreshShop();
        doKeyFun(KM2);
        assert(itemSaveInfo.itemMonster==(previous|(UINT64_C(1)<<id)));
        assert(itemSaveInfo.monsterIndex==9 && dataSaveInfo.money==37);
        doKeyFun(KM2);reload();
        assert(itemSaveInfo.monsterIndex==id && dataSaveInfo.money==37);
        assert(itemSaveInfo.itemMonster==(previous|(UINT64_C(1)<<id)));
        assert(memcmp(flashBytes+dataInfoAdress,&dataSaveInfo,sizeof(dataSaveInfo))==0);
        doKeyFun(KM2);assert(dataSaveInfo.money==37);
    }

    for(u8 id=19;id<28;id++){
        resetPageView(NULL);addSavedBike(6,140,175,0);
        phlPkm=getNewMvAction(momsterListArr[id-1],-100,116);
        placeHomeFollower(id);
        if(checkMonsterType(id)) phlPkm->x=80;
        for(u8 frame=0;frame<phlPkm->ac->totallFrame;frame++){
            AcGotoAndStop(phlPkm->ac,frame);
            char name[80];
            snprintf(name,sizeof(name),"home-%s-frame-%u",followerNames[id-15],frame);
            capture(argv[1],name);
        }
    }

    // Exercise both follower scan directions, including odd and >32767 offsets.
    uint16_t reference[320*240];
    for(u8 id=19;id<28;id++){
        resetPageView(NULL);
        DisplayObject *sprite=getNewMvAction(momsterListArr[id-1],160,120);
        addChild(5,sprite);
        for(u8 frame=0;frame<sprite->ac->totallFrame;frame++){
            AcGotoAndStop(sprite->ac,frame);sprite->dir=0;
            rendWin(0,0,winW,winH,0x9EBF);memcpy(reference,picCache,sizeof(reference));
            sprite->dir=1;rendWin(0,0,winW,winH,0x9EBF);
            const u8 *desc=sprite->ac->p+2+frame*7;
            int left=160-sprite->width/2+(int)desc[4]-127;
            int top=120-sprite->height/2+(int)desc[5]-127;
            for(int y=0;y<sprite->height;y++) for(int x=0;x<sprite->width;x++)
                assert(picCache[(top+y)*320+left+x]==reference[(top+y)*320+left+sprite->width-1-x]);
        }
    }

    DATASAVEINFO savedSettings=dataSaveInfo;
    dataSaveInfo.homePlantHintConfig=HOME_PLANT_HINT_ON;
    dataSaveInfo.autoRotationEnabled=1;
    setSystemInit();capture(argv[1],"system-plant-on");
    systemNext();systemNext();assert(systemSelect==2);
    systemConfirm();assert(dataSaveInfo.homePlantHintConfig==HOME_PLANT_HINT_OFF);
    capture(argv[1],"system-plant-off");saveSystemSettingsIfDirty();
    resetPageView(NULL);pageNow=PAGE_HOME;
    addChild(9,newChar(18,"12:30",0xFFFF,46,8));
    addChild(9,newChar(12,"11-13 Fri",0xFFFF,46,32));
    addChild(9,newNativePic(weather0,256,HOME_STATUS_ICON_Y));
    homeAlarmHintObj=newNativePic(alarmHint,WS_HOME_RIGHT-58,HOME_STATUS_ICON_Y);
    homeAlarmCountObj=newChar(12,homeAlarmCountText,0xFFFF,WS_HOME_RIGHT-38,HOME_STATUS_ICON_Y);
    addChild(9,homeAlarmHintObj);addChild(9,homeAlarmCountObj);
    snprintf(homeBatteryText,sizeof(homeBatteryText),"电量 100%%");
    homeBatteryObj=newChar(12,homeBatteryText,0xFFFF,0,221);addChild(9,homeBatteryObj);
    homePlantHintObj=newNativePic(gardenHint,0,218);
    homePlantCountObj=newChar(12,homePlantCountText,0xFFFF,0,221);
    addChild(9,homePlantHintObj);addChild(9,homePlantCountObj);
    homePlantHintObj->visible=homePlantCountObj->visible=0;
    memset(dataSaveInfo.alarmClock,0,sizeof(dataSaveInfo.alarmClock));
    memset(dataSaveInfo.alarmClockWeek,0,sizeof(dataSaveInfo.alarmClockWeek));
    dataSaveInfo.alarmClock[0]=(450<<4)|1;dataSaveInfo.alarmClockWeek[0]=31;
    dataSaveInfo.alarmClock[1]=(480<<4)|2;dataSaveInfo.alarmClockWeek[1]=64;
    cloverNumNow=12;dataSaveInfo.homePlantHintConfig=HOME_PLANT_HINT_ON;
    refreshHomePlantHint();refreshHomeAlarmHint();
    capture(argv[1],"home-hints-on");
    dataSaveInfo.homePlantHintConfig=HOME_PLANT_HINT_OFF;refreshHomePlantHint();
    capture(argv[1],"home-plant-off");
    memset(dataSaveInfo.alarmClockWeek,0,sizeof(dataSaveInfo.alarmClockWeek));refreshHomeAlarmHint();
    capture(argv[1],"home-no-alarms");
    dataSaveInfo=savedSettings;

    // Saved clothing and bicycle colors render exactly like the original rider.
    itemSaveInfo.headIndex=2;itemSaveInfo.shirtIndex=1;
    itemSaveInfo.pantsIndex=itemSaveInfo.shoesIndex=itemSaveInfo.glovesIndex=itemSaveInfo.bagIndex=0;
    for(int color=0;color<9;color++){
        resetPageView(NULL);addSavedBike(6,140,175,color);
        assert(bikeman->ac->p==bikeMan);
        rendWin(0,0,winW,winH,0x9EBF);
        unsigned char original[320*240*2];memcpy(original,picCache,sizeof(original));
        resetPageView(NULL);addManBike(6,140,175,color,2,1,0,0,0,0);
        rendWin(0,0,winW,winH,0x9EBF);assert(memcmp(original,picCache,sizeof(original))==0);
        if(!color){
            capture(argv[1],"original");
            for(int frame=0;frame<4;frame++){
                for(int part=2;part<homeBikePartCount;part++)AcGotoAndStop(homeBikeParts[part]->ac,frame);
                char name[32];snprintf(name,sizeof(name),"original-frame-%d",frame);capture(argv[1],name);
            }
        }
    }
    homeBikeFrame=0; homeWheelFrame=homeWheelTicks=0;
    homeRiderFrame=homeRiderTicks=0;
    for(int tick=0;tick<201;tick++){
        moveHomeBike();
        for(int part=2;part<homeBikePartCount;part++){
            assert(homeBikeParts[part]->x-homeBikeBaseX[part]==homeBikeParts[0]->x-homeBikeBaseX[0]);
            assert(homeBikeParts[part]->ac->currentFrame==bikeman->ac->currentFrame || homeBikeParts[part]->ac->totallFrame==1);
        }
    }
    itemSaveInfo.legacyRiderIndex=255; legacyRiderSaveNeeded=0; migrateLegacyRiderSelection();
    assert(itemSaveInfo.legacyRiderIndex==0 && legacyRiderSaveNeeded);
    resetView(NULL);
    free(stage.movieclipArr); free(displayMc); free(TweenArr); free(Action);
    free(picCache); free(picCacheTemp);
    puts("PASS: nine shop categories, no character selector, legacy save migration, original pixels and riding motion");
    return 0;
}
'''

for placeholder, value in {
    "__DEFINITIONS__": definitions,
    "__RECORDS__": records,
    "__MIGRATION__": function(rom, "static void migrateLegacyRiderSelection("),
    "__WRITE_DATA__": function(rom, "void writeData("),
    "__PALETTES__": palettes,
    "__RENDERER__": production,
    "__SHOP__": shop,
    "__PLACE_PREVIEW__": function(read("main/PAGE/pageDraw.c"), "void placePixelPreview("),
    "__HOME_FOLLOWER__": function(read("main/PAGE/HOME/pkmMovie.c"), "u8 checkMonsterType(") + function(home, "static void placeHomeFollower("),
    "__SETTINGS__": settings,
    "__INDICATORS__": indicators,
}.items():
    harness = harness.replace(placeholder, value)

with tempfile.TemporaryDirectory(prefix="gview-player-") as directory:
    directory = Path(directory)
    source, binary = directory / "test.c", directory / "test"
    source.write_bytes(harness.encode("gb18030"))
    (directory / "sdkconfig.h").write_text("")
    (directory / "lcd_init.h").write_text('''
#include <stdint.h>
#define LCD_W 240
#define LCD_H 240
void LCD_NativeTextBegin(void);
uint8_t LCD_NativeTextDraw(const char *,int16_t,int16_t,uint16_t,uint8_t,int16_t,int16_t,int16_t,int16_t);
const uint8_t *LCD_NativeTextPixels(void);
const uint16_t *LCD_NativeTextPalette(void);
''')
    (directory / "esp_heap_caps.h").write_text('''
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static inline void *heap_caps_malloc(size_t size,int caps){ (void)caps; return malloc(size); }
''')
    native_text = directory / "native_text.c"
    native_text.write_bytes((ROOT / "components/LCD/native_text.c").read_bytes())
    # Mach-O has no ESP32 IRAM section; remove only that placement annotation.
    engine = directory / "gview.c"
    engine.write_bytes((ROOT / "components/GVIEW/gview.c").read_bytes().replace(
        b'#define FAST_CODE __attribute__((section(".iram1")))', b'#define FAST_CODE'))
    subprocess.run([
        "cc", "-std=gnu11", "-O1", "-g", "-funsigned-char", "-fsanitize=address,undefined",
        "-Wno-invalid-source-encoding", "-Wno-incompatible-pointer-types",
        "-Wno-incompatible-pointer-types-discards-qualifiers", "-Wno-pointer-sign",
        "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast", "-Wno-format-security",
        "-Wno-parentheses-equality", "-Wno-constant-conversion",
        "-Wno-deprecated-declarations", "-Wno-nonportable-include-path",
        "-I", str(directory), "-I", str(ROOT), "-I", str(ROOT / "components"),
        "-I", str(ROOT / "components/LCD"),
        "-I", str(ROOT / "components/GVIEW"), str(source), str(engine),
        str(native_text),
        str(ROOT / "components/GVIEW/utf8_gb2312_switch.c"),
        str(ROOT / "main/pic_data/picGroup.c"), str(ROOT / "main/PAGE/SHOP/shop_model.c"),
        "-lm", "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary), str(OUT)], check=True)

frames = []
for name in ("original", *(f"original-frame-{frame}" for frame in range(4)),
             "shop-bike", "shop-headgear",
             "system-plant-on", "system-plant-off", "home-hints-on", "home-plant-off", "home-no-alarms",
             *(p.stem for p in sorted(OUT.glob("shop-*-frame-*.rgb565"))),
             *(p.stem for p in sorted(OUT.glob("home-*-frame-*.rgb565")))):
    data = (OUT / f"{name}.rgb565").read_bytes()
    image = Image.new("RGB", (320, 240))
    image.putdata([(((color >> 11) & 31) * 255 // 31,
                    ((color >> 5) & 63) * 255 // 63, (color & 31) * 255 // 31)
                   for color in (int.from_bytes(data[i:i + 2], "big") for i in range(0, len(data), 2))])
    image.crop((40, 0, 280, 240)).save(OUT / f"{name}.png")
    if name == "original" or name.startswith("original-frame-"):
        frames.append(image.crop((115, 110, 210, 224)).resize((285, 342), Image.Resampling.NEAREST))
sheet = Image.new("RGB", (285 * len(frames), 366), "#eeeeee")
draw = ImageDraw.Draw(sheet)
for column, frame in enumerate(frames):
    draw.text((column * 285 + 8, 6), "Original" if column == 0 else f"Original / frame {column}", fill="black")
    sheet.paste(frame, (column * 285, 24))
sheet.save(OUT / "characters.png")
followers = Image.new("RGB", (240 * 5, 240), "white")
for column, name in enumerate(("taillow", "wingull", "volbeat", "ninjask", "tropius")):
    followers.paste(Image.open(OUT / f"shop-{name}-frame-0.png"), (column * 240, 0))
followers.save(OUT / "new-followers.png")
hud = Image.new("RGB", (240*5, 240), "white")
for column, name in enumerate(("home-hints-on", "home-plant-off", "home-no-alarms", "system-plant-on", "system-plant-off")):
    hud.paste(Image.open(OUT / f"{name}.png"), (column*240, 0))
hud.save(OUT / "home-hints-and-settings.png")
print(f"Native character captures: {OUT / 'characters.png'}")
