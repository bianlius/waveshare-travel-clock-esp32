#include "lcd_init.h"
#include "GVIEW/native_text_raster.h"
#include "esp_heap_caps.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

uint8_t LCD_NativeTextDraw(const char *text, int16_t x, int16_t y,
                           uint16_t color, uint8_t size,
                           int16_t clipLeft, int16_t clipTop,
                           int16_t clipRight, int16_t clipBottom){
    assert(nativeTextPixels && text && (size == 12 || size == 16 || size == 18));
    if (!nativeTextPixels || !text || (size != 12 && size != 16 && size != 18)) return 0;
    const uint8_t ink = nativeColorIndex(color);
    assert(ink && "native LCD text palette exhausted");
    if (!ink) abort();
    const GVIEWNativeTextSurface surface = {
        .viewport = LCD_GetViewport(), .width = LCD_W, .height = LCD_H,
        .left = 0, .top = 0, .right = LCD_W, .bottom = LCD_H, .stride = LCD_W,
        .indices = nativeTextPixels, .rows = nativeTextRows, .pixels = NULL
    };
    return gviewRasterNativeText(&surface, text, x, y, ink, size,
                                clipLeft, clipTop, clipRight, clipBottom);
}
