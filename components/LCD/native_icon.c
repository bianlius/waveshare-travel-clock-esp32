#include "lcd_init.h"
#include "viewport.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <string.h>

// Only small UI sprites are sent here. Scene art remains in GVIEW's framebuffer.
static uint16_t *iconPixels;
static uint8_t *iconMask;
static uint8_t iconRows[LCD_H];
static uint8_t allocationTried;

const uint16_t *LCD_NativeIconPixels(void){ return iconPixels; }
const uint8_t *LCD_NativeIconMask(void){ return iconMask; }
const uint8_t *LCD_NativeIconRows(void){ return iconRows; }

void LCD_NativeIconBegin(void){
    if(!allocationTried){
        allocationTried = 1;
        iconPixels = heap_caps_malloc(LCD_W * LCD_H * sizeof(uint16_t),
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        iconMask = heap_caps_malloc((LCD_W * LCD_H + 7) / 8,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if(!iconPixels || !iconMask){
            free(iconPixels);
            free(iconMask);
            iconPixels = NULL;
            iconMask = NULL;
        }
    }
    if(iconMask) memset(iconMask, 0, (LCD_W * LCD_H + 7) / 8);
    memset(iconRows, 0, sizeof(iconRows));
}

static int pixelColor(const DisplayObject *icon, int x, int y, uint16_t *color){
    const unsigned index = (unsigned)y * icon->width + x;
    const uint8_t *frame = icon->p + icon->frameOffset;
    unsigned paletteIndex;
    switch(icon->picType){
        case 1:
            if(!(frame[index >> 3] & (1u << (index & 7)))) return 0;
            *color = icon->color;
            return 1;
        case 2:
            paletteIndex = (frame[6 + (index >> 2)] >> ((index & 3) * 2)) & 3;
            if(paletteIndex == 3) return 0;
            if(icon->color != COLORNONE) *color = icon->color;
            else {
                const uint8_t *palette = icon->type == 0xF4 ?
                    (const uint8_t *)icon->f : frame;
                *color = ((uint16_t)palette[paletteIndex * 2] << 8) |
                         palette[paletteIndex * 2 + 1];
            }
            return 1;
        case 4:
            paletteIndex = (frame[30 + (index >> 1)] >> ((index & 1) * 4)) & 15;
            if(paletteIndex == 15) return 0;
            if(icon->color != COLORNONE) *color = icon->color;
            else {
                const uint8_t *palette = icon->type == 0xF4 ?
                    (const uint8_t *)icon->f : frame;
                *color = ((uint16_t)palette[paletteIndex * 2] << 8) |
                         palette[paletteIndex * 2 + 1];
            }
            return 1;
        case 8:
            paletteIndex = frame[index];
            if(paletteIndex == 0xff) return 0;
            *color = icon->color != COLORNONE ? icon->color :
                     gviewPalette8Color(paletteIndex);
            return 1;
        case 16:
            *color = ((const uint16_t *)frame)[index];
            if(*color == 0x0001) return 0;
            if(icon->color != COLORNONE) *color = icon->color;
            return 1;
        default:
            return 0;
    }
}

uint8_t LCD_NativeIconDraw(const DisplayObject *icon, int16_t sourceX,
                           int16_t sourceY){
    if(!iconPixels || !iconMask || !icon || !icon->p || icon->ac ||
       icon->alpha != 15 || icon->width <= 0 || icon->height <= 0 ||
       icon->width > 128 || icon->height > 128 ||
       (icon->picType != 1 && icon->picType != 2 && icon->picType != 4 &&
        icon->picType != 8 && icon->picType != 16)) return 0;
    const LCDViewport viewport = LCD_GetViewport();
    if(viewport.width == LCD_W && viewport.height == LCD_H) return 0;
    // Preserve the original logical icon centre while reducing it to 1:1.
    const int centerX = ((2 * (sourceX - viewport.left) + icon->width) *
                         LCD_W + viewport.width) / (2 * viewport.width);
    const int centerY = ((2 * (sourceY - viewport.top) + icon->height) *
                         LCD_H + viewport.height) / (2 * viewport.height);
    const int originX = centerX - icon->width / 2;
    const int originY = centerY - icon->height / 2;
    for(int y=0; y<icon->height; ++y){
        const int py = originY + y;
        if(py < 0 || py >= LCD_H) continue;
        for(int x=0; x<icon->width; ++x){
            const int px = originX + x;
            if(px < 0 || px >= LCD_W) continue;
            const int sourcePixelX = (icon->dir & 0x0f) ? icon->width - 1 - x : x;
            uint16_t color;
            if(!pixelColor(icon, sourcePixelX, y, &color)) continue;
            const size_t index = (size_t)py * LCD_W + px;
            iconPixels[index] = color;
            iconMask[index >> 3] |= 1u << (index & 7);
            iconRows[py] = 1;
        }
    }
    return 1;
}
