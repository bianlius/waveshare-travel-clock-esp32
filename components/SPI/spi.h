#ifndef _SPI_H_
#define _SPI_H_

#include <string.h>
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "sdkconfig.h"


/* 定义LCD通信管脚端口 */
#define LCD_RES_GPIO_PIN        40
#define LCD_DC_GPIO_PIN         45
#define BSP_SPI_MOSI_GPIO_PIN GPIO_NUM_39
#define BSP_SPI_CLK_GPIO_PIN GPIO_NUM_38
#define BSP_SPI_CS_GPIO_PIN GPIO_NUM_21
#define LCD_BLK_GPIO_PIN 46



/* 定义端口电平状态 */
#define LCD_RES_Set() gpio_set_level(LCD_RES_GPIO_PIN, 1)
#define LCD_RES_Clr() gpio_set_level(LCD_RES_GPIO_PIN, 0)

#define LCD_DC_Set() gpio_set_level(LCD_DC_GPIO_PIN, 1)
#define LCD_DC_Clr() gpio_set_level(LCD_DC_GPIO_PIN, 0)

#define LCD_BLK_Set() gpio_set_level(LCD_BLK_GPIO_PIN, 1)
#define LCD_BLK_Clr() gpio_set_level(LCD_BLK_GPIO_PIN, 0)

void LCD_GPIOInit(void);
void BSP_SPI_GPIOInit(void);
void BSP_SPI_WR_Bus(uint8_t dat);
void LCD_WR_DATA(uint8_t *dat, int len);
#endif



