#include "rom.h"
#include "LCD/backLight.h"
#include "bsp_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
u8 backLightNum;
static SemaphoreHandle_t backLightLock;
void backLightInit(void){backLightLock=xSemaphoreCreateMutex();configASSERT(backLightLock);}
void updateBackLight(u8 num){if(num>10)num=10;bsp_display_backlight(num*num);}
void setBackLightVal(u8 num){
    xSemaphoreTake(backLightLock, portMAX_DELAY);
    backLightNum = num;
    if(backLightNum>10) backLightNum=10;
    if(!backLightNum) backLightNum=1;
    updateBackLight(backLightNum);
    xSemaphoreGive(backLightLock);
}
