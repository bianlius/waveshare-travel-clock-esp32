#include <string.h>
#include "lwip/err.h"
#include "lwip/sys.h"
#include "connect.h"
#include "../config.h"
#include "../rom.h"
#include "weather_forecast.h"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;//WIFI重连次数
wifi_config_t wifi_config;

uint8_t WifiConnectingState;//WIFI连接中的状态
uint32_t IPAdressGet;//获取到的IP地址

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    /*参数：
        *EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet );
         *   xEventGroup：要操作的事件标志组的句柄。
         *  uxBitsToClear：指定要置 1 的事件位，比如要将 bit3 值 1 的话就设置为 0X08。可以同时将多个 bit 置 1，如设置为 0X09 的话就是同时将 bit3 和 bit0 置 1。

         *返回值：任何值：在将指定事件位置 1 后的事件组值。
        */   
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        WifiConnectingState = 1;
        esp_wifi_connect();
        ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        //WIFI 断开事件
        IPAdressGet = 0;
        // 多次连接
        if(pageNow == PAGE_WIFI_CONNECT){//如果是配置密码连接
            if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY&&WifiConnectingState) {
                WifiConnectingState = 2;
                s_retry_num++;
                ESP_LOGI(TAG, "retry to connect to the AP");
                esp_wifi_connect();
            } else {
                //停止连接并报错
                WifiConnectingState = 4;
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                ESP_LOGI(TAG, "4 WifiConnectingState %d",s_retry_num);
            }
        }else{
            // if (wifiSaveInfo.selectWifiNow < wifiSaveInfo.saveNumAll-1) {
            //     wifiSaveInfo.selectWifiNow ++;
            // } else {
            //     wifiSaveInfo.selectWifiNow = 0;
            // }
            wifi_connect_default();
            WifiConnectingState = 2;
            printf("REConnectWifi:%d\n",wifiSaveInfo.selectWifiNow);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
        
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        IPAdressGet = event->ip_info.ip.addr;
        weatherForecastRequest(0);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        WifiConnectingState = 3;
        s_retry_num = 0;
    }
}





void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));


    wifi_config.sta.listen_interval = 100;//路由器监听间隔
    
    

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);//省电模式
    //esp_wifi_set_ps(WIFI_PS_MIN_MODEM);//非省电模式

    // ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* 以下可查询WIFI状态，状态由回调事件event_handler函数处理*/
}


//--------------------WIFI搜索-----------------------------
wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];//缓存搜索到的WIFI结构体
uint8_t refrushScanHd;//是否搜索完毕标记
uint8_t ScaningHd;//是否处于搜索中
uint16_t wifiListNum;//设置最大搜索数量

void wifi_scan(void)
{
    ScaningHd = 1;
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    //如果wifi没有连接成功则关闭连接
    if(WifiConnectingState!=3){
        printf("disconnect:%d\n",WifiConnectingState);
        WifiConnectingState = 0;//停止WIFI自动连接，同时连接WIFI会导致搜索失败
        esp_wifi_disconnect();
    }
    esp_wifi_scan_start(NULL, true);
    wifiListNum = DEFAULT_SCAN_LIST_SIZE;//设置最大搜索数量
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&wifiListNum, ap_info));
    ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, wifiListNum);

    refrushScanHd = 1;
    ScaningHd = 0;
}
//----------------------搜索END---------------------------

void resetWIFIConnectInfo(char *ssid,char *pass){
    if(ssid[0]=='\0'||ssid[1]=='\0'||pass[0]=='\0'||pass[1]=='\0'){
        printf("resetWIFI error: empty credentials\n");
        return;
    }
    uint8_t i;
    for(i = 0; i<32; i++) {
        wifi_config.sta.ssid[i] = ssid[i];
    }
    for(i = 0; i<64; i++) {
        wifi_config.sta.password[i] = pass[i];
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    printf("resetWIFI:%s\n",ssid);
}

void wifi_startConnect(void){
    printf("start connect wifi\n");
    s_retry_num = 0;
    WifiConnectingState = 1;
    esp_wifi_connect();
}

//使用已记录的账号密码登录
void wifi_connect_default(void){
    if(!IPAdressGet){
        //初始化保存的WIFI SSID及密码
        resetWIFIConnectInfo(wifiSaveInfo.ssid[wifiSaveInfo.selectWifiNow],wifiSaveInfo.password[wifiSaveInfo.selectWifiNow]);
        //连接WIFI;
        wifi_startConnect();
    }
}




void wifi_Init(void)
{
    WifiConnectingState = 0;
    //初始化WIFI为STA模式
    wifi_init_sta();
    wifiSaveInfo.selectWifiNow = wifiSaveInfo.saveNumAll>0?wifiSaveInfo.saveNumAll-1:0;
    wifi_connect_default();
}