#ifndef _LCD_INIT_H_
#define _LCD_INIT_H_
#include <stdbool.h>
#include <stdint.h>

#include "../SPI/spi.h"
#include "GVIEW/gview.h"


/* 定义液晶分辨率 */
#define USE_HORIZONTAL 0
#define LCD_W 240
#define LCD_H 240


/* 函数说明 */                                                     // 初始化LCD端口
void LCD_WR_REG(uint8_t reg);                                                      // LCD写入一个字节指令
void LCD_WR_Byte(uint8_t dat);                                                     // LCD写入一个字节数据
void LCD_WR_HalfWord(uint16_t dat);                                                // LCD写入一个半字数据
void LCD_SetCursor(uint16_t x, uint16_t y);                                        // 设置显示光标
void LCD_Address_Set(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye);          // 设置显示窗口
void LCD_Fill(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color); // 纯色填充函数
void LCD_Enter_Sleep(void);                                                        // 液晶进入休眠
void LCD_Exit_Sleep(void);                                                         // 液晶退出休眠
void LCD_Init(void);
// Clockwise quarter turns from the board default orientation (0..3).
void LCD_SetRotation(uint8_t quarter_turns);
uint8_t LCD_GetRotation(void);
void LCD_MapTouchToLogical(int *x, int *y);

void LCD_SetSkipUnchanged(bool enabled);
void LCD_PrepareFrame(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey);
void LCD_SendPrepared(void);  //发送屏幕缓存

// Optional 2-bit-per-pixel native LCD text overlay (0: transparent,
// 1: black, 2: blue, 3: dark gray), active only on the selected page.
void LCD_SetNativeOverlay(const uint8_t *pixels, const volatile uint8_t *page,
                          uint8_t active_page);
// Called once per frame by GVIEW to draw small text at native LCD resolution.
void LCD_NativeTextBegin(void);
uint8_t LCD_NativeTextDraw(const char *text, int16_t x, int16_t y,
                           uint16_t color, uint8_t size,
                           int16_t clipLeft, int16_t clipTop,
                           int16_t clipRight, int16_t clipBottom);
const uint8_t *LCD_NativeTextPixels(void);
const uint8_t *LCD_NativeTextRows(void);
const uint16_t *LCD_NativeTextPalette(void);

void LCD_NativeIconBegin(void);
uint8_t LCD_NativeIconDraw(const DisplayObject *icon, int16_t x, int16_t y);
const uint16_t *LCD_NativeIconPixels(void);
const uint8_t *LCD_NativeIconMask(void);
const uint8_t *LCD_NativeIconRows(void);

#endif
