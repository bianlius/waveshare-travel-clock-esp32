#include "lcd_init.h"
#include "viewport.h"
#include "esp_timer.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


/**
 * @brief       向液晶写寄存器命令
 * @param       reg: 要写的命令
 * @retval      无
 */
void LCD_WR_REG(uint8_t reg)
{
    LCD_DC_Clr();
    BSP_SPI_WR_Bus(reg);
    LCD_DC_Set();
}

/**
 * @brief       向液晶写一个字节数据
 * @param       dat: 要写的数据
 * @retval      无
 */
void LCD_WR_Byte(uint8_t dat)
{
    LCD_DC_Set();
    BSP_SPI_WR_Bus(dat);
    LCD_DC_Set();
}

/**
 * @brief       向液晶写一个半字数据
 * @param       dat: 要写的数据
 * @retval      无
 */
void LCD_WR_HalfWord(uint16_t dat)
{
    LCD_DC_Set();
    BSP_SPI_WR_Bus(dat >> 8);
    BSP_SPI_WR_Bus(dat & 0xFF);
    LCD_DC_Set();
}



/**
 * @brief       液晶进入休眠
 * @param       无
 * @retval      无
 */
void LCD_Enter_Sleep(void)
{
    LCD_WR_REG(0x28);
    vTaskDelay(120 / portTICK_PERIOD_MS);
    LCD_WR_REG(0x10);
    vTaskDelay(50 / portTICK_PERIOD_MS);
}

/**
 * @brief       液晶退出休眠
 * @param       无
 * @retval      无
 */
void LCD_Exit_Sleep(void)
{
    LCD_WR_REG(0x11);
    vTaskDelay(120 / portTICK_PERIOD_MS);
    LCD_WR_REG(0x29);
}


/**
 * @brief       设置显示窗口
 * @param       xs:窗口列起始地址
 * @param       ys:坐标行起始地址
 * @param       xe:窗口列结束地址
 * @param       ye:坐标行结束地址
 * @retval      无
 */
void LCD_Address_Set(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye)
{
    LCD_WR_REG(0x2A);
    LCD_WR_HalfWord(xs);
    LCD_WR_HalfWord(xe);
    LCD_WR_REG(0x2B);
    LCD_WR_HalfWord(ys);
    LCD_WR_HalfWord(ye);
    LCD_WR_REG(0x2C);
}

/**
 * @brief       指定颜色填充区域
 * @param       xs:填充区域列起始地址
 * @param       ys:填充区域行起始地址
 * @param       xe:填充区域列结束地址
 * @param       ye:填充区域行结束地址
 * @param       color:填充颜色值
 * @retval      无
 */
void LCD_Fill(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color)
{
    uint16_t i, j;
    LCD_Address_Set(xs, ys, xe - 1, ye - 1);
    for (j = ys; j < ye; j++)
    {
        for (i = xs; i < xe; i++)
        {
            LCD_WR_HalfWord(color);
        }
    }
}

void LCD_Init(void)
{
    LCD_GPIOInit();
    LCD_BLK_Clr();
    LCD_RES_Clr();
    vTaskDelay(pdMS_TO_TICKS(20));
    LCD_RES_Set();
    vTaskDelay(pdMS_TO_TICKS(120));
    BSP_SPI_GPIOInit();
    LCD_WR_REG(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    LCD_WR_REG(0x3A);
    LCD_WR_Byte(0x55);
    LCD_WR_REG(0x36);
    LCD_WR_Byte(0x00);
    LCD_WR_REG(0x21);
    // Match the original C3 ST7789 normal-mode frame-rate setting.
    LCD_WR_REG(0xC6);
    LCD_WR_Byte(0x01);
    LCD_WR_REG(0x29);
    printf("LCD init complete, C6=0x01\n");
}


// Send 24 LCD rows per SPI transaction to reduce per-transaction overhead.
static uint8_t lcd_row_batch[24 * LCD_W * 2] __attribute__((aligned(4)));
static uint8_t *lcd_prepared_frame;
static uint8_t *lcd_sent_frame;
static bool lcd_sent_frame_allocated;
static bool lcd_sent_frame_valid;
static bool lcd_skip_unchanged;
static size_t lcd_sent_frame_bytes;
static uint16_t sent_x0, sent_y0, sent_x1, sent_y1;
static uint16_t prepared_x0, prepared_y0, prepared_x1, prepared_y1, prepared_row_bytes;
static const uint8_t *volatile native_overlay;
static const volatile uint8_t *native_overlay_page;
static uint8_t native_overlay_active_page;

void LCD_SetNativeOverlay(const uint8_t *pixels, const volatile uint8_t *page,
                          uint8_t active_page){
    native_overlay = NULL;
    native_overlay_page = page;
    native_overlay_active_page = active_page;
    native_overlay = pixels;
}

// Sample from destination pixel centers while keeping nearest-neighbor
// scaling. This keeps pixel-art edges hard and avoids a left/top bias.
static inline uint16_t lcdSourceX(uint16_t x, const LCDViewport *viewport){
    if(viewport->width == LCD_W) return viewport->left + x;
    if(viewport->width == LCD_W / 2) return viewport->left + x / 2;
    return viewport->left +
        ((uint32_t)(2 * x + 1) * viewport->width) / (2 * LCD_W);
}
static inline uint16_t lcdSourceY(uint16_t y, const LCDViewport *viewport){
    if(viewport->height == LCD_H) return viewport->top + y;
    if(viewport->height == LCD_H / 2) return viewport->top + y / 2;
    return viewport->top +
        ((uint32_t)(2 * y + 1) * viewport->height) / (2 * LCD_H);
}

void LCD_SetSkipUnchanged(bool enabled){
    lcd_skip_unchanged = enabled;
    if(!enabled) lcd_sent_frame_valid = false;
}

void LCD_PrepareFrame(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey){
    prepared_y1 = 0;
    // Pixel pages copy their 240x240 crop directly; legacy main views retain their viewport.
    const LCDViewport viewport = LCD_GetViewport();
    if (ex <= sx || ey <= sy || ex > 320 || ey > 240) {
        return;
    }
    uint16_t dx0 = 0, dx1 = LCD_W;
    uint16_t dy0 = 0, dy1 = LCD_H;
    while (dx0 < dx1 && lcdSourceX(dx0, &viewport) < sx) ++dx0;
    while (dx1 > dx0 && lcdSourceX(dx1 - 1, &viewport) >= ex) --dx1;
    while (dy0 < dy1 && lcdSourceY(dy0, &viewport) < sy) ++dy0;
    while (dy1 > dy0 && lcdSourceY(dy1 - 1, &viewport) >= ey) --dy1;
    if (dx0 == dx1 || dy0 == dy1) {
        return;
    }
    if(!lcd_prepared_frame){
        lcd_prepared_frame = malloc((size_t)LCD_W * LCD_H * 2);
        if(!lcd_prepared_frame){
            printf("LCD frame buffer allocation failed\n");
            return;
        }
    }
    prepared_x0 = dx0; prepared_x1 = dx1;
    prepared_y0 = dy0; prepared_y1 = dy1;
    const uint8_t *source = (const uint8_t *)picCache;
    const uint8_t *text_overlay = LCD_NativeTextPixels();
    const uint8_t *text_rows = LCD_NativeTextRows();
    const uint16_t *text_colors = LCD_NativeTextPalette();
    const uint16_t *icon_pixels = LCD_NativeIconPixels();
    const uint8_t *icon_mask = LCD_NativeIconMask();
    const uint8_t *icon_rows = LCD_NativeIconRows();
    const uint8_t *overlay = native_overlay;
    if (overlay && (!native_overlay_page ||
                    *native_overlay_page != native_overlay_active_page))
        overlay = NULL;
    const uint16_t row_bytes = (dx1 - dx0) * 2;
    const uint8_t direct_pixels = viewport.width == LCD_W &&
        viewport.height == LCD_H && !overlay && dx0 == 0 && dx1 == LCD_W;
    prepared_row_bytes = row_bytes;
    for (uint16_t y = dy0; y < dy1; ++y) {
        uint8_t *row = lcd_prepared_frame + (size_t)(y - dy0) * row_bytes;
        const uint16_t src_y = lcdSourceY(y, &viewport);
        const uint8_t has_text = text_overlay && text_rows[y];
        const uint8_t has_icons = icon_pixels && icon_mask && icon_rows[y];
        if(direct_pixels){
            const size_t source_offset =
                ((size_t)(src_y - sy) * (ex - sx) + viewport.left - sx) * 2;
            memcpy(row, source + source_offset, row_bytes);
            if(has_icons){
                for(uint16_t x=0; x<LCD_W; ++x){
                    const size_t pixel = (size_t)y * LCD_W + x;
                    if(icon_mask[pixel >> 3] & (1u << (pixel & 7))){
                        const uint16_t color = icon_pixels[pixel];
                        row[x*2] = color >> 8;
                        row[x*2+1] = color & 0xff;
                    }
                }
            }
            if(has_text){
                for(uint16_t x=0; x<LCD_W; ++x){
                    const size_t pixel = (size_t)y * LCD_W + x;
                    if(text_overlay[pixel]){
                        const uint16_t color = text_colors[text_overlay[pixel]];
                        row[x*2] = color >> 8;
                        row[x*2+1] = color & 0xff;
                    }
                }
            }
        }else for (uint16_t x = dx0; x < dx1; ++x) {
            const uint16_t src_x = lcdSourceX(x, &viewport);
            const size_t index = ((size_t)(src_y - sy) * (ex - sx) + src_x - sx) * 2;
            const size_t out = (x - dx0) * 2;
            row[out] = source[index];
            row[out + 1] = source[index + 1];
            const size_t pixel = (size_t)y * LCD_W + x;
            if (overlay) {
                const uint8_t ink = (overlay[pixel >> 2] >> ((pixel & 3) * 2)) & 3;
                if (ink) {
                    const uint16_t color = ink == 1 ? 0x0000 :
                                           ink == 2 ? 0x14DF : 0x4208;
                    row[out] = color >> 8;
                    row[out + 1] = color & 0xff;
                }
            }
            if (has_icons && (icon_mask[pixel >> 3] & (1u << (pixel & 7)))) {
                const uint16_t color = icon_pixels[pixel];
                row[out] = color >> 8;
                row[out + 1] = color & 0xff;
            }
            if (has_text && text_overlay[pixel]) {
                const uint16_t color = text_colors[text_overlay[pixel]];
                row[out] = color >> 8;
                row[out + 1] = color & 0xff;
            }
        }
    }
}

void LCD_SendPrepared(void){
    const int64_t started_us = esp_timer_get_time();
    if(prepared_y1 > prepared_y0 && lcd_prepared_frame){
        const size_t frame_bytes = (size_t)(prepared_y1 - prepared_y0) * prepared_row_bytes;
        if(lcd_skip_unchanged && lcd_sent_frame_valid &&
           frame_bytes == lcd_sent_frame_bytes &&
           prepared_x0 == sent_x0 && prepared_y0 == sent_y0 &&
           prepared_x1 == sent_x1 && prepared_y1 == sent_y1 &&
           memcmp(lcd_sent_frame, lcd_prepared_frame, frame_bytes) == 0){
            spiSendHd = 0;
            return;
        }
        LCD_Address_Set(prepared_x0, prepared_y0, prepared_x1 - 1, prepared_y1 - 1);
        const uint16_t rows = prepared_y1 - prepared_y0;
        for(uint16_t row = 0; row < rows; row += 24){
            const uint16_t count = rows - row < 24 ? rows - row : 24;
            const size_t bytes = (size_t)count * prepared_row_bytes;
            memcpy(lcd_row_batch, lcd_prepared_frame + (size_t)row * prepared_row_bytes, bytes);
            LCD_WR_DATA(lcd_row_batch, bytes);
        }
        if(lcd_skip_unchanged && !lcd_sent_frame_allocated){
            lcd_sent_frame = malloc((size_t)LCD_W * LCD_H * 2);
            lcd_sent_frame_allocated = true;
        }
        if(lcd_skip_unchanged && lcd_sent_frame){
            memcpy(lcd_sent_frame, lcd_prepared_frame, frame_bytes);
            lcd_sent_frame_bytes = frame_bytes;
            sent_x0 = prepared_x0; sent_y0 = prepared_y0;
            sent_x1 = prepared_x1; sent_y1 = prepared_y1;
            lcd_sent_frame_valid = true;
        }
    }
    static uint32_t measured_frames;
    if(++measured_frames == 1500){
        printf("[LCD] full frame send: %lld us\n",
               (long long)(esp_timer_get_time() - started_us));
        measured_frames = 0;
    }
    spiSendHd = 0;
}

#ifdef GVIEW_UI_AUDIT
void LCD_AuditDumpFrame(const char *name){
    if(!lcd_prepared_frame || prepared_x0 || prepared_y0 || prepared_x1 != 240 || prepared_y1 != 240) return;
    static char line[3072];
    printf("@UI_BEGIN %s\n",name);
    for(unsigned y=0;y<240;y++){
        unsigned used=snprintf(line,sizeof(line),"@UI %s %u ",name,y);
        const uint8_t *row=lcd_prepared_frame+y*prepared_row_bytes;
        for(unsigned x=0;x<240;){
            unsigned color=(row[x*2]<<8)|row[x*2+1], count=1;
            while(x+count<240 && row[(x+count)*2] == row[x*2] && row[(x+count)*2+1] == row[x*2+1]) ++count;
            used+=snprintf(line+used,sizeof(line)-used,"%04x:%x,",color,count);
            x+=count;
        }
        line[used++]='\n';
        line[used]=0;
        fputs(line,stdout);
    }
    printf("@UI_END %s\n",name);
}
#endif
