"""Check the LCD/touch inverse transform and IMU orientation decisions."""

from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
lcd = (root / "components/LCD/lcd_init.c").read_bytes().decode("latin1")
imu = (root / "main/imu_rotation.c").read_text()


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


production = "\n".join([
    function(lcd, "void LCD_SetRotation(uint8_t quarter_turns){"),
    function(lcd, "uint8_t LCD_GetRotation(void){"),
    function(lcd, "static inline void lcdPhysicalToLogical("),
    function(lcd, "void LCD_MapTouchToLogical(int *x, int *y){"),
    function(imu, "int imuRotationFromAcceleration("),
])

harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#define LCD_W 240
#define LCD_H 240
static atomic_uchar lcd_rotation;
__PRODUCTION__
int main(void){
    for(int rotation=0; rotation<4; ++rotation){
        LCD_SetRotation(rotation);
        for(int ly=0; ly<240; ++ly){
            for(int lx=0; lx<240; ++lx){
                int px=lx, py=ly;
                if(rotation==1){ px=239-ly; py=lx; }
                if(rotation==2){ px=239-lx; py=239-ly; }
                if(rotation==3){ px=ly; py=239-lx; }
                LCD_MapTouchToLogical(&px,&py);
                assert(px==lx && py==ly);
            }
        }
    }
    LCD_SetRotation(4);
    assert(LCD_GetRotation()==3);
    assert(imuRotationFromAcceleration(0,8192,0)==0);
    assert(imuRotationFromAcceleration(-8192,0,0)==1);
    assert(imuRotationFromAcceleration(0,-8192,0)==2);
    assert(imuRotationFromAcceleration(8192,0,0)==3);
    assert(imuRotationFromAcceleration(0,0,8192)==-1);
    assert(imuRotationFromAcceleration(6000,6000,0)==-1);
    assert(imuRotationFromAcceleration(3000,0,0)==-1);
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="gview-screen-rotation-") as folder:
    source = Path(folder) / "rotation.c"
    binary = Path(folder) / "rotation"
    source.write_text(harness.replace("__PRODUCTION__", production))
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(source), "-o", str(binary)],
                   check=True)
    subprocess.run([str(binary)], check=True)


frame_production = "\n".join([
    function(lcd, "void LCD_SetRotation(uint8_t quarter_turns){"),
    function(lcd, "uint8_t LCD_GetRotation(void){"),
    function(lcd, "static inline uint16_t lcdSourceX("),
    function(lcd, "static inline uint16_t lcdSourceY("),
    function(lcd, "void LCD_PrepareFrame("),
])

frame_harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define LCD_W 240
#define LCD_H 240
typedef struct { uint16_t left, top, width, height; } LCDViewport;
static atomic_uchar lcd_rotation;
static uint8_t *lcd_prepared_frame;
static uint16_t prepared_x0, prepared_y0, prepared_x1, prepared_y1, prepared_row_bytes;
static uint8_t prepared_rotation;
static const uint8_t *volatile native_overlay;
static const volatile uint8_t *native_overlay_page;
static uint8_t native_overlay_active_page;
static uint16_t *picCache;
static uint8_t text_pixels[240*240], text_rows[240];
static uint16_t text_palette[2]={0,0xabcd};
static uint16_t icon_pixels[240*240];
static uint8_t icon_mask[(240*240+7)/8], icon_rows[240];
static uint8_t overlay_pixels[(240*240+3)/4];
static uint8_t overlay_page = 1;
static LCDViewport LCD_GetViewport(void){ return (LCDViewport){40,0,240,240}; }
static const uint8_t *LCD_NativeTextPixels(void){ return text_pixels; }
static const uint8_t *LCD_NativeTextRows(void){ return text_rows; }
static const uint16_t *LCD_NativeTextPalette(void){ return text_palette; }
static const uint16_t *LCD_NativeIconPixels(void){ return icon_pixels; }
static const uint8_t *LCD_NativeIconMask(void){ return icon_mask; }
static const uint8_t *LCD_NativeIconRows(void){ return icon_rows; }
__PRODUCTION__
int main(void){
    uint8_t *source=malloc(320*240*2);
    assert(source);
    picCache=(uint16_t *)source;
    for(int y=0;y<240;++y) for(int x=0;x<320;++x){
        source[(y*320+x)*2]=(uint8_t)(x-40);
        source[(y*320+x)*2+1]=(uint8_t)y;
    }
    text_pixels[20*240+10]=1; text_rows[20]=1;
    icon_pixels[40*240+30]=0x1234;
    icon_mask[(40*240+30)>>3] |= 1u<<((40*240+30)&7);
    icon_rows[40]=1;
    for(int rotation=0;rotation<4;++rotation){
        LCD_SetRotation(rotation);
        LCD_PrepareFrame(0,0,320,240);
        assert(prepared_rotation==rotation);
        assert(prepared_x0==0 && prepared_y0==0 &&
               prepared_x1==240 && prepared_y1==240);
        for(int py=0;py<240;++py) for(int px=0;px<240;++px){
            int lx=px,ly=py;
            if(rotation==1){ lx=py; ly=239-px; }
            if(rotation==2){ lx=239-px; ly=239-py; }
            if(rotation==3){ lx=239-py; ly=px; }
            // GRAM is sent unrotated; MADCTL performs the physical mapping.
            const size_t index=(ly*240+lx)*2;
            if(lx==10 && ly==20){
                assert(lcd_prepared_frame[index]==0xab);
                assert(lcd_prepared_frame[index+1]==0xcd);
            }else if(lx==30 && ly==40){
                assert(lcd_prepared_frame[index]==0x12);
                assert(lcd_prepared_frame[index+1]==0x34);
            }else{
                assert(lcd_prepared_frame[index]==lx);
                assert(lcd_prepared_frame[index+1]==ly);
            }
        }
    }
    // Exercise a cropped source and all three overlay layers after rotation.
    uint8_t *crop=malloc(140*70*2);
    assert(crop);
    for(int y=30;y<100;++y) for(int x=20;x<160;++x){
        const size_t index=((y-30)*140+x-20)*2;
        crop[index]=(uint8_t)x;
        crop[index+1]=(uint8_t)y;
    }
    picCache=(uint16_t *)crop;
    text_pixels[70*240+80]=1; text_rows[70]=1;
    const size_t overlay_pixel=60*240+50;
    overlay_pixels[overlay_pixel>>2] |= 2u<<((overlay_pixel&3)*2);
    native_overlay=overlay_pixels;
    native_overlay_page=&overlay_page;
    native_overlay_active_page=overlay_page;
    for(int rotation=0;rotation<4;++rotation){
        LCD_SetRotation(rotation);
        LCD_PrepareFrame(60,30,200,100);
        assert(prepared_rotation==rotation);
        assert(prepared_x0==20 && prepared_x1==160);
        assert(prepared_y0==30 && prepared_y1==100);
        for(int ly=30;ly<100;++ly) for(int lx=20;lx<160;++lx){
            int px=lx,py=ly;
            if(rotation==1){ px=239-ly; py=lx; }
            if(rotation==2){ px=239-lx; py=239-ly; }
            if(rotation==3){ px=ly; py=239-lx; }
            assert(px>=0 && px<240 && py>=0 && py<240);
            const size_t index=(size_t)(ly-prepared_y0)*prepared_row_bytes+
                               (lx-prepared_x0)*2;
            const uint16_t actual=((uint16_t)lcd_prepared_frame[index]<<8)|
                                  lcd_prepared_frame[index+1];
            uint16_t expected=(lx<<8)|ly;
            if(lx==50 && ly==60) expected=0x14df;
            if(lx==30 && ly==40) expected=0x1234;
            if(lx==80 && ly==70) expected=0xabcd;
            if(actual!=expected) fprintf(stderr,"rotation=%d logical=(%d,%d) physical=(%d,%d) actual=%04x expected=%04x\n",rotation,lx,ly,px,py,actual,expected);
            assert(actual==expected);
        }
    }
    free(lcd_prepared_frame);
    free(crop);
    free(source);
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="gview-rotated-frame-") as folder:
    source = Path(folder) / "frame.c"
    binary = Path(folder) / "frame"
    source.write_text(frame_harness.replace("__PRODUCTION__", frame_production))
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(source), "-o", str(binary)],
                   check=True)
    subprocess.run([str(binary)], check=True)


panel_config = "\n".join(line for line in lcd.splitlines()
                         if line.startswith("static const uint8_t lcd_madctl[")
                         or line.startswith("static const uint8_t lcd_x_offset[")
                         or line.startswith("static const uint8_t lcd_y_offset["))
panel_harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LCD_W 240
#define LCD_H 240
static uint8_t frame[8]={1,2,3,4,5,6,7,8};
static uint8_t batch[24*LCD_W*2];
#define lcd_row_batch batch
static uint8_t *lcd_prepared_frame=frame, *lcd_sent_frame=frame;
static uint16_t prepared_x0=10, prepared_y0=20, prepared_x1=12, prepared_y1=22;
static uint16_t prepared_row_bytes=4;
static uint16_t sent_x0=10, sent_y0=20, sent_x1=12, sent_y1=22;
static uint8_t prepared_rotation, lcd_panel_rotation;
static bool lcd_skip_unchanged=true, lcd_sent_frame_valid=true;
static bool lcd_sent_frame_allocated=true;
static size_t lcd_sent_frame_bytes=8;
static int spiSendHd, last_reg=-1, madctl=-1, sends;
static uint16_t address[4];
__PANEL_CONFIG__
static int64_t esp_timer_get_time(void){ return 0; }
static void LCD_WR_REG(uint8_t value){ last_reg=value; }
static void LCD_WR_Byte(uint8_t value){
    assert(last_reg==0x36);
    madctl=value;
}
static void LCD_Address_Set(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1){
    address[0]=x0; address[1]=y0; address[2]=x1; address[3]=y1;
}
static void LCD_WR_DATA(const uint8_t *pixels,size_t bytes){
    assert(bytes==sizeof(frame) && memcmp(pixels,frame,bytes)==0);
    ++sends;
}
__SEND_PRODUCTION__
int main(void){
    // Unchanged pixels can skip a send in the same orientation.
    LCD_SendPrepared();
    assert(sends==0 && madctl==-1);
    for(int rotation=1;rotation<4;++rotation){
        prepared_rotation=rotation;
        lcd_sent_frame_valid=true;
        LCD_SendPrepared();
        assert(madctl==lcd_madctl[rotation]);
        assert(address[0]==10+lcd_x_offset[rotation]);
        assert(address[1]==20+lcd_y_offset[rotation]);
        assert(address[2]==11+lcd_x_offset[rotation]);
        assert(address[3]==21+lcd_y_offset[rotation]);
        assert(sends==rotation);
    }
    prepared_rotation=0;
    LCD_SendPrepared();
    assert(madctl==0 && sends==4);
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="gview-panel-rotation-") as folder:
    source = Path(folder) / "panel.c"
    binary = Path(folder) / "panel"
    source.write_text(panel_harness.replace("__PANEL_CONFIG__", panel_config)
                    .replace("__SEND_PRODUCTION__",
                             function(lcd, "void LCD_SendPrepared(void){")))
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(source), "-o", str(binary)],
                   check=True)
    subprocess.run([str(binary)], check=True)


imu_task_production = "\n".join([
    function(imu, "int imuRotationFromAcceleration("),
    function(imu, "void imuRotationSetEnabled(bool enabled){"),
    function(imu, "static void imuRotationTask(void *arg){"),
])
imu_task_harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
typedef uint32_t TickType_t;
#define pdMS_TO_TICKS(ms) (ms)
static atomic_bool imu_rotation_enabled = true;
static jmp_buf done;
static int step, observed=1, rotation, rotations, changed_at[2];
void imuRotationSetEnabled(bool enabled);
static TickType_t xTaskGetTickCount(void){ return 0; }
static int LCD_GetRotation(void){ return rotation; }
static void LCD_SetRotation(uint8_t value){
    assert(rotations<2);
    changed_at[rotations++]=step;
    rotation=value;
}
static bool imuReadAcceleration(int16_t *x,int16_t *y,int16_t *z){
    *x=observed==1 ? -8192 : 0;
    *y=observed==2 ? -8192 : 0;
    *z=0;
    return true;
}
static void vTaskDelayUntil(TickType_t *next,TickType_t ticks){
    *next+=ticks;
    ++step;
    if(step==4){
        assert(rotation==1 && rotations==1);
        observed=2;
        imuRotationSetEnabled(false);
    }
    if(step==10){
        assert(rotation==1 && rotations==1);
        imuRotationSetEnabled(true);
    }
    if(step==16) longjmp(done,1);
}
__PRODUCTION__
int main(void){
    if(!setjmp(done)) imuRotationTask(NULL);
    assert(rotation==2 && rotations==2);
    assert(changed_at[0]==3 && changed_at[1]==13);
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="gview-imu-toggle-") as folder:
    source = Path(folder) / "imu.c"
    binary = Path(folder) / "imu"
    source.write_text(imu_task_harness.replace("__PRODUCTION__", imu_task_production))
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(source), "-o", str(binary)],
                   check=True)
    subprocess.run([str(binary)], check=True)
