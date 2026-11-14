#ifndef DESKTOP_COMPAT_H
#define DESKTOP_COMPAT_H
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NVS_NOT_FOUND 4354
#define ESP_ERR_NVS_NO_FREE_PAGES 4365
#define ESP_ERR_NVS_NEW_VERSION_FOUND 4368
#define ESP_ERROR_CHECK(x) do { int desktop_error=(x); if(desktop_error!=ESP_OK){fprintf(stderr,"Desktop ESP shim error: %d\n",desktop_error);abort();} } while(0)
#define ESP_LOGI(tag,...) ((void)0)
#define ESP_LOGW(tag,...) ((void)0)
#define ESP_LOGE(tag,...) ((void)0)
#define IRAM_ATTR
#define DRAM_ATTR
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static inline void *heap_caps_malloc(size_t n,int caps){(void)caps;return malloc(n);}
typedef void *i2c_master_bus_handle_t;
typedef void *SemaphoreHandle_t;
typedef void *TaskHandle_t;
typedef void *EventGroupHandle_t;
#define portMAX_DELAY 0xffffffff
#define portTICK_PERIOD_MS 1
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void){return (void *)1;}
static inline int xSemaphoreTake(SemaphoreHandle_t x,unsigned n){(void)x;(void)n;return 1;}
static inline int xSemaphoreGive(SemaphoreHandle_t x){(void)x;return 1;}
void vTaskDelay(unsigned ticks);
int64_t esp_timer_get_time(void);
uint32_t esp_random(void);
void esp_restart(void);
typedef struct {char label[17];uint32_t size,address;uint8_t type;} esp_partition_t;
typedef uintptr_t esp_partition_mmap_handle_t;
#define ESP_PARTITION_MMAP_DATA 0
esp_partition_t *esp_partition_find_first(int type,int subtype,const char *label);
esp_err_t esp_partition_read(const esp_partition_t *,size_t,void *,size_t);
esp_err_t esp_partition_write(const esp_partition_t *,size_t,const void *,size_t);
esp_err_t esp_partition_erase_range(const esp_partition_t *,size_t,size_t);
esp_err_t esp_partition_mmap(const esp_partition_t *,size_t,size_t,int,const void **,esp_partition_mmap_handle_t *);
void esp_partition_munmap(esp_partition_mmap_handle_t);
typedef unsigned nvs_handle_t;
#define NVS_READWRITE 1
#define NVS_READONLY 0
int nvs_flash_init(void);
int nvs_flash_erase(void);
int nvs_open(const char *,int,nvs_handle_t *);
int nvs_set_blob(nvs_handle_t,const char *,const void *,size_t);
int nvs_get_blob(nvs_handle_t,const char *,void *,size_t *);
int nvs_erase_key(nvs_handle_t,const char *);
int nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
typedef struct {uint8_t ssid[33];int8_t rssi;int authmode;} wifi_ap_record_t;
#define WIFI_AUTH_OPEN 0
int esp_wifi_disconnect(void);
#endif
