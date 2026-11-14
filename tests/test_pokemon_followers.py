"""Check asset colors, saved follower IDs, price navigation, and movement bounds."""

import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageOps

ROOT = Path(__file__).resolve().parents[1]
manifest = json.loads((ROOT / "assets/pokemon_followers/manifest.json").read_text())
packed = (ROOT / "main/pic_data/pocketMon/followers.h").read_text()
column_width = max((e["frame_width"] * e.get("scale_percent", 100) + 50) // 100
                   for e in manifest["assets"]) * 3 + 16
preview_height = sum(((e["frame_height"] * e.get("scale_percent", 100) + 50) // 100) * 3 + 30
                     for e in manifest["assets"]) + 8
preview = Image.new("RGB", (column_width * 2, preview_height), "#e9ecef")
draw = ImageDraw.Draw(preview)
preview_y = 8
for entry in manifest["assets"]:
    path = ROOT / "assets/pokemon_followers" / entry["file"]
    assert hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"]
    source = Image.open(path).convert("RGBA")
    body = packed.split(f"{entry['symbol']}[] = {{", 1)[1].split("};", 1)[0]
    data = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-f]+)", body))
    depth, count = data[:2]
    assert count == entry["frames"] and depth in (4, 13, 16)
    offset = 2 + 7 * count
    source_width, source_height = entry["frame_width"], entry["frame_height"]
    scale = entry.get("scale_percent", 100)
    display_size = ((source_width * scale + 50) // 100,
                    (source_height * scale + 50) // 100)
    source_frames = []
    dimensions = entry.get("frame_dimensions", [[source_width, source_height]] * count)
    strip_x = 0
    for frame_width, frame_height in dimensions:
        original = source.crop((strip_x, 0, strip_x + frame_width, frame_height))
        strip_x += frame_width
        if entry["flip_x"]:
            original = ImageOps.mirror(original)
        if scale != 100:
            original = original.resize(display_size, Image.Resampling.NEAREST)
        source_frames.append(original)
    target_bottom = (max(frame.getchannel("A").getbbox()[3] for frame in source_frames)
                     if entry.get("align_opaque_bottom") else None)
    for frame in range(count):
        width, height, anchor_x, anchor_y, ticks = struct.unpack_from(">HHBBB", data, 2 + 7 * frame)
        assert (width, height) == (tuple(dimensions[frame]) if depth == 13 else display_size)
        dx, dy = entry.get("frame_offsets", [[0, 0]] * count)[frame]
        assert (anchor_x, anchor_y, ticks) == (127 + dx, 127 + dy, entry["frame_ticks"])
        assert offset < (1 << 31)  # Animation offsets are 32-bit, including >64K.
        original = source_frames[frame]
        if target_bottom is not None:
            aligned = Image.new("RGBA", display_size)
            aligned.alpha_composite(original,
                                    (0, target_bottom - original.getchannel("A").getbbox()[3]))
            original = aligned
        colors = []
        if depth == 13:
            encoded = struct.unpack_from(f">{width * height}H", data, offset)
            colors = [((c >> 9 & 15) * 255 // 15, (c >> 4 & 31) * 255 // 31,
                       (c & 15) * 255 // 15, (c >> 13) * 255 // 7) for c in encoded]
            offset += width * height * 2
            assert colors == list(original.getdata()), (entry["file"], frame, "RGB454 + alpha")
        elif depth == 4:
            palette = struct.unpack_from(">15H", data, offset)
            offset += 30
            for pixel in range(width * height):
                index = (data[offset + pixel // 2] >> (4 * (pixel & 1))) & 15
                colors.append(None if index == 15 else palette[index])
            offset += (width * height + 1) // 2
        else:
            colors = [None if color == 1 else color for color in
                      struct.unpack_from(f"<{width * height}H", data, offset)]
            offset += width * height * 2
        expected = [None if a < 128 else ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                    for r, g, b, a in original.getdata()]
        if depth != 13:
            assert colors == expected, (entry["file"], frame)
        if target_bottom is not None:
            assert max(i // width for i, color in enumerate(colors) if color is not None) + 1 == target_bottom
        if frame == 0:
            decoded = Image.new("RGBA", (width, height))
            decoded.putdata(colors if depth == 13 else [(0, 0, 0, 0) if c is None else
                             (((c >> 11) & 31) * 255 // 31,
                              ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31, 255)
                             for c in colors])
            draw.text((8, preview_y), entry["file"], fill="black")
            preview_y += 18
            for column, background in enumerate(("#172536", "#f8f5eb")):
                tile = Image.new("RGBA", decoded.size, background)
                tile.alpha_composite(decoded)
                preview.paste(tile.resize((width * 3, height * 3), Image.Resampling.NEAREST),
                              (8 + column_width * column, preview_y))
            preview_y += height * 3 + 12
    assert offset == len(data), entry["file"]
out = ROOT / "build/pokemon_followers/packed-preview.png"
out.parent.mkdir(parents=True, exist_ok=True)
preview.crop((0, 0, column_width * 2, preview_y)).save(out)
print("PASS: all packed frames retain source RGB565 colors and transparency")

shop = (ROOT / "main/PAGE/SHOP/shop.c").read_bytes().decode("gb18030")
motion = (ROOT / "main/PAGE/HOME/pkmMovie.c").read_bytes().decode("gb18030")
home = (ROOT / "main/PAGE/HOME/home.c").read_bytes().decode("gb18030")


def function(source, signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


arrays = "\n".join(re.search(rf"const [^\n]*\b{name}\[\] = \{{[^;]*;", shop).group()
                   for name in ("ShopListMonsterTxt", "ShopListMonsterCost", "momsterListArr",
                                "ShopListMonsterGround"))
production = "\n".join(function(shop, signature) for signature in (
    "u8 shopMonsterAvailable(u8 index)", "static void buildMonsterOrder(void)",
    "static u8 shopItemCount(void)",
    "static u8 itemDisplayPosition(void)", "static u8 itemAtDisplayPosition(u8 position)",
    "static void previousItem(void)", "static void nextItem(void)"))
production += function(motion, "u8 checkMonsterType(u8 index)")
production += function(motion, "static void followerExtents(")
production += function(motion, "void pkmMovieEx1(u8 index)")
production += function(motion, "void pkmMovieEx2(u8 index,short yOffset)")
production += function(home, "static void placeHomeFollower(u8 index)")
harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "main/PAGE/SHOP/shop_model.h"
#include "main/pic_data/pocketMon/followers.h"
typedef uint8_t u8;
typedef uint16_t u16;
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define WS_UI_LEFT 40
#define WS_UI_RIGHT 280
#define WS_UI_MID 160
#define WS_UI_FOOTER_TOP 220
#define WS_HOME_GROUND_OFFSET 70
#define fs_Sine_EaseInOut 0
const u8 PHL_M[]={0},FSG_M[]={0},LDYS_M[]={0},pkm_ldys2[]={0},LYM_M[]={0},
         GS_M[]={0},YD_M[]={0},SMQT_M[]={0},BKQ_M[]={0};
__ARRAYS__
static u8 categoryIndex,itemIndex,monsterOrder[COUNT(ShopListMonsterCost)],monsterCount;
static struct {u8 count;} categories[]={{COUNT(ShopListMonsterCost)},{3}};
static void refreshShop(void){}
static u8 getShopListArrNum(u8 category){return categories[category].count;}
typedef struct {float frameMultiple;const u8 *p;} DisObjAction;
typedef struct {short x,width,y,height; DisObjAction *ac;} DisplayObject;
static DisplayObject *phlPkm;
static int followerLayer;
static void addChild(u8 layer,DisplayObject *obj){
    assert(obj==phlPkm); followerLayer=layer;
}
static struct {DisplayObject *disOb;unsigned nextTime;} pkmMvProp[5];
static short Ex1Min,Ex1Max;
static float timeOffset;
static u8 fps=25;
static const short winW=320;
static unsigned randomSeed=7;
static long testRandom(void){randomSeed=randomSeed*1664525u+1013904223u;return randomSeed>>1;}
#define random testRandom
static void Tween(short *axis,float seconds,short target,int delay,int easing){
    (void)seconds;(void)delay;(void)easing;*axis=target;
}
__PRODUCTION__
int main(void){
    assert(COUNT(ShopListMonsterCost)==28 && COUNT(momsterListArr)==27);
    assert(momsterListArr[0]==PHL_M && momsterListArr[8]==BKQ_M);
    assert(momsterListArr[13]==NULL && !shopMonsterAvailable(14));
    assert(shopMonsterAvailable(0) && !shopMonsterAvailable(255));
    const u8 *newSprites[]={followerTorchic,followerMudkip,followerZigzagoon,
                            followerManectric,NULL,followerTaillow,followerWingull,
                            followerVolbeat,followerNinjask,followerTropius,
                            followerMeowth,followerLugia,followerHoOh,followerPidgeot,
                            followerAron,followerMetang,followerSwablu,followerShedinja};
    const u16 newPrices[]={1200,1200,100,700,0,100,100,300,700,1200,100,5000,5000,700,300,2000,300,1200};
    buildMonsterOrder();
    assert(monsterCount==27 && shopItemCount()==27);
    assert(monsterOrder[0]==0 && monsterOrder[1]==12 && monsterOrder[2]==15);
    for(u8 position=0;position<monsterCount;++position){
        assert(itemDisplayPosition()==position && itemIndex==monsterOrder[position]);
        assert(itemIndex!=14 && shopMonsterAvailable(itemIndex));
        if(position) assert(ShopListMonsterCost[monsterOrder[position-1]]<=ShopListMonsterCost[itemIndex]);
        nextItem();
    }
    assert(itemIndex==0);previousItem();
    assert(itemDisplayPosition()==monsterCount-1 && itemIndex==22);
    categoryIndex=1;itemIndex=0;previousItem();assert(itemIndex==2);nextItem();assert(itemIndex==0);
    categoryIndex=0;
    for(u8 id=10;id<28;++id){
        if(id==14) continue;
        assert(momsterListArr[id-1]==newSprites[id-10]);
        assert(ShopListMonsterCost[id]==newPrices[id-10]);
        assert(checkMonsterType(id)==(id<14 || id==20 || id==24));
        uint64_t original=1|(UINT64_C(1)<<2)|(UINT64_C(1)<<9),owned=original;
        uint8_t equipped=2;uint32_t balance=ShopListMonsterCost[id]+10;
        assert(shopApply(&owned,&equipped,id,ShopListMonsterCost[id],&balance)==SHOP_UNLOCKED);
        assert(owned==(original|(UINT64_C(1)<<id)) && balance==10 && equipped==2);
        assert(shopApply(&owned,&equipped,id,ShopListMonsterCost[id],&balance)==SHOP_EQUIPPED);
        assert(equipped==id && balance==10);
        DisObjAction action={.p=newSprites[id-10]};
        DisplayObject object={.x=160,.y=116,.ac=&action,
            .width=(short)((newSprites[id-10][2]<<8)|newSprites[id-10][3]),
            .height=(short)((newSprites[id-10][4]<<8)|newSprites[id-10][5])};
        pkmMvProp[0].disOb=&object;Ex1Min=60;Ex1Max=240;
        unsigned leftFlights=0,rightFlights=0;
        for(unsigned i=0;i<1000;++i){
            short before=object.x;pkmMvProp[0].nextTime=0;
            if(checkMonsterType(id)) pkmMovieEx1(0);
            else pkmMovieEx2(0,WS_HOME_GROUND_OFFSET);
            assert(object.x-object.width/2>=WS_UI_LEFT+4);
            assert(object.x+(object.width+1)/2<=WS_UI_RIGHT-4);
            for(u8 f=0;f<action.p[1];f++){
                const u8 *desc=action.p+2+f*7;
                int w=(desc[0]<<8)|desc[1],h=(desc[2]<<8)|desc[3];
                int dx=(int)desc[4]-127,dy=(int)desc[5]-127;
                assert(object.x-w/2+dx>=WS_UI_LEFT+4);
                assert(object.x+(w+1)/2+dx<=WS_UI_RIGHT-4);
                if(!checkMonsterType(id)){
                    assert(object.y-h/2+dy>=4);
                    assert(object.y+(h+1)/2+dy<=WS_UI_FOOTER_TOP-4);
                }
            }
            if(checkMonsterType(id)) assert(object.x-before<=80 && before-object.x<=80);
            else{
                assert(object.y-object.height/2>=4);
                assert(object.y+(object.height+1)/2<=WS_UI_FOOTER_TOP-4);
                assert(action.frameMultiple==1 || action.frameMultiple==0.5);
                if(id==16 || id==19 || id==25){
                    assert(action.frameMultiple==1);
                    if(object.x<before)++leftFlights;
                    if(object.x>before)++rightFlights;
                }
            }
        }
        if(id==16 || id==19 || id==25)assert(leftFlights>0 && rightFlights>0);
    }
    uint64_t saved=1|(UINT64_C(1)<<2)|(UINT64_C(1)<<13)|(UINT64_C(1)<<14);
    uint8_t equipped=14;
    assert(shopRemoveRetiredMonster(&saved,&equipped));
    assert(equipped==0 && saved==(1|(UINT64_C(1)<<2)|(UINT64_C(1)<<13)));
    assert(!shopRemoveRetiredMonster(&saved,&equipped));
    equipped=13; saved|=UINT64_C(1)<<14;
    assert(shopRemoveRetiredMonster(&saved,&equipped) && equipped==13);
    assert(!(saved&(UINT64_C(1)<<15))); // Retired ownership never unlocks a new species.
    pkmMvProp[0].disOb=0;pkmMovieEx1(0);pkmMovieEx1(5);
    pkmMovieEx2(0,70);pkmMovieEx2(5,70);
    assert(!checkMonsterType(0) && !checkMonsterType(1) && !checkMonsterType(255));
    assert(checkMonsterType(2) && checkMonsterType(5) && checkMonsterType(7) && checkMonsterType(9));
    DisplayObject follower={.height=24};
    phlPkm=&follower;
    for(u8 id=1;id<COUNT(ShopListMonsterTxt);++id){
        if(!shopMonsterAvailable(id)) continue;
        Ex1Min=Ex1Max=0;
        placeHomeFollower(id);
        int small=checkMonsterType(id) && id>=9;
        assert(followerLayer==(small ? 6 : 1));
        if(checkMonsterType(id)){
            assert(Ex1Min==(small ? 40 : 60));
            assert(Ex1Max==(small ? 100 : 240));
            assert(follower.y==(small ? 146 : 140)+WS_HOME_GROUND_OFFSET-12);
        }else if(id>=SHOP_FIRST_FLYING_FOLLOWER){
            assert(follower.x==160 && follower.y==116);
        }
    }
    puts("PASS: retired ID migration, sorted navigation, all prices, ground/flight bounds, and follower layers");
}
'''
with tempfile.TemporaryDirectory(prefix="gview-followers-") as folder:
    test = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    test.write_text(harness.replace("__ARRAYS__", arrays).replace("__PRODUCTION__", production))
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I", str(ROOT), str(test),
                    str(ROOT / "main/PAGE/SHOP/shop_model.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
