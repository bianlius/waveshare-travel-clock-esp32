#include "LCD/lcd_init.h"
#include "PAGE/pageDraw.h"
#include "passport_display.h"
static const uint8_t *extra_ink;
static const volatile uint8_t *extra_page;
static uint8_t extra_page_id;
void LCD_SetNativeOverlay(const uint8_t *ink,const volatile uint8_t *page,uint8_t id){
    extra_ink=ink;extra_page=page;extra_page_id=id;
}
void passport_draw_overlay(void){
    if(extra_ink && extra_page && *extra_page==extra_page_id){
        for(unsigned y=rendSY;y<rendEY;y++)for(unsigned x=rendSX;x<rendEX;x++){
            size_t index=y*LCD_W+x;
            unsigned value=(extra_ink[index/4]>>((index%4)*2))&3;
            if(value){
                uint16_t color=value==1?BLACK:value==2?BLUE:GRAY;
                picCache[(y-rendSY)*rendW+x-rendSX]=(color>>8)|(color<<8);
            }
        }
    }
    drawPageReturnHint();
}
