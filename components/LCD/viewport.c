#include "viewport.h"

static const volatile uint8_t *active_page_ptr;
static uint8_t active_page_id;
static LCDViewport active_viewport;

void LCD_SetPageViewport(const volatile uint8_t *page, uint8_t active_page,
                         uint16_t left, uint16_t top,
                         uint16_t width, uint16_t height){
    active_page_ptr = page;
    active_page_id = active_page;
    active_viewport = (LCDViewport){left,top,width,height};
}

LCDViewport LCD_GetViewport(void){
    if(active_page_ptr && *active_page_ptr == active_page_id)
        return active_viewport;
    return (LCDViewport){40,0,240,240};
}
