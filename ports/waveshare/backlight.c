#include "LCD/backLight.h"
#include <stdio.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "rom.h"


#define LEDC_TIMER              LEDC_TIMER_1
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL            LEDC_CHANNEL_5
#define LEDC_DUTY_RES           LEDC_TIMER_10_BIT
#define LEDC_FREQUENCY          (1000)    // Frequency in Hertz. Set frequency at 50 Hz


u8 backLightNum = 4; //0-10;
static SemaphoreHandle_t backLightLock;

void backLightInit(void){
    backLightLock = xSemaphoreCreateMutex();
    configASSERT(backLightLock);
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    printf("Backlight PWM GPIO: %d\n", LEDC_OUTPUT_IO);
}

void setBackLightVal(u8 num){
    xSemaphoreTake(backLightLock, portMAX_DELAY);
    backLightNum = num;
    if(backLightNum>10) backLightNum=10;
    if(!backLightNum) backLightNum=1;
    updateBackLight(backLightNum);
    xSemaphoreGive(backLightLock);
}

void updateBackLight(u8 num){
    num = num * num;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, (u16)(1024/100*num)));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}