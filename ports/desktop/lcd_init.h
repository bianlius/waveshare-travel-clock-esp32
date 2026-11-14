#ifndef DESKTOP_LCD_H
#define DESKTOP_LCD_H
#include <stdint.h>
#include <stdbool.h>
#include "GVIEW/gview.h"
#define LCD_W 240
#define LCD_H 240
void LCD_SetNativeOverlay(const uint8_t *,const volatile uint8_t *,uint8_t);
void LCD_NativeTextBegin(void);
uint8_t LCD_NativeTextDraw(const char *,int16_t,int16_t,uint16_t,uint8_t,int16_t,int16_t,int16_t,int16_t);
const uint8_t *LCD_NativeTextPixels(void);
const uint8_t *LCD_NativeTextRows(void);
const uint16_t *LCD_NativeTextPalette(void);
#endif
