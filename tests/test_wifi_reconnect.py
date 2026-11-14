"""Exercise real reconnect callbacks and serialized Wi-Fi selection across boots."""

from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def read(name):
    return (ROOT / name).read_bytes().decode("gb18030")


def function(source, signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


rom, header = read("main/rom.c"), read("main/rom.h")
record = re.search(r"typedef struct WIFISAVEINFO\{.*?\}WIFISAVEINFO;", header, re.S).group()
storage = "\n".join(function(rom, signature) for signature in (
    "u8 writeWifi(void)", "void rememberWIFISelection(u8 index)",
    "u8 writeWIFIInfo(", "void deleteSavedWIFI(u8 num)"))
connection = re.sub(r'^#include[^\n]*\n', '', read("main/NET/connect.c"), flags=re.M)

harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <stdbool.h>
typedef uint8_t u8;
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERROR_CHECK(call) assert((call)==ESP_OK)
#define BIT0 1
#define BIT1 2
#define DEFAULT_SCAN_LIST_SIZE 20
#define WIFI_IF_STA 0
#define WIFI_MODE_STA 1
#define WIFI_PS_MAX_MODEM 2
#define WIFI_STORAGE_RAM 1
#define ESP_EVENT_ANY_ID -1
#define WIFI_EVENT ((void *)1)
#define IP_EVENT ((void *)2)
enum { WIFI_EVENT_STA_START, WIFI_EVENT_STA_DISCONNECTED, IP_EVENT_STA_GOT_IP };
enum { PAGE_HOME, PAGE_WIFI_CONNECT };
static u8 pageNow=PAGE_HOME;
typedef void *esp_event_base_t;
typedef int esp_event_handler_instance_t;
typedef void (*event_fn)(void *,esp_event_base_t,int32_t,void *);
typedef struct { unsigned reason; } wifi_event_sta_disconnected_t;
typedef struct { struct { struct { uint32_t addr; } ip; } ip_info; } ip_event_got_ip_t;
#define IPSTR "ip"
#define IP2STR(ip) 0
static void logSink(const char *tag,const char *format,...){ (void)tag; (void)format; }
#define ESP_LOGI(...) logSink(__VA_ARGS__)
#define ESP_LOGW(...) logSink(__VA_ARGS__)
typedef struct { struct { unsigned char ssid[32],password[64]; int listen_interval; } sta; } wifi_config_t;
typedef struct { int unused; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
typedef struct { char ssid[33]; } wifi_ap_record_t;
typedef int EventGroupHandle_t;
static event_fn wifiEvents;
static int driverStarted,connectCalls,configCalls,storageMode,weatherRequests;
static unsigned eventBits;
static wifi_config_t driverConfig;
static int attemptedSlots[128];
__RECORD__
static WIFISAVEINFO wifiSaveInfo;
static u8 savedWifiSelection=255;
static unsigned char flash[4096];
static void *Wifi_res=flash;
static int flashWrites;
#define wifiInfoAdress 0
static int esp_partition_erase_range(void *partition,size_t offset,size_t size){
    assert(partition==Wifi_res && offset+size<=sizeof(flash));
    memset(flash+offset,255,size); ++flashWrites; return ESP_OK;
}
static int esp_partition_write(void *partition,size_t offset,const void *data,size_t size){
    assert(partition==Wifi_res && offset+size<=sizeof(flash));
    memcpy(flash+offset,data,size); return ESP_OK;
}
static void updateWifiTxt(void){}
__STORAGE__

static const char *esp_err_to_name(int error){ (void)error; return "error"; }
static int xEventGroupCreate(void){ eventBits=0; return 1; }
static void xEventGroupSetBits(int group,int bits){ (void)group; eventBits|=bits; }
static void xEventGroupClearBits(int group,int bits){ (void)group; eventBits&=~bits; }
static int esp_netif_init(void){ return 0; }
static int esp_event_loop_create_default(void){ return 0; }
static void esp_netif_create_default_wifi_sta(void){}
static int esp_wifi_init(wifi_init_config_t *cfg){ (void)cfg; return 0; }
static int esp_wifi_set_storage(int mode){ storageMode=mode; return 0; }
static int esp_event_handler_instance_register(esp_event_base_t base,int id,event_fn fn,void *arg,int *instance){
    (void)base;(void)id;(void)arg;(void)instance;wifiEvents=fn;return 0;
}
static int esp_wifi_set_mode(int mode){ (void)mode;return 0; }
static int esp_wifi_set_config(int interface,wifi_config_t *cfg){
    (void)interface;driverConfig=*cfg;++configCalls;return 0;
}
static int esp_wifi_connect(void){
    assert(driverStarted && driverConfig.sta.ssid[0]);
    assert(connectCalls<(int)(sizeof(attemptedSlots)/sizeof(attemptedSlots[0])));
    attemptedSlots[connectCalls++]=memcmp(driverConfig.sta.ssid,"SavedA",6)==0 ? 0 : 1;
    return 0;
}
static int esp_wifi_start(void){
    driverStarted=1;
    wifiEvents(NULL,WIFI_EVENT,WIFI_EVENT_STA_START,NULL);
    return 0;
}
static int esp_wifi_set_ps(int mode){ (void)mode;return 0; }
static int esp_wifi_disconnect(void){
    wifi_event_sta_disconnected_t event={.reason=8};
    wifiEvents(NULL,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,&event);return 0;
}
static int esp_wifi_scan_start(void *config,int block){ (void)config;(void)block;return 0; }
static int esp_wifi_scan_get_ap_num(uint16_t *count){ *count=0;return 0; }
static int esp_wifi_scan_get_ap_records(uint16_t *count,wifi_ap_record_t *records){ (void)records;*count=0;return 0; }
static void weatherForecastRequest(int force){ (void)force;++weatherRequests; }
void resetWIFIConnectInfo(char *,char *);
void wifi_startConnect(void);
void wifi_connect_default(void);
void wifiProcessSavedConnection(void);
extern uint8_t ScaningHd;
__CONNECTION__

static void fail(void){
    wifi_event_sta_disconnected_t event={.reason=201};
    event_handler(NULL,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,&event);
}
static void gotIp(void){
    ip_event_got_ip_t event={0};event.ip_info.ip.addr=1234;
    event_handler(NULL,IP_EVENT,IP_EVENT_STA_GOT_IP,&event);
}
static void boot(void){
    memcpy(&wifiSaveInfo,flash,sizeof(wifiSaveInfo));
    savedWifiSelection=wifiSaveInfo.selectWifiNow;
    IPAdressGet=0;ScaningHd=0;driverStarted=0;connectCalls=0;configCalls=0;
    memset(&wifi_config,0,sizeof(wifi_config));
    wifi_Init();
}
int main(void){
    assert(sizeof(WIFISAVEINFO)==972); // Existing flash layout stays compatible.
    wifiSaveInfo.saveNumAll=2;
    strcpy(wifiSaveInfo.ssid[0],"SavedA");strcpy(wifiSaveInfo.password[0],"password-a");
    strcpy(wifiSaveInfo.ssid[1],"SavedB");strcpy(wifiSaveInfo.password[1],"password-b");
    wifiSaveInfo.selectWifiNow=0;writeWifi();flashWrites=0;
    boot();
    assert(storageMode==WIFI_STORAGE_RAM && connectCalls==1 && attemptedSlots[0]==0);
    assert(configCalls==1 && WifiConnectingState==1 && wifiSaveInfo.selectWifiNow==0);
    assert(flashWrites==0);
    fail();assert(connectCalls==2 && attemptedSlots[1]==0);
    fail();assert(connectCalls==3 && attemptedSlots[2]==1 && wifiSaveInfo.selectWifiNow==1);
    assert(flashWrites==0); // Failed candidates do not overwrite the saved preference.
    gotIp();assert(IPAdressGet && WifiConnectingState==3 && weatherRequests==1);
    assert(flashWrites==0); // Event callback leaves persistence to the render task.
    wifiProcessSavedConnection();assert(flashWrites==1 && flash[1]==1);
    wifiProcessSavedConnection();assert(flashWrites==1);
    boot();assert(connectCalls==1 && attemptedSlots[0]==1 && wifiSaveInfo.selectWifiNow==1);
    fail();fail();assert(attemptedSlots[2]==0);
    gotIp();wifiProcessSavedConnection();assert(flash[1]==0);
    boot();assert(attemptedSlots[0]==0); // Next reboot immediately uses the successful AP.

    const int attempts=connectCalls;
    wifi_scan();assert(connectCalls==attempts && WifiConnectingState==0 && !ScaningHd);
    fail();assert(connectCalls==attempts); // Scan/cancel stays stopped.
    pageNow=PAGE_WIFI_CONNECT;wifiSaveInfo.selectWifiNow=1;
    resetWIFIConnectInfo(wifiSaveInfo.ssid[1],wifiSaveInfo.password[1]);
    wifi_startConnect();
    const int manual=connectCalls;
    for(int i=0;i<5;i++) fail();
    assert(connectCalls==manual+5 && wifiSaveInfo.selectWifiNow==1);
    fail();assert(WifiConnectingState==4 && (eventBits&WIFI_FAIL_BIT));
    pageNow=PAGE_HOME;

    resetWIFIConnectInfo("A","");wifi_startConnect(); // Single-character and open SSIDs are valid.
    assert(driverConfig.sta.ssid[0]=='A' && driverConfig.sta.ssid[1]==0 && !driverConfig.sta.password[0]);
    resetWIFIConnectInfo("","");const int invalid=connectCalls;wifi_startConnect();
    assert(connectCalls==invalid && WifiConnectingState==4);
    memset(&wifiSaveInfo,0,sizeof(wifiSaveInfo));writeWifi();
    boot();assert(connectCalls==0 && WifiConnectingState==0);
    wifi_connect_default();assert(connectCalls==0);

    for(int i=0;i<10;i++){
        char ssid[33];snprintf(ssid,sizeof(ssid),"Saved%d",i);
        writeWIFIInfo(ssid,"password");
    }
    assert(wifiSaveInfo.saveNumAll==10 && wifiSaveInfo.selectWifiNow==9);
    writeWIFIInfo("Saved9","updated-password");
    assert(wifiSaveInfo.saveNumAll==10 && strcmp(wifiSaveInfo.password[9],"updated-password")==0);
    WIFISAVEINFO full=wifiSaveInfo;
    const int fullWrites=flashWrites;
    assert(!writeWIFIInfo("Eleventh","password"));
    assert(memcmp(&full,&wifiSaveInfo,sizeof(full))==0 && flashWrites==fullWrites);
    assert(writeWIFIInfo(wifiSaveInfo.ssid[9],wifiSaveInfo.password[9]));
    assert(strcmp(wifiSaveInfo.ssid[9],"Saved9")==0 && strcmp(wifiSaveInfo.password[9],"updated-password")==0);
    const int before=flashWrites;
    rememberWIFISelection(3);assert(flash[1]==3 && flashWrites==before+1);
    rememberWIFISelection(3);assert(flashWrites==before+1);
    deleteSavedWIFI(0);assert(wifiSaveInfo.selectWifiNow==2 && flash[1]==2);
    deleteSavedWIFI(2);assert(wifiSaveInfo.selectWifiNow==0 && flash[1]==0);
    puts("PASS: preferred Wi-Fi persists, saved AP fallback, safe startup, scan/manual retry limits, and full-list updates");
}
'''

for key, value in {"__RECORD__": record, "__STORAGE__": storage, "__CONNECTION__": connection}.items():
    harness = harness.replace(key, value)

with tempfile.TemporaryDirectory(prefix="gview-wifi-reconnect-") as directory:
    source, binary = Path(directory) / "test.c", Path(directory) / "test"
    source.write_text(harness)
    subprocess.run(["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-parameter", "-fsanitize=address,undefined",
                    str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
