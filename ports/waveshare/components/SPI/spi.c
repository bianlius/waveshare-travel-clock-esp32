#include "spi.h"


/**
 * @brief       液晶端口初始化
 * @param       无
 * @retval      无
 */
void LCD_GPIOInit(void)
{
    gpio_config_t gpio_init_struct = {0};
    gpio_init_struct.intr_type = GPIO_INTR_DISABLE;           /* 失能引脚中断 */
    gpio_init_struct.mode = GPIO_MODE_OUTPUT;           /* 输入输出模式 */
    gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;         /* 使能上拉 */
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;    /* 失能下拉 */
    gpio_init_struct.pin_bit_mask = 1ull << LCD_RES_GPIO_PIN; /* 设置的引脚的位掩码 */
    gpio_config(&gpio_init_struct);                           /* 配置GPIO */

    gpio_init_struct.intr_type = GPIO_INTR_DISABLE;          /* 失能引脚中断 */
    gpio_init_struct.mode = GPIO_MODE_OUTPUT;          /* 输入输出模式 */
    gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;        /* 使能上拉 */
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;   /* 失能下拉 */
    gpio_init_struct.pin_bit_mask = 1ull << LCD_DC_GPIO_PIN; /* 设置的引脚的位掩码 */
    gpio_config(&gpio_init_struct);                          /* 配置GPIO */

    gpio_init_struct.intr_type = GPIO_INTR_DISABLE;           /* 失能引脚中断 */
    gpio_init_struct.mode = GPIO_MODE_OUTPUT;           /* 输入输出模式 */
    gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;         /* 使能上拉 */
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;    /* 失能下拉 */
    gpio_init_struct.pin_bit_mask = 1ull << BSP_SPI_MOSI_GPIO_PIN; /* 设置的引脚的位掩码 */
    gpio_config(&gpio_init_struct);                           /* 配置GPIO */

    gpio_init_struct.intr_type = GPIO_INTR_DISABLE;           /* 失能引脚中断 */
    gpio_init_struct.mode = GPIO_MODE_OUTPUT;           /* 输入输出模式 */
    gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;         /* 使能上拉 */
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;    /* 失能下拉 */
    gpio_init_struct.pin_bit_mask = 1ull << BSP_SPI_CLK_GPIO_PIN; /* 设置的引脚的位掩码 */
    gpio_config(&gpio_init_struct);                           /* 配置GPIO */

    gpio_init_struct.intr_type = GPIO_INTR_DISABLE;           /* 失能引脚中断 */
    gpio_init_struct.mode = GPIO_MODE_OUTPUT;           /* 输入输出模式 */
    gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;         /* 使能上拉 */
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;    /* 失能下拉 */
    gpio_init_struct.pin_bit_mask = 1ULL<<LCD_BLK_GPIO_PIN; /* 设置的引脚的位掩码 */
    gpio_config(&gpio_init_struct);   
}

/* SPI句柄 */
spi_device_handle_t LCD_Handle;
/**
 * @brief       初始化SPI
 * @param       无
 * @retval      无
 */
void BSP_SPI_GPIOInit(void)
{
    esp_err_t ret = 0;
    spi_bus_config_t spi_bus_conf = {0};
    /* SPI总线配置 */
    spi_bus_conf.miso_io_num = -1;
    spi_bus_conf.mosi_io_num = BSP_SPI_MOSI_GPIO_PIN; /* SPI_MOSI引脚 */
    spi_bus_conf.sclk_io_num = BSP_SPI_CLK_GPIO_PIN;  /* SPI_SCLK引脚 */
    spi_bus_conf.quadwp_io_num = -1;              /* SPI写保护信号引脚，该引脚未使能 */
    spi_bus_conf.quadhd_io_num = -1;              /* SPI保持信号引脚，该引脚未使能 */
    spi_bus_conf.max_transfer_sz = 24 * 240 * 2; /* 配置最大传输大小，以字节为单位 */
    /* 初始化SPI总线 */
    ret = spi_bus_initialize(SPI2_HOST, &spi_bus_conf, SPI_DMA_CH_AUTO); /* SPI总线初始化 */
    ESP_ERROR_CHECK(ret);                                                /* 校验参数值 */


    /* SPI驱动接口配置 */
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0, /* ST7789 samples SDA on SCL rising edge. */
        .spics_io_num = BSP_SPI_CS_GPIO_PIN, /* SPI设备引脚 */
        .queue_size = 7,                     /* 事务队列尺寸 7个 */
    };
    /* 添加SPI总线设备 */
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &LCD_Handle); /* 配置SPI总线设备 */
    ESP_ERROR_CHECK(ret);
    int actual_khz = 0;
    ESP_ERROR_CHECK(spi_device_get_actual_freq(LCD_Handle, &actual_khz));
    printf("LCD SPI clock: %d kHz, mode 0\n", actual_khz);
}

/**
 * @brief       SPI发送一个字节
 * @param       handle : SPI句柄
 * @param       dat    : 字节内容
 * @retval      无
 */
void BSP_SPI_WR_Bus(uint8_t dat)
{
    esp_err_t ret;
    spi_transaction_t t = {0};
    t.length = 8;                                  /* 要传输的位数 一个字节 8位 */
    t.tx_buffer = &dat;                            /* 将命令填充进去 */
    ret = spi_device_polling_transmit(LCD_Handle, &t); /* 开始传输 */
    ESP_ERROR_CHECK(ret);
}

/**
 * @brief       SPI连续发送len个字节
 * @param       handle : SPI句柄
 * @param       dat    : 要发送的字节地址
 * @param       len    : 要发送的字节长度
 * @retval      无
 */
void BSP_SPI_Write_Data(const uint8_t *dat, int len)
{
    esp_err_t ret;
    spi_transaction_t t = {0};
    if (len == 0)
    {
        return; /* 长度为0 没有数据要传输 */
    }
    t.length = len * 8;                            /* 要传输的位数 一个字节 8位 */
    t.tx_buffer = dat;                             /* 将命令填充进去 */
    ret = spi_device_polling_transmit(LCD_Handle, &t); /* 开始传输 */
    ESP_ERROR_CHECK(ret);                          /* 一般不会有问题 */
}

/**
 * @brief       向液晶连续写入len个字节数据
 * @param       dat: 要写的数据地址
 * @param       len：要写入字节长度
 * @retval      无
 */
void LCD_WR_DATA(uint8_t *dat, int len)
{
    BSP_SPI_Write_Data(dat, len);
}

