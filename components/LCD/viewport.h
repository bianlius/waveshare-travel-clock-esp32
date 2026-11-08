#ifndef LCD_VIEWPORT_H
#define LCD_VIEWPORT_H

#include <stdint.h>

typedef struct {
    uint16_t left;
    uint16_t top;
    uint16_t width;
    uint16_t height;
} LCDViewport;

// Default pages use a 240x240 crop with one source pixel per LCD pixel.
void LCD_SetPageViewport(const volatile uint8_t *page, uint8_t active_page,
                         uint16_t left, uint16_t top,
                         uint16_t width, uint16_t height);
LCDViewport LCD_GetViewport(void);

#endif
