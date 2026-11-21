#include "LCD/lcd_init.h"
#include "GVIEW/native_text_raster.h"
#include <assert.h>

static uint8_t nativeTextRows[LCD_H];
const uint8_t *LCD_NativeTextPixels(void){ return NULL; }
const uint16_t *LCD_NativeTextPalette(void){ return NULL; }
const uint8_t *LCD_NativeTextRows(void){ return nativeTextRows; }
void LCD_NativeTextBegin(void){}

uint8_t LCD_NativeTextDraw(const char *text, int16_t x, int16_t y,
                           uint16_t color, uint8_t size,
                           int16_t clipLeft, int16_t clipTop,
                           int16_t clipRight, int16_t clipBottom){
    assert(text && (size == 12 || size == 16 || size == 18));
    if (!text || (size != 12 && size != 16 && size != 18)) return 0;
    const GVIEWNativeTextSurface surface = {
        .viewport = LCD_GetViewport(), .width = LCD_W, .height = LCD_H,
        .left = rendSX, .top = rendSY, .right = rendEX, .bottom = rendEY, .stride = rendW,
        .indices = NULL, .rows = nativeTextRows, .pixels = picCache
    };
    return gviewRasterNativeText(&surface, text, x, y, (color >> 8) | (color << 8), size,
                                clipLeft, clipTop, clipRight, clipBottom);
}
