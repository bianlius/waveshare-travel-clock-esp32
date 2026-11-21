"""Exercise production rounded bitmaps across the 255-pixel size boundary."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "components/GVIEW/gview.c").read_bytes().decode("gb18030")


def function(signature):
    start = source.index(signature)
    brace = source.index("{", start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define basicOffset 7
#define my_malloc malloc
typedef struct {uint8_t *p;} DisplayObject;
static DisplayObject bitmap;
static DisplayObject *newPicSingle(uint8_t *p,uint16_t color,short x,short y){
    (void)color;(void)x;(void)y;bitmap.p=p;return &bitmap;
}
'''
production = "\n".join(function(signature) for signature in [
    "uint16_t getPicWidth(", "uint16_t getPicHeight(",
    "void setPColor1(", "void Draw_Circle(", "void PIC_DrawCircle(",
    "void PIC_DrawRect(", "void PIC_DrawHLine(", "DisplayObject* newRoundRectDisObj(",
])
scenarios = r'''
static int pixel(const uint8_t *p,unsigned x,unsigned y,unsigned width){
    unsigned index=y*width+x;
    return (p[basicOffset+2+index/8]>>(index%8))&1;
}
int main(void){
    const unsigned widths[]={224,239,240,255,256,269,284,289,296,304,320};
    const unsigned heights[]={20,26,58,240,256,320};
    for(unsigned w=0;w<sizeof(widths)/sizeof(*widths);w++)
    for(unsigned h=0;h<sizeof(heights)/sizeof(*heights);h++)
    for(unsigned filled=0;filled<2;filled++){
        unsigned width=widths[w],height=heights[h];
        DisplayObject *obj=newRoundRectDisObj(0,width,height,0,0,6,filled);
        uint8_t *p=obj->p;
        assert(getPicWidth(p,0)==width && getPicHeight(p,0)==height);
        assert(!pixel(p,0,0,width) && !pixel(p,width-1,height-1,width));
        assert(pixel(p,width/2,height/2,width)==(int)filled);
        assert(pixel(p,width/2,0,width));
        assert(pixel(p,width-1,height/2,width));
        if(filled)for(unsigned x=6;x<width-6;x++)
            assert(pixel(p,x,height/2,width));
        free(p);
    }
    puts("PASS: filled/outlined rounded rectangles retain 16-bit sizes and complete rows across 255 pixels");
}
'''
with tempfile.TemporaryDirectory(prefix="gview-round-rect-") as folder:
    test = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    test.write_text(harness + production + scenarios)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
