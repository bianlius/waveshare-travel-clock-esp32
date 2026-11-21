#include <string.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "connect.h"
#include "../rom.h"
#include "weather_forecast.h"

#define CONNECT_TIMEOUT_US INT64_C(12000000)
#define DHCP_TIMEOUT_US INT64_C(10000000)
#define MANUAL_TIMEOUT_US INT64_C(25000000)
#define SCAN_TIMEOUT_US INT64_C(10000000)
#define DISCONNECT_TIMEOUT_US INT64_C(1000000)
#define RETRY_DELAY_US INT64_C(500000)
#define ROUND_DELAY_US INT64_C(10000000)
#define ATTEMPTS_PER_NETWORK 2

typedef enum { PH_IDLE, PH_ASSOC, PH_DHCP, PH_ONLINE, PH_RETRY, PH_SETTLE } WifiPhase;
typedef enum { DO_NOTHING, DO_CONNECT, DO_SCAN, DO_RETRY, DO_FAIL } WifiAction;
typedef enum { EV_START, EV_DISCONNECT, EV_ASSOC, EV_IP, EV_LOST_IP, EV_SCAN } WifiEventKind;
typedef struct {
    WifiEventKind kind;
    uint32_t ip;
    uint16_t reason;
    uint8_t status;
    uint8_t ssid[32];
} WifiEvent;

static const char *TAG="wifi station";
static QueueHandle_t s_events;
static atomic_bool s_event_overflow;
static WifiPhase s_phase;
static WifiAction s_after_disconnect;
static uint8_t s_started,s_manual,s_valid,s_attempts,s_failed_slots;
static uint8_t s_scan_started,s_connect_after_scan;
static int s_saved_slot;
static int64_t s_deadline,s_retry_at,s_manual_deadline,s_scan_deadline;
static char s_requested_ssid[33],s_requested_password[64];
static wifi_config_t wifi_config;

uint8_t WifiConnectingState;
uint8_t WifiConnectionDetail,WifiConnectionFailure,WifiSaveState,WifiScanFailed;
uint32_t IPAdressGet;
wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
uint8_t refrushScanHd,ScaningHd;
uint16_t wifiListNum;

static void execute_action(WifiAction action);
static void connection_failed(uint8_t failure,int disconnect_first);

/* Only this callback runs on the event task. Never touch UI, saved records or
 * connection policy here: publish small, ordered copies to the render task. */
static void event_handler(void *arg,esp_event_base_t base,int32_t id,void *data){
    (void)arg;
    WifiEvent event={0};
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_START) event.kind=EV_START;
    else if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED){
        event.kind=EV_DISCONNECT;
        if(data)event.reason=((wifi_event_sta_disconnected_t *)data)->reason;
    }else if(base==WIFI_EVENT && id==WIFI_EVENT_STA_CONNECTED){
        event.kind=EV_ASSOC;
        const wifi_event_sta_connected_t *connected=data;
        if(connected)memcpy(event.ssid,connected->ssid,connected->ssid_len<32?connected->ssid_len:32);
    }else if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP){
        event.kind=EV_IP;
        event.ip=((ip_event_got_ip_t *)data)->ip_info.ip.addr;
    }else if(base==IP_EVENT && id==IP_EVENT_STA_LOST_IP) event.kind=EV_LOST_IP;
    else if(base==WIFI_EVENT && id==WIFI_EVENT_SCAN_DONE){
        event.kind=EV_SCAN;
        if(data)event.status=((wifi_event_sta_scan_done_t *)data)->status;
    }else return;
    if(xQueueSend(s_events,&event,0)!=pdTRUE)atomic_store(&s_event_overflow,true);
}

void resetWIFIConnectInfo(char *ssid,char *password){
    const size_t ssid_length=ssid?strnlen(ssid,33):0;
    const size_t password_length=password?strnlen(password,64):64;
    s_valid=ssid_length && ssid_length<=32 && password_length<=63;
    if(!s_valid)return;
    /* UI buffers and saved-list rows can change as soon as the page closes. */
    memmove(s_requested_ssid,ssid,ssid_length);
    s_requested_ssid[ssid_length]=0;
    memmove(s_requested_password,password,password_length);
    s_requested_password[password_length]=0;
}

static int load_saved(int slot){
    const unsigned count=wifiSaveInfo.saveNumAll;
    if(!count || count>10)return 0;
    for(unsigned tried=0;tried<count;++tried){
        slot=(slot+(int)count)%(int)count;
        resetWIFIConnectInfo(wifiSaveInfo.ssid[slot],wifiSaveInfo.password[slot]);
        if(s_valid){s_saved_slot=slot;return 1;}
        ++slot;
    }
    return 0;
}

static void set_power_save(wifi_ps_type_t mode){
    const esp_err_t result=esp_wifi_set_ps(mode);
    if(result!=ESP_OK)ESP_LOGW(TAG,"Cannot update Wi-Fi power policy: %s",esp_err_to_name(result));
}

static void switch_action(WifiAction action){
    if(!s_started){s_after_disconnect=action;return;}
    if(s_phase==PH_SETTLE){s_after_disconnect=action;return;}
    if(s_phase==PH_ASSOC || s_phase==PH_DHCP || s_phase==PH_ONLINE){
        s_after_disconnect=action;
        s_phase=PH_SETTLE;
        s_deadline=esp_timer_get_time()+DISCONNECT_TIMEOUT_US;
        IPAdressGet=0;
        /* Wait for the old disconnect event before configuring a new AP. */
        if(esp_wifi_disconnect()==ESP_OK)return;
    }
    execute_action(action);
}

static void execute_action(WifiAction action){
    s_after_disconnect=DO_NOTHING;
    s_phase=PH_IDLE;
    if(action==DO_CONNECT){
        s_connect_after_scan=0;
        if(!s_valid){WifiConnectingState=4;WifiConnectionFailure=WIFI_FAILURE_CONFIG;return;}
        memset(&wifi_config,0,sizeof(wifi_config));
        memcpy(wifi_config.sta.ssid,s_requested_ssid,strlen(s_requested_ssid));
        memcpy(wifi_config.sta.password,s_requested_password,strlen(s_requested_password));
        wifi_config.sta.scan_method=WIFI_FAST_SCAN;
        wifi_config.sta.sort_method=WIFI_CONNECT_AP_BY_SIGNAL;
        wifi_config.sta.listen_interval=3;
        ++s_attempts;
        const esp_err_t configured=esp_wifi_set_config(WIFI_IF_STA,&wifi_config);
        if(configured!=ESP_OK){connection_failed(WIFI_FAILURE_CONFIG,0);return;}
        set_power_save(WIFI_PS_NONE);
        s_phase=PH_ASSOC;
        s_deadline=esp_timer_get_time()+CONNECT_TIMEOUT_US;
        WifiConnectionDetail=WIFI_DETAIL_ASSOC;
        if(WifiConnectingState!=1)WifiConnectingState=2;
        if(esp_wifi_connect()!=ESP_OK)connection_failed(WIFI_FAILURE_CONFIG,0);
    }else if(action==DO_SCAN){
        const wifi_scan_config_t scan={.scan_type=WIFI_SCAN_TYPE_ACTIVE,
            .scan_time.active={.min=0,.max=120},.home_chan_dwell_time=30};
        s_scan_started=1;
        s_scan_deadline=esp_timer_get_time()+SCAN_TIMEOUT_US;
        if(esp_wifi_scan_start(&scan,false)!=ESP_OK){
            s_scan_started=0;ScaningHd=0;WifiScanFailed=1;refrushScanHd=1;
            if(!IPAdressGet && !s_manual)load_saved(wifiSaveInfo.selectWifiNow);
            if(!IPAdressGet && s_valid){
                s_retry_at=esp_timer_get_time()+RETRY_DELAY_US;s_phase=PH_RETRY;
                WifiConnectingState=2;WifiConnectionDetail=WIFI_DETAIL_RETRY;
            }
        }
    }else if(action==DO_RETRY) s_phase=PH_RETRY;
    else if(action==DO_FAIL){
        s_manual=0;s_valid=0; // A later search resumes remembered APs, not this failed password.
        WifiConnectingState=4;
        WifiConnectionDetail=WIFI_DETAIL_IDLE;
        set_power_save(WIFI_PS_MIN_MODEM);
    }
}

static uint8_t failure_from_reason(uint16_t reason){
    if(reason==WIFI_REASON_AUTH_FAIL || reason==WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
       reason==WIFI_REASON_HANDSHAKE_TIMEOUT) return WIFI_FAILURE_AUTH;
    if(reason==WIFI_REASON_NO_AP_FOUND) return WIFI_FAILURE_NOT_FOUND;
    return WIFI_FAILURE_CONNECTION;
}

static void connection_failed(uint8_t failure,int disconnect_first){
    IPAdressGet=0;
    WifiConnectionFailure=failure;
    if(ScaningHd){s_phase=PH_IDLE;return;}
    const int64_t now=esp_timer_get_time();
    if(s_manual && (failure==WIFI_FAILURE_AUTH || failure==WIFI_FAILURE_CONFIG ||
       s_attempts>=ATTEMPTS_PER_NETWORK || now>=s_manual_deadline)){
        if(disconnect_first)switch_action(DO_FAIL);else execute_action(DO_FAIL);
        return;
    }
    s_retry_at=now+RETRY_DELAY_US;
    if(!s_manual && (failure==WIFI_FAILURE_AUTH || s_attempts>=ATTEMPTS_PER_NETWORK)){
        s_attempts=0;
        ++s_failed_slots;
        if(wifiSaveInfo.saveNumAll && wifiSaveInfo.saveNumAll<=10){
            if(s_failed_slots>=wifiSaveInfo.saveNumAll){
                s_failed_slots=0;s_retry_at=now+ROUND_DELAY_US;
                load_saved(wifiSaveInfo.selectWifiNow);
            }else load_saved(s_saved_slot+1);
        }else s_retry_at=now+ROUND_DELAY_US;
    }
    WifiConnectingState=2;
    WifiConnectionDetail=WIFI_DETAIL_RETRY;
    if(disconnect_first)switch_action(DO_RETRY);else execute_action(DO_RETRY);
}

static void finish_scan(uint8_t failed){
    if(!s_scan_started)return;
    /* Fetching once also frees the driver's results, even after leaving the page. */
    uint16_t count=DEFAULT_SCAN_LIST_SIZE;
    const esp_err_t result=esp_wifi_scan_get_ap_records(&count,ap_info);
    if(result!=ESP_OK){esp_wifi_clear_ap_list();count=0;failed=1;}
    wifiListNum=failed?0:count;
    WifiScanFailed=failed;
    s_scan_started=0;ScaningHd=0;refrushScanHd=1;
    if(s_connect_after_scan){
        s_connect_after_scan=0;
        switch_action(DO_CONNECT);
    }else if(!IPAdressGet){
        if(!s_manual)load_saved(wifiSaveInfo.selectWifiNow);
        if(s_valid){s_attempts=0;s_manual_deadline=esp_timer_get_time()+MANUAL_TIMEOUT_US;switch_action(DO_CONNECT);}
        else WifiConnectingState=0;
    }
}

static void process_event(const WifiEvent *event){
    if(event->kind==EV_START){
        s_started=1;
        execute_action(s_after_disconnect);
    }else if(event->kind==EV_SCAN) finish_scan(event->status!=0);
    else if(event->kind==EV_DISCONNECT){
        IPAdressGet=0;
        if(s_phase==PH_SETTLE){execute_action(s_after_disconnect);return;}
        if(s_phase==PH_ASSOC || s_phase==PH_DHCP || s_phase==PH_ONLINE){
            s_phase=PH_IDLE;
            ESP_LOGW(TAG,"Wi-Fi disconnected: reason=%u",event->reason);
            connection_failed(failure_from_reason(event->reason),0);
        }
    }else if(event->kind==EV_ASSOC && s_phase==PH_ASSOC &&
             memcmp(event->ssid,wifi_config.sta.ssid,sizeof(event->ssid))==0){
        s_phase=PH_DHCP;
        s_deadline=esp_timer_get_time()+DHCP_TIMEOUT_US;
        WifiConnectionDetail=WIFI_DETAIL_DHCP;
    }else if(event->kind==EV_IP && event->ip &&
             (s_phase==PH_ASSOC || s_phase==PH_DHCP || s_phase==PH_ONLINE)){
        /* Confirm the current AP: a late IP event from a replaced attempt must
         * never save the newly typed password as though it had succeeded. */
        if(s_connect_after_scan)return;
        wifi_ap_record_t current={0};
        if(esp_wifi_sta_get_ap_info(&current)!=ESP_OK)return;
        const size_t length=strnlen((char *)wifi_config.sta.ssid,32);
        if(strnlen((char *)current.ssid,33)!=length ||
           memcmp(current.ssid,wifi_config.sta.ssid,length)!=0)return;
        char ssid[33]={0},password[64]={0};
        memcpy(ssid,wifi_config.sta.ssid,32);
        memcpy(password,wifi_config.sta.password,63);
        uint8_t can_save=wifiSaveInfo.saveNumAll<10;
        for(u8 i=0;i<wifiSaveInfo.saveNumAll && i<10;++i)
            if(strncmp(wifiSaveInfo.ssid[i],ssid,33)==0)can_save=1;
        const u8 saved=writeWIFIInfo(ssid,password);
        WifiSaveState=saved?WIFI_SAVE_OK:(can_save?WIFI_SAVE_ERROR:WIFI_SAVE_FULL);
        if(saved)s_saved_slot=wifiSaveInfo.selectWifiNow;
        const uint8_t became_online=s_phase!=PH_ONLINE || !IPAdressGet;
        IPAdressGet=event->ip;
        s_phase=PH_ONLINE;s_manual=0;s_attempts=0;s_failed_slots=0;
        WifiConnectingState=3;WifiConnectionDetail=WIFI_DETAIL_IDLE;WifiConnectionFailure=WIFI_FAILURE_NONE;
        set_power_save(WIFI_PS_MIN_MODEM);
        if(became_online)weatherForecastRequest(0);
        ESP_LOGI(TAG,"Wi-Fi ready; credentials %s",saved?"saved":"not saved");
    }else if(event->kind==EV_LOST_IP && s_phase==PH_ONLINE)
        connection_failed(WIFI_FAILURE_DHCP,1);
}

void wifiProcessSavedConnection(void){
    if(!s_events)return;
    WifiEvent event;
    while(xQueueReceive(s_events,&event,0)==pdTRUE)process_event(&event);
    const int64_t now=esp_timer_get_time();
    if(atomic_exchange(&s_event_overflow,false)){
        ESP_LOGW(TAG,"Recovering Wi-Fi event queue overflow");
        if(s_scan_started){esp_wifi_scan_stop();finish_scan(1);}
        connection_failed(WIFI_FAILURE_CONNECTION,1);
    }
    if(s_scan_started && now>=s_scan_deadline){esp_wifi_scan_stop();finish_scan(1);}
    if(s_phase==PH_SETTLE && now>=s_deadline)execute_action(s_after_disconnect);
    else if(s_phase==PH_ASSOC || s_phase==PH_DHCP){
        if(now>=s_deadline || (s_manual && now>=s_manual_deadline))
            connection_failed(s_phase==PH_DHCP?WIFI_FAILURE_DHCP:WIFI_FAILURE_TIMEOUT,1);
    }else if(s_phase==PH_RETRY && !ScaningHd && now>=s_retry_at){
        if(s_manual && now>=s_manual_deadline)execute_action(DO_FAIL);
        else execute_action(DO_CONNECT);
    }
}

void wifi_startConnect(void){
    if(!s_valid){WifiConnectingState=4;WifiConnectionFailure=WIFI_FAILURE_CONFIG;return;}
    if(IPAdressGet && s_phase==PH_ONLINE && !ScaningHd &&
       strlen(s_requested_ssid)==strnlen((char *)wifi_config.sta.ssid,32) &&
       memcmp(s_requested_ssid,wifi_config.sta.ssid,strlen(s_requested_ssid))==0 &&
       strlen(s_requested_password)==strnlen((char *)wifi_config.sta.password,64) &&
       memcmp(s_requested_password,wifi_config.sta.password,strlen(s_requested_password))==0){
        WifiConnectingState=3;WifiConnectionFailure=WIFI_FAILURE_NONE;
        WifiConnectionDetail=WIFI_DETAIL_IDLE;
        return; // Choosing the already connected network needs no new handshake.
    }
    s_manual=1;s_attempts=0;s_failed_slots=0;s_saved_slot=-1;
    s_manual_deadline=esp_timer_get_time()+MANUAL_TIMEOUT_US;
    WifiConnectingState=1;WifiConnectionFailure=WIFI_FAILURE_NONE;
    WifiConnectionDetail=WIFI_DETAIL_ASSOC;WifiSaveState=WIFI_SAVE_NONE;
    if(ScaningHd){s_connect_after_scan=1;return;}
    switch_action(DO_CONNECT);
}

void wifi_forgetSaved(char *ssid){
    if(ssid && (!s_valid || strncmp(s_requested_ssid,ssid,33)!=0))return;
    if(!ssid && s_scan_started){
        esp_wifi_scan_stop();esp_wifi_clear_ap_list();s_scan_started=ScaningHd=0;
    }
    s_valid=s_manual=s_connect_after_scan=0;s_saved_slot=-1;
    WifiConnectingState=0;WifiSaveState=WIFI_SAVE_NONE;
    WifiConnectionDetail=WIFI_DETAIL_IDLE;WifiConnectionFailure=WIFI_FAILURE_NONE;
    switch_action(DO_NOTHING); // A late IP/retry must not re-add a forgotten row.
}

void wifi_connect_default(void){
    if(IPAdressGet || s_phase==PH_ASSOC || s_phase==PH_DHCP || s_phase==PH_SETTLE)return;
    if(!load_saved(wifiSaveInfo.selectWifiNow))return;
    s_manual=0;s_attempts=0;s_failed_slots=0;
    WifiConnectingState=1;WifiConnectionDetail=WIFI_DETAIL_ASSOC;
    WifiConnectionFailure=WIFI_FAILURE_NONE;
    if(ScaningHd){s_connect_after_scan=1;return;}
    switch_action(DO_CONNECT);
}

void wifi_scan(void){
    if(ScaningHd)return;
    ScaningHd=1;refrushScanHd=0;wifiListNum=0;WifiScanFailed=0;
    memset(ap_info,0,sizeof(ap_info));
    if(!s_started){s_after_disconnect=DO_SCAN;return;}
    if(IPAdressGet){
        /* A connected background scan must keep the current link and address. */
        const WifiPhase previous=s_phase;
        execute_action(DO_SCAN);
        if(IPAdressGet)s_phase=previous;
    }else switch_action(DO_SCAN);
}

void wifi_Init(void){
    WifiConnectingState=0;IPAdressGet=0;
    WifiConnectionDetail=WIFI_DETAIL_IDLE;WifiConnectionFailure=WIFI_FAILURE_NONE;
    WifiSaveState=WIFI_SAVE_NONE;WifiScanFailed=0;
    s_started=s_manual=s_valid=s_attempts=s_failed_slots=0;
    s_scan_started=s_connect_after_scan=ScaningHd=refrushScanHd=0;
    s_phase=PH_IDLE;s_after_disconnect=DO_NOTHING;s_saved_slot=-1;
    atomic_store(&s_event_overflow,false);
    s_events=xQueueCreate(16,sizeof(WifiEvent));configASSERT(s_events);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    esp_event_handler_instance_t wifi_instance,ip_instance;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&event_handler,NULL,&wifi_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,ESP_EVENT_ANY_ID,&event_handler,NULL,&ip_instance));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    set_power_save(WIFI_PS_MIN_MODEM);
    wifi_connect_default();
    ESP_ERROR_CHECK(esp_wifi_start());
}
