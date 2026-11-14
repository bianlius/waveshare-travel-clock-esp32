#ifndef CONNECT_H_
#define CONNECT_H_
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define DEFAULT_SCAN_LIST_SIZE 20
extern wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];//缓存搜索到的WIFI结构体
extern uint8_t refrushScanHd;//搜索完毕标记
extern uint8_t ScaningHd;//搜索中标记
extern uint16_t wifiListNum;//设置最大搜索数量

extern uint8_t WifiConnectingState;//wifi连接状态：0 就绪 1开始连接 2重连 3连接成功 4连接失败
extern char *passWordSTxt;//
extern char *ssidTxt;//

void wifi_scan(void);
void wifi_Init(void);
void wifiProcessSavedConnection(void);
void resetWIFIConnectInfo(char *ssid,char *pass);
void wifi_startConnect(void);
void wifi_connect_default(void);
void selectWIFIConnectInfo(void);//选择空的WIFI储存空间
#endif
