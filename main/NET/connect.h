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

enum { WIFI_DETAIL_IDLE, WIFI_DETAIL_ASSOC, WIFI_DETAIL_DHCP, WIFI_DETAIL_RETRY };
enum { WIFI_FAILURE_NONE, WIFI_FAILURE_AUTH, WIFI_FAILURE_NOT_FOUND,
       WIFI_FAILURE_TIMEOUT, WIFI_FAILURE_DHCP, WIFI_FAILURE_CONFIG, WIFI_FAILURE_CONNECTION };
enum { WIFI_SAVE_NONE, WIFI_SAVE_OK, WIFI_SAVE_FULL, WIFI_SAVE_ERROR };
extern uint8_t WifiConnectionDetail,WifiConnectionFailure,WifiSaveState,WifiScanFailed;

// Render-task APIs; callbacks only publish events. Poll every frame on every page.
void wifi_scan(void);
void wifi_Init(void);
void wifiProcessSavedConnection(void);
void resetWIFIConnectInfo(char *ssid,char *pass);
void wifi_startConnect(void);
void wifi_connect_default(void);
// Forget an active target; NULL stops all activity before a factory reset.
void wifi_forgetSaved(char *ssid);
void selectWIFIConnectInfo(void);//选择空的WIFI储存空间
#endif
