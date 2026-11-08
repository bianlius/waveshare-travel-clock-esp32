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
preview = Image.new("RGB", (520, 900), "#e9ecef")
draw = ImageDraw.Draw(preview)
preview_y = 8
for entry in manifest["assets"]:
    path = ROOT / "assets/pokemon_followers" / entry["file"]
    assert hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"]
    source = Image.open(path).convert("RGBA")
    body = packed.split(f"{entry['symbol']}[] = {{", 1)[1].split("};", 1)[0]
    data = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-f]+)", body))
    depth, count = data[:2]
    assert count == entry["frames"] and depth in (4, 16)
    offset = 2 + 7 * count
    for frame in range(count):
        width, height, anchor_x, anchor_y, ticks = struct.unpack_from(">HHBBB", data, 2 + 7 * frame)
        assert (width, height) == (entry["frame_width"], entry["frame_height"])
        assert (anchor_x, anchor_y, ticks) == (127, 127, entry["frame_ticks"])
        original = source.crop((frame * width, 0, (frame + 1) * width, height))
        if entry["flip_x"]:
            original = ImageOps.mirror(original)
        colors = []
        if depth == 4:
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
        assert colors == expected, (entry["file"], frame)
        if frame == 0:
            decoded = Image.new("RGBA", (width, height))
            decoded.putdata([(0, 0, 0, 0) if c is None else
                             (((c >> 11) & 31) * 255 // 31,
                              ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31, 255)
                             for c in colors])
            draw.text((8, preview_y), entry["file"], fill="black")
            preview_y += 18
            for column, background in enumerate(("#172536", "#f8f5eb")):
                tile = Image.new("RGBA", decoded.size, background)
                tile.alpha_composite(decoded)
                preview.paste(tile.resize((width * 3, height * 3), Image.Resampling.NEAREST),
                              (8 + 250 * column, preview_y))
            preview_y += height * 3 + 12
    assert offset == len(data), entry["file"]
out = ROOT / "build/pokemon_followers/packed-preview.png"
out.parent.mkdir(parents=True, exist_ok=True)
preview.crop((0, 0, 520, preview_y)).save(out)
print("PASS: all packed frames retain source RGB565 colors and transparency")

shop = (ROOT / "main/PAGE/SHOP/shop.c").read_bytes().decode("gb18030")
motion = (ROOT / "main/PAGE/HOME/pkmMovie.c").read_bytes().decode("gb18030")


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
    "static u8 itemDisplayPosition(void)", "static u8 itemAtDisplayPosition(u8 position)",
    "static void previousItem(void)", "static void nextItem(void)"))
production += function(motion, "u8 checkMonsterType(u8 index)")
production += function(motion, "void pkmMovieEx1(u8 index)")
harness = r'''
#include <assert.h>
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
#define fs_Sine_EaseInOut 0
const u8 PHL_M[]={0},FSG_M[]={0},LDYS_M[]={0},pkm_ldys2[]={0},LYM_M[]={0},
         GS_M[]={0},YD_M[]={0},SMQT_M[]={0},BKQ_M[]={0};
__ARRAYS__
static u8 categoryIndex,itemIndex,monsterOrder[COUNT(ShopListMonsterCost)];
static struct {u8 count;} categories[]={{COUNT(ShopListMonsterCost)},{3}};
static void refreshShop(void){}
static u8 getShopListArrNum(u8 category){return categories[category].count;}
typedef struct {short x,width;} DisplayObject;
static struct {DisplayObject *disOb;unsigned nextTime;} pkmMvProp[5];
static short Ex1Min,Ex1Max;
static float timeOffset;
static u8 fps=25;
static unsigned randomSeed=7;
static long testRandom(void){randomSeed=randomSeed*1664525u+1013904223u;return randomSeed>>1;}
#define random testRandom
static void Tween(short *axis,float seconds,short target,int delay,int easing){
    (void)seconds;(void)delay;(void)easing;*axis=target;
}
__PRODUCTION__
int main(void){
    assert(COUNT(ShopListMonsterCost)==15 && COUNT(momsterListArr)==14);
    assert(momsterListArr[0]==PHL_M && momsterListArr[8]==BKQ_M);
    const u8 *newSprites[]={followerTorchic,followerMudkip,followerZigzagoon,
                            followerManectric,followerBreloom};
    const u16 newPrices[]={150,150,300,600,500};
    shopBuildPriceOrder(ShopListMonsterCost,COUNT(ShopListMonsterCost),monsterOrder);
    assert(monsterOrder[0]==0 && monsterOrder[1]==10 && monsterOrder[2]==11);
    for(u8 position=0;position<COUNT(monsterOrder);++position){
        assert(itemDisplayPosition()==position && itemIndex==monsterOrder[position]);
        if(position) assert(ShopListMonsterCost[monsterOrder[position-1]]<=ShopListMonsterCost[itemIndex]);
        nextItem();
    }
    assert(itemIndex==0);previousItem();
    assert(itemDisplayPosition()==COUNT(monsterOrder)-1 && itemIndex==9);
    categoryIndex=1;itemIndex=0;previousItem();assert(itemIndex==2);nextItem();assert(itemIndex==0);
    categoryIndex=0;
    for(u8 id=10;id<15;++id){
        assert(momsterListArr[id-1]==newSprites[id-10]);
        assert(ShopListMonsterCost[id]==newPrices[id-10] && checkMonsterType(id));
        uint64_t original=1|(UINT64_C(1)<<2)|(UINT64_C(1)<<9),owned=original;
        uint8_t equipped=2;uint32_t balance=ShopListMonsterCost[id]+10;
        assert(shopApply(&owned,&equipped,id,ShopListMonsterCost[id],&balance)==SHOP_UNLOCKED);
        assert(owned==(original|(UINT64_C(1)<<id)) && balance==10 && equipped==2);
        assert(shopApply(&owned,&equipped,id,ShopListMonsterCost[id],&balance)==SHOP_EQUIPPED);
        assert(equipped==id && balance==10);
        DisplayObject object={160,(short)((newSprites[id-10][2]<<8)|newSprites[id-10][3])};
        pkmMvProp[0].disOb=&object;Ex1Min=60;Ex1Max=240;
        for(unsigned i=0;i<1000;++i){
            short before=object.x;pkmMvProp[0].nextTime=0;pkmMovieEx1(0);
            assert(object.x-object.width/2>=WS_UI_LEFT+4);
            assert(object.x+(object.width+1)/2<=WS_UI_RIGHT-4);
            assert(object.x-before<=80 && before-object.x<=80);
        }
    }
    pkmMvProp[0].disOb=0;pkmMovieEx1(0);pkmMovieEx1(5);
    assert(!checkMonsterType(0) && !checkMonsterType(1) && !checkMonsterType(255));
    assert(checkMonsterType(2) && checkMonsterType(5) && checkMonsterType(7) && checkMonsterType(9));
    puts("PASS: sorted shop navigation preserves saved IDs, purchase prices, and follower bounds");
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
