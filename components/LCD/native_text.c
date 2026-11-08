#include "lcd_init.h"
#include "viewport.h"
#include "esp_heap_caps.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *getFont(uint8_t size, uint8_t type, uint8_t line);
extern uint16_t getFont16(uint8_t *s, uint8_t line);

// A palette index per physical pixel. Index zero is transparent.
static uint8_t *nativeTextPixels;
static uint8_t nativeTextAllocationTried;
static uint8_t nativeTextRows[LCD_H];
static uint16_t nativeTextColors[256];
static uint8_t nativeTextColorCount;

const uint8_t *LCD_NativeTextPixels(void){ return nativeTextPixels; }
const uint16_t *LCD_NativeTextPalette(void){ return nativeTextColors; }
const uint8_t *LCD_NativeTextRows(void){ return nativeTextRows; }

void LCD_NativeTextBegin(void){
    if(!nativeTextAllocationTried){
        nativeTextAllocationTried = 1;
        nativeTextPixels = heap_caps_malloc(LCD_W * LCD_H,
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if(!nativeTextPixels) nativeTextPixels = malloc(LCD_W * LCD_H);
        // Without this buffer, text would be drawn by the scaled canvas path.
        assert(nativeTextPixels && "native LCD text buffer allocation failed");
        if(!nativeTextPixels) abort();
        printf("[LCD] native text buffer: ready\n");
    }
    if(nativeTextPixels) memset(nativeTextPixels,0,LCD_W * LCD_H);
    memset(nativeTextRows,0,sizeof(nativeTextRows));
    nativeTextColorCount = 0;
}

static uint8_t nativeColorIndex(uint16_t color){
    for(int i=1; i<=nativeTextColorCount; ++i){
        if(nativeTextColors[i] == color) return i;
    }
    if(nativeTextColorCount == 255) return 0;
    nativeTextColors[++nativeTextColorCount] = color;
    return nativeTextColorCount;
}

// Find the first LCD pixel whose sampled source pixel reaches the clip edge.
// Only the boundary is transformed, so glyph loops remain constant time.
static int firstPhysicalPixel(int edge, int start, int extent, int pixels){
    int pos = ((edge - start) * pixels) / extent;
    if(pos < 0) pos = 0;
    if(pos > pixels) pos = pixels;
    while(pos > 0 &&
          start + ((2 * (pos - 1) + 1) * extent) / (2 * pixels) >= edge)
        --pos;
    while(pos < pixels &&
          start + ((2 * pos + 1) * extent) / (2 * pixels) < edge)
        ++pos;
    return pos;
}

uint8_t LCD_NativeTextDraw(const char *text, int16_t x, int16_t y,
                           uint16_t color, uint8_t size,
                           int16_t clipLeft, int16_t clipTop,
                           int16_t clipRight, int16_t clipBottom){
    assert(nativeTextPixels && text &&
           (size == 12 || size == 16 || size == 18));
    if(!nativeTextPixels || !text || (size != 12 && size != 16 && size != 18)) return 0;
    const uint8_t ink = nativeColorIndex(color);
    assert(ink && "native LCD text palette exhausted");
    if(!ink) abort();
    const LCDViewport viewport = LCD_GetViewport();
    const int pixelLeft = firstPhysicalPixel(clipLeft, viewport.left, viewport.width, LCD_W);
    const int pixelRight = firstPhysicalPixel(clipRight, viewport.left, viewport.width, LCD_W);
    const int pixelTop = firstPhysicalPixel(clipTop, viewport.top, viewport.height, LCD_H);
    const int pixelBottom = firstPhysicalPixel(clipBottom, viewport.top, viewport.height, LCD_H);
    if(pixelLeft >= pixelRight || pixelTop >= pixelBottom) return 1;
    int originX = ((x - viewport.left) * LCD_W + viewport.width/2) / viewport.width;
    const int originY = ((y - viewport.top) * LCD_H + viewport.height/2) / viewport.height;
    const int glyphSize = size == 12 ? 16 : size == 18 ? 22 : 20;
    const int asciiWidth = glyphSize / 2;
    const int linePitch = size == 12 ? 20 : size == 18 ? 27 : 25;
    const int maxColumns = ((pixelRight - originX) / asciiWidth) & ~1;
    if(maxColumns < 2) return 1;
    if(size == 16 && !strchr(text,'\n')){
        const int bytes = strlen(text);
        if(bytes <= maxColumns &&
           abs(x + bytes * 4 - (viewport.left + viewport.width/2)) <= 2){
            const int originalWidth = (bytes * 8 * LCD_W + viewport.width/2) / viewport.width;
            originX += (originalWidth - bytes * asciiWidth) / 2;
        }
    }
    int columns = 0, line = 0;
    for(const uint8_t *p=(const uint8_t *)text; *p; ){
        if(*p == '\n'){
            columns = 0;
            ++line;
            ++p;
            continue;
        }
        const int chinese = *p > 0x80 && p[1];
        const int step = chinese ? 2 : 1;
        if(columns + step > maxColumns){ columns = 0; ++line; }
        if(!chinese && (*p < 32 || *p > 126)){
            columns += step;
            ++p;
            continue;
        }
        const int glyphX = originX + columns * asciiWidth;
        const int glyphY = originY + line * linePitch;
        const int glyphWidth = chinese ? glyphSize : asciiWidth;
        const int sourceWidth = chinese ? 16 : 8;
        for(int row=0; row<glyphSize; ++row){
            const int py = glyphY + row;
            if(py < pixelTop || py >= pixelBottom) continue;
            const int sourceRow = ((2 * row + 1) * 16) / (2 * glyphSize);
            const uint16_t bits = chinese ? getFont16((uint8_t *)p,sourceRow) :
                                            (uint8_t)*getFont(16,*p - 32,sourceRow);
            for(int col=0; col<glyphWidth; ++col){
                const int sourceCol = ((2 * col + 1) * sourceWidth) / (2 * glyphWidth);
                const int bit = chinese ? (sourceCol < 8 ? sourceCol + 8 : sourceCol - 8)
                                        : sourceCol;
                const int px = glyphX + col;
                if((bits & (1u << bit)) && px >= pixelLeft && px < pixelRight){
                    nativeTextPixels[py * LCD_W + px] = ink;
                    nativeTextRows[py] = 1;
                }
            }
        }
        columns += step;
        p += step;
    }
    return 1;
}
