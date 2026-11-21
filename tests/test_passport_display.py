"""Exercise the real copying DMA sink with deliberately delayed completions."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "ports/ai-passport"

compat = r'''
#ifndef DISPLAY_TEST_COMPAT_H
#define DISPLAY_TEST_COMPAT_H
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
typedef int esp_err_t;
typedef int BaseType_t;
typedef void *SemaphoreHandle_t;
typedef void *esp_lcd_panel_io_handle_t;
typedef struct { int unused; } esp_lcd_panel_io_event_data_t;
typedef struct {
    bool (*on_color_trans_done)(esp_lcd_panel_io_handle_t,
                               esp_lcd_panel_io_event_data_t *,void *);
} esp_lcd_panel_io_callbacks_t;
typedef void (*GVIEWStripSink)(uint16_t,uint16_t,uint16_t,uint16_t,const uint16_t *);
#define configASSERT assert
#define ESP_ERROR_CHECK(value) assert((value)==0)
#define pdFALSE 0
#define pdTRUE 1
#define pdMS_TO_TICKS(value) (value)
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_DMA 2
void *bsp_display_panel(void);
void *bsp_display_io(void);
int bsp_display_init(void);
int esp_lcd_panel_swap_xy(void *,bool);
int esp_lcd_panel_mirror(void *,bool,bool);
int esp_lcd_panel_draw_bitmap(void *,int,int,int,int,const void *);
int esp_lcd_panel_io_register_event_callbacks(void *,const esp_lcd_panel_io_callbacks_t *,void *);
void *heap_caps_malloc(size_t,int);
int64_t esp_timer_get_time(void);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
int xSemaphoreGiveFromISR(SemaphoreHandle_t,BaseType_t *);
int xSemaphoreTake(SemaphoreHandle_t,int);
void gviewSetStripRenderer(uint16_t,GVIEWStripSink,void (*)(void));
void gviewSetStripCompletion(void (*)(void));
void gviewSetNativeTextHooks(void (*)(void),void *);
void LCD_NativeTextBegin(void);
void LCD_SetRotation(uint8_t);
extern void *LCD_NativeTextDraw;
#endif
'''

harness = r'''
#include "compat.h"
#include <stdio.h>
#include <string.h>
#include "PRODUCTION_DISPLAY"

static GVIEWStripSink sink;
static void (*drain)(void);
static esp_lcd_panel_io_callbacks_t callbacks;
static const uint16_t *inflight;
static unsigned pixels_inflight, submitted, completed, tokens, render_number;
static int64_t fake_clock;
void *LCD_NativeTextDraw;
void LCD_NativeTextBegin(void){}
void passport_draw_overlay(void){}
void *bsp_display_panel(void){return (void *)1;}
void *bsp_display_io(void){return (void *)2;}
int bsp_display_init(void){return 0;}
int esp_lcd_panel_swap_xy(void *panel,bool swap){assert(panel && swap && !inflight);return 0;}
int esp_lcd_panel_mirror(void *panel,bool x,bool y){assert(panel && x!=y && !inflight);return 0;}
int esp_lcd_panel_io_register_event_callbacks(void *io,const esp_lcd_panel_io_callbacks_t *cb,void *user){
    assert(io && !user);callbacks=*cb;return 0;
}
void *heap_caps_malloc(size_t size,int caps){
    assert(size==320*16*2 && caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA));return malloc(size);
}
int64_t esp_timer_get_time(void){fake_clock+=17;return fake_clock;}
SemaphoreHandle_t xSemaphoreCreateBinary(void){return (void *)3;}
int xSemaphoreGiveFromISR(SemaphoreHandle_t semaphore,BaseType_t *woke){
    assert(semaphore && tokens==0);++tokens;*woke=pdTRUE;return pdTRUE;
}
int xSemaphoreTake(SemaphoreHandle_t semaphore,int timeout){
    assert(semaphore && timeout>0 && inflight && submitted==completed+1);
    // The original render buffer has already been overwritten for the next strip.
    // DMA must still see the previous strip's immutable private copy.
    for(unsigned i=0;i<pixels_inflight;++i)assert(inflight[i]==render_number-1);
    inflight=NULL;++completed;
    assert(callbacks.on_color_trans_done(NULL,NULL,NULL));
    assert(tokens==1);--tokens;return pdTRUE;
}
int esp_lcd_panel_draw_bitmap(void *panel,int x,int y,int right,int bottom,const void *pixels){
    assert(panel && !inflight && tokens==0 && submitted==completed);
    assert(x>=0 && right<=320 && y>=0 && bottom<=240);
    assert(right>x && bottom>y && bottom-y<=16);
    inflight=pixels;pixels_inflight=(right-x)*(bottom-y);++submitted;return 0;
}
void gviewSetStripRenderer(uint16_t rows,GVIEWStripSink cb,void (*overlay)(void)){
    assert(rows==16 && overlay==passport_draw_overlay);sink=cb;
}
void gviewSetStripCompletion(void (*cb)(void)){drain=cb;}
void gviewSetNativeTextHooks(void (*begin)(void),void *draw){assert(begin==LCD_NativeTextBegin);(void)draw;}
int main(void){
    LCD_Init();passport_display_start();assert(sink && drain);
    uint16_t original[320*16];
    // Include odd-width, short and partial frames; every frame must drain.
    for(unsigned strips=1;strips<=15;++strips){
        unsigned width=320-(strips%3),height=16-(strips%4);
        for(unsigned n=0;n<strips;++n){
            for(unsigned i=0;i<320*16;++i)original[i]=render_number;
            sink(0,n*16,width,height,original);
            assert(inflight!=original && transfer_pending);
            memset(original,0xff,sizeof(original));
            ++render_number;
        }
        drain();assert(!inflight && !transfer_pending && completed==submitted);
        drain();assert(!inflight && !tokens);
        LCD_SetRotation(strips&2);
    }
    assert(submitted==120 && passport_display_take_wait_us()>0);
    assert(passport_display_take_wait_us()==0);
    free(dma_pixels);
    puts("PASS: real DMA sink overlaps rendering, preserves 120 delayed strips and drains partial frames");
}
'''.replace("PRODUCTION_DISPLAY", str(PORT / "main/display.c"))

with tempfile.TemporaryDirectory(prefix="passport-display-") as temp:
    folder = Path(temp)
    (folder / "compat.h").write_text(compat)
    for name in ("LCD/lcd_init.h", "PAGE/pageDraw.h", "bsp_display.h",
                 "esp_lcd_panel_io.h", "esp_lcd_panel_ops.h", "freertos/FreeRTOS.h",
                 "freertos/semphr.h", "esp_heap_caps.h", "esp_timer.h"):
        path = folder / name
        path.parent.mkdir(exist_ok=True, parents=True)
        path.write_text('#include "compat.h"\n')
    source, binary = folder / "test.c", folder / "test"
    source.write_text(harness)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(folder), "-I" + str(PORT / "include"),
                    str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
