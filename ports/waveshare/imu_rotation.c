#include "imu_rotation.h"
#include "LCD/lcd_init.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

// Waveshare ESP32-S3-Touch-LCD-1.54: QMI8658 on the touch I2C bus.
#define QMI8658_ADDR       0x6b
#define QMI8658_WHO_AM_I   0x00
#define QMI8658_CTRL1      0x02
#define QMI8658_CTRL2      0x03
#define QMI8658_CTRL7      0x08
#define QMI8658_STATUS0    0x2e
#define QMI8658_AX_L       0x35
#define QMI8658_RESET      0x60
#define QMI8658_CHIP_ID    0x05

static i2c_master_dev_handle_t imu_dev;
static atomic_bool imu_rotation_enabled = true;

void imuRotationSetEnabled(bool enabled){
    atomic_store(&imu_rotation_enabled, enabled);
}

static esp_err_t imuRead(uint8_t reg, uint8_t *data, size_t length){
    return i2c_master_transmit_receive(imu_dev, &reg, 1, data, length, 30);
}

static esp_err_t imuWrite(uint8_t reg, uint8_t value){
    uint8_t command[2] = {reg, value};
    return i2c_master_transmit(imu_dev, command, sizeof(command), 30);
}

int imuRotationFromAcceleration(int16_t x, int16_t y, int16_t z){
    const int ax = abs((int)x), ay = abs((int)y), az = abs((int)z);
    // At +/-4g full scale, 8192 counts = 1g. Ignore flat and diagonal poses.
    // Board-specific IMU axis signs are calibrated against the native LCD.
    if(az > 9000) return -1;
    if(ax > ay){
        if(ax < 5000 || ax * 2 < ay * 3) return -1;
        return x > 0 ? 3 : 1;
    }
    if(ay < 5000 || ay * 2 < ax * 3) return -1;
    return y > 0 ? 0 : 2;
}

static bool imuReadAcceleration(int16_t *x, int16_t *y, int16_t *z){
    uint8_t status, data[6];
    if(imuRead(QMI8658_STATUS0, &status, 1) != ESP_OK || !(status & 1) ||
       imuRead(QMI8658_AX_L, data, sizeof(data)) != ESP_OK) return false;
    *x = (int16_t)((uint16_t)data[0] | (uint16_t)data[1] << 8);
    *y = (int16_t)((uint16_t)data[2] | (uint16_t)data[3] << 8);
    *z = (int16_t)((uint16_t)data[4] | (uint16_t)data[5] << 8);
    return true;
}

static void imuRotationTask(void *arg){
    (void)arg;
    int candidate = -1;
    unsigned stable_samples = 0;
    TickType_t next = xTaskGetTickCount();
    while(1){
        if(!atomic_load(&imu_rotation_enabled)){
            candidate = -1;
            stable_samples = 0;
            vTaskDelayUntil(&next, pdMS_TO_TICKS(100));
            continue;
        }
        int16_t x, y, z;
        const int observed = imuReadAcceleration(&x, &y, &z) ?
                             imuRotationFromAcceleration(x, y, z) : -1;
        if(observed < 0){
            candidate = -1;
            stable_samples = 0;
        }else if(observed != candidate){
            candidate = observed;
            stable_samples = 1;
        }else if(stable_samples < 4){
            ++stable_samples;
        }
        if(stable_samples == 4 && atomic_load(&imu_rotation_enabled) &&
           LCD_GetRotation() != candidate){
            LCD_SetRotation((uint8_t)candidate);
            printf("QMI8658 screen rotation: %d degrees\n", candidate * 90);
        }
        vTaskDelayUntil(&next, pdMS_TO_TICKS(100));
    }
}

void imuRotationStart(i2c_master_bus_handle_t bus){
    if(!bus || i2c_master_probe(bus, QMI8658_ADDR, 100) != ESP_OK){
        printf("QMI8658 absent; auto rotation disabled\n");
        return;
    }
    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = QMI8658_ADDR,
        .scl_speed_hz = 400000,
    };
    if(i2c_master_bus_add_device(bus, &config, &imu_dev) != ESP_OK) return;
    uint8_t id = 0;
    if(imuRead(QMI8658_WHO_AM_I, &id, 1) != ESP_OK || id != QMI8658_CHIP_ID ||
       imuWrite(QMI8658_RESET, 0xb0) != ESP_OK){
        printf("QMI8658 ID/init failed (0x%02x); auto rotation disabled\n", id);
        i2c_master_bus_rm_device(imu_dev);
        imu_dev = NULL;
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    // Auto-increment, +/-4g acceleration at 125 Hz, accelerometer only.
    if(imuWrite(QMI8658_CTRL1, 0x40) != ESP_OK ||
       imuWrite(QMI8658_CTRL2, 0x16) != ESP_OK ||
       imuWrite(QMI8658_CTRL7, 0x01) != ESP_OK ||
       xTaskCreate(imuRotationTask, "imu_rotation", 3072, NULL, 3, NULL) != pdPASS){
        printf("QMI8658 setup failed; auto rotation disabled\n");
        i2c_master_bus_rm_device(imu_dev);
        imu_dev = NULL;
    }else{
        printf("QMI8658 ready; auto rotation enabled\n");
    }
}
