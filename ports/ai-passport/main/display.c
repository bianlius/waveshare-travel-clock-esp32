#include "LCD/lcd_init.h"
#include "passport_display.h"
#include "PAGE/pageDraw.h"
#include "passport_layout.h"
#include "bsp_display.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <string.h>
#include <assert.h>

static SemaphoreHandle_t transfer_done;
static uint8_t rotation;
static uint16_t *dma_pixels;
static bool transfer_pending;
static uint64_t transfer_wait_us;
static bool on_transfer_done(esp_lcd_panel_io_handle_t io,
                            esp_lcd_panel_io_event_data_t *event,void *user){
    (void)io;(void)event;(void)user;
    BaseType_t woke=pdFALSE;xSemaphoreGiveFromISR(transfer_done,&woke);
    return woke==pdTRUE;
}
static void finish_transfer(void){
    if(!transfer_pending)return;
    int64_t start=esp_timer_get_time();
    configASSERT(xSemaphoreTake(transfer_done,pdMS_TO_TICKS(1000))==pdTRUE);
    transfer_wait_us+=esp_timer_get_time()-start;
    transfer_pending=false;
}
uint64_t passport_display_take_wait_us(void){
    uint64_t value=transfer_wait_us;transfer_wait_us=0;return value;
}
static void send_strip(uint16_t x,uint16_t y,uint16_t width,uint16_t height,const uint16_t *pixels){
    // Render the next strip while DMA reads this private staging buffer.
    // The sink has finished reading GVIEW's buffer before returning.
    finish_transfer();
    configASSERT((size_t)width*height<=PASSPORT_WIDTH*PASSPORT_STRIP_ROWS);
    memcpy(dma_pixels,pixels,(size_t)width*height*sizeof(*pixels));
    // GVIEW already stores high-byte-first RGB565.
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(bsp_display_panel(),x,y,x+width,y+height,dma_pixels));
    transfer_pending=true;
}
void LCD_Init(void){
    ESP_ERROR_CHECK(bsp_display_init());
    transfer_done=xSemaphoreCreateBinary();configASSERT(transfer_done);
    const esp_lcd_panel_io_callbacks_t callbacks={.on_color_trans_done=on_transfer_done};
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(bsp_display_io(),&callbacks,NULL));
    LCD_SetRotation(0);
}
void LCD_SetRotation(uint8_t quarter_turns){
    rotation=quarter_turns&2;
    if(!bsp_display_panel()) return;
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(bsp_display_panel(),true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(bsp_display_panel(),rotation==0,rotation!=0));
}
uint8_t LCD_GetRotation(void){return rotation;}
void LCD_MapTouchToLogical(int *x,int *y){(void)x;(void)y;}
void passport_display_start(void){
    dma_pixels=heap_caps_malloc(PASSPORT_WIDTH*PASSPORT_STRIP_ROWS*sizeof(*dma_pixels),
                               MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
    configASSERT(dma_pixels);
    gviewSetStripRenderer(PASSPORT_STRIP_ROWS,send_strip,passport_draw_overlay);
    // Drain even partial frames before rotation, page teardown or sleep.
    gviewSetStripCompletion(finish_transfer);
    gviewSetNativeTextHooks(LCD_NativeTextBegin,LCD_NativeTextDraw);
}
