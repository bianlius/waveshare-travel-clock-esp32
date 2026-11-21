#ifndef PASSPORT_LCD_H
#define PASSPORT_LCD_H
#include <stdint.h>
#include <stdbool.h>
#include "GVIEW/gview.h"
#define LCD_W 320
#define LCD_H 240
void LCD_SetNativeOverlay(const uint8_t *,const volatile uint8_t *,uint8_t);
void LCD_NativeTextBegin(void);
uint8_t LCD_NativeTextDraw(const char *,int16_t,int16_t,uint16_t,uint8_t,int16_t,int16_t,int16_t,int16_t);
const uint8_t *LCD_NativeTextPixels(void);
const uint8_t *LCD_NativeTextRows(void);
const uint16_t *LCD_NativeTextPalette(void);

void LCD_Init(void);
void LCD_SetRotation(uint8_t);
uint8_t LCD_GetRotation(void);
void LCD_MapTouchToLogical(int *,int *);

#endif
