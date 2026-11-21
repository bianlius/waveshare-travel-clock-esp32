"""Run production Wi-Fi events, deadlines, scans and persistence against a fake driver."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
def read(name):
    return (ROOT/name).read_bytes().decode('gb18030')
def function(source,signature):
    start=source.index(signature)
    end=source.index('{',start)+1
    depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}')
        end+=1
    return source[start:end]+'\n'
rom,header=read('main/rom.c'),read('main/rom.h')
record=re.search(r'typedef struct WIFISAVEINFO\{.*?\}WIFISAVEINFO;',header,re.S).group()
storage='\n'.join(function(rom,sig) for sig in ('u8 writeWifi(void)','void rememberWIFISelection(u8 index)','u8 writeWIFIInfo(','void deleteSavedWIFI(u8 num)'))
connection=re.sub(r'^#include[^\n]*\n','',read('main/NET/connect.c'),flags=re.M)
enums='\n'.join(re.findall(r'enum \{.*?\};',read('main/NET/connect.h'),re.S))
harness=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdbool.h>
typedef uint8_t u8;
typedef int esp_err_t;
typedef int wifi_ps_type_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERROR_CHECK(call) assert((call)==ESP_OK)
#define configASSERT assert
#define pdTRUE 1
#define DEFAULT_SCAN_LIST_SIZE 20
#define WIFI_IF_STA 0
#define WIFI_MODE_STA 1
#define WIFI_PS_NONE 0
#define WIFI_PS_MIN_MODEM 1
#define WIFI_STORAGE_RAM 1
#define WIFI_FAST_SCAN 0
#define WIFI_CONNECT_AP_BY_SIGNAL 0
#define WIFI_SCAN_TYPE_ACTIVE 0
#define ESP_EVENT_ANY_ID -1
#define WIFI_EVENT ((void *)1)
#define IP_EVENT ((void *)2)
enum { WIFI_EVENT_STA_START, WIFI_EVENT_STA_DISCONNECTED,WIFI_EVENT_STA_CONNECTED,
       WIFI_EVENT_SCAN_DONE, IP_EVENT_STA_GOT_IP, IP_EVENT_STA_LOST_IP };
enum { WIFI_REASON_AUTH_FAIL=202, WIFI_REASON_NO_AP_FOUND=201,
       WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT=15,WIFI_REASON_HANDSHAKE_TIMEOUT=204 };
__ENUMS__
typedef void *esp_event_base_t;
typedef int esp_event_handler_instance_t;
typedef void (*event_fn)(void *,esp_event_base_t,int32_t,void *);
typedef struct { unsigned reason; } wifi_event_sta_disconnected_t;
typedef struct { unsigned char ssid[32],ssid_len; } wifi_event_sta_connected_t;
typedef struct { unsigned status; } wifi_event_sta_scan_done_t;
typedef struct { struct { struct { uint32_t addr; } ip; } ip_info; } ip_event_got_ip_t;
static void logSink(const char *tag,const char *text,...){(void)tag;(void)text;}
#define ESP_LOGI(...) logSink(__VA_ARGS__)
#define ESP_LOGW(...) logSink(__VA_ARGS__)
typedef struct { struct { unsigned char ssid[32],password[64];int listen_interval,scan_method,sort_method; } sta; } wifi_config_t;
typedef struct { int unused; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
typedef struct { unsigned char ssid[33]; } wifi_ap_record_t;
typedef struct { int scan_type;struct { struct {unsigned min,max;} active;} scan_time;unsigned home_chan_dwell_time; } wifi_scan_config_t;
typedef struct {unsigned char *data;size_t size;unsigned capacity,count,read,write;} FakeQueue;
typedef FakeQueue *QueueHandle_t;
static QueueHandle_t allocatedQueue;
static QueueHandle_t xQueueCreate(unsigned capacity,size_t size){
    if(allocatedQueue){free(allocatedQueue->data);free(allocatedQueue);}
    allocatedQueue=calloc(1,sizeof(*allocatedQueue));assert(allocatedQueue);
    allocatedQueue->data=calloc(capacity,size);assert(allocatedQueue->data);
    allocatedQueue->capacity=capacity;allocatedQueue->size=size;return allocatedQueue;
}
static int xQueueSend(QueueHandle_t q,const void *item,unsigned ticks){
    assert(ticks==0);if(q->count==q->capacity)return 0;
    memcpy(q->data+q->write*q->size,item,q->size);q->write=(q->write+1)%q->capacity;++q->count;return pdTRUE;
}
static int xQueueReceive(QueueHandle_t q,void *item,unsigned ticks){
    assert(ticks==0);if(!q->count)return 0;
    memcpy(item,q->data+q->read*q->size,q->size);q->read=(q->read+1)%q->capacity;--q->count;return pdTRUE;
}
static int64_t fakeNow;
static int64_t esp_timer_get_time(void){return fakeNow;}
static event_fn wifiEvents;
static int driverStarted,driverConnecting,driverAssociated,driverScanning;
static int connectCalls,configCalls,storageMode,powerMode,weatherRequests,disconnectCalls;
static int connectError,configError,scanError,scanFetchError,emitDisconnect=1;
static int scanCalls,scanFetches,scanStops,scanClears;
static wifi_config_t driverConfig;
static wifi_ap_record_t driverAP;
static unsigned char attemptedNames[512][33];
__RECORD__
static WIFISAVEINFO wifiSaveInfo;
static u8 savedWifiSelection=255,wifiSaveDirty;
static unsigned char flash[4096];
static void *Wifi_res=flash;
static int flashWrites,eraseError;
#define wifiInfoAdress 0
static int esp_partition_erase_range(void *partition,size_t offset,size_t size){
    assert(partition==Wifi_res && offset+size<=sizeof(flash));
    if(eraseError)return ESP_FAIL;
    memset(flash+offset,255,size);++flashWrites;return ESP_OK;
}
static int esp_partition_write(void *partition,size_t offset,const void *data,size_t size){
    assert(partition==Wifi_res && offset+size<=sizeof(flash));memcpy(flash+offset,data,size);return ESP_OK;
}
static void updateWifiTxt(void){}
__STORAGE__
static const char *esp_err_to_name(int error){(void)error;return "error";}
static int esp_netif_init(void){return 0;}
static int esp_event_loop_create_default(void){return 0;}
static void esp_netif_create_default_wifi_sta(void){}
static int esp_wifi_init(wifi_init_config_t *cfg){(void)cfg;return 0;}
static int esp_wifi_set_storage(int mode){storageMode=mode;return 0;}
static int esp_event_handler_instance_register(esp_event_base_t base,int id,event_fn fn,void *arg,int *instance){
    (void)base;(void)id;(void)arg;(void)instance;wifiEvents=fn;return 0;
}
static int esp_wifi_set_mode(int mode){(void)mode;return 0;}
static int esp_wifi_set_config(int interface,wifi_config_t *cfg){
    (void)interface;assert(!driverConnecting && !driverAssociated);
    if(configError)return ESP_FAIL;driverConfig=*cfg;++configCalls;return 0;
}
static int esp_wifi_connect(void){
    assert(driverStarted && driverConfig.sta.ssid[0] && !driverScanning);
    assert(connectCalls<512);memcpy(attemptedNames[connectCalls++],driverConfig.sta.ssid,32);
    if(connectError)return ESP_FAIL;driverConnecting=1;return 0;
}
static int esp_wifi_start(void){driverStarted=1;wifiEvents(NULL,WIFI_EVENT,WIFI_EVENT_STA_START,NULL);return 0;}
static int esp_wifi_set_ps(int mode){powerMode=mode;return 0;}
static int esp_wifi_disconnect(void){
    ++disconnectCalls;
    const int active=driverConnecting||driverAssociated;
    driverConnecting=driverAssociated=0;
    if(active && emitDisconnect){wifi_event_sta_disconnected_t e={.reason=8};wifiEvents(NULL,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,&e);}
    return 0;
}
static int esp_wifi_scan_start(const wifi_scan_config_t *cfg,int block){
    assert(!block && cfg && cfg->scan_time.active.max==120 && cfg->home_chan_dwell_time==30);
    assert(!driverConnecting);++scanCalls;if(scanError)return ESP_FAIL;driverScanning=1;return 0;
}
static int esp_wifi_scan_stop(void){driverScanning=0;++scanStops;return 0;}
static int esp_wifi_scan_get_ap_records(uint16_t *count,wifi_ap_record_t *records){
    ++scanFetches;assert(*count==20);if(scanFetchError)return ESP_FAIL;
    *count=2;memcpy(records[0].ssid,"NearbyA",8);memcpy(records[1].ssid,"NearbyB",8);return 0;
}
static int esp_wifi_clear_ap_list(void){++scanClears;return 0;}
static int esp_wifi_sta_get_ap_info(wifi_ap_record_t *ap){if(!driverAssociated)return ESP_FAIL;*ap=driverAP;return 0;}
static void weatherForecastRequest(int clear){(void)clear;++weatherRequests;}
void resetWIFIConnectInfo(char *,char *);
void wifi_startConnect(void);
void wifi_connect_default(void);
void wifiProcessSavedConnection(void);
__CONNECTION__

static void poll(void){wifiProcessSavedConnection();}
static void advance(int64_t us){fakeNow+=us;poll();}
static void failed(unsigned reason){
    driverConnecting=driverAssociated=0;
    wifi_event_sta_disconnected_t e={.reason=reason};wifiEvents(NULL,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,&e);poll();
}
static void associated(void){
    driverConnecting=0;driverAssociated=1;memset(&driverAP,0,sizeof(driverAP));memcpy(driverAP.ssid,driverConfig.sta.ssid,32);
    wifi_event_sta_connected_t e={0};e.ssid_len=strnlen((char *)driverConfig.sta.ssid,32);memcpy(e.ssid,driverConfig.sta.ssid,32);
    wifiEvents(NULL,WIFI_EVENT,WIFI_EVENT_STA_CONNECTED,&e);poll();
}
static void gotIp(void){ip_event_got_ip_t e={0};e.ip_info.ip.addr=1234;wifiEvents(NULL,IP_EVENT,IP_EVENT_STA_GOT_IP,&e);}
static void scanned(int status){driverScanning=0;wifi_event_sta_scan_done_t e={.status=status};wifiEvents(NULL,WIFI_EVENT,WIFI_EVENT_SCAN_DONE,&e);poll();}
static void boot(void){
    memcpy(&wifiSaveInfo,flash,sizeof(wifiSaveInfo));savedWifiSelection=wifiSaveInfo.selectWifiNow;wifiSaveDirty=0;
    fakeNow=1000000;driverStarted=driverConnecting=driverAssociated=driverScanning=0;
    connectCalls=configCalls=weatherRequests=disconnectCalls=0;
    connectError=configError=scanError=scanFetchError=eraseError=0;emitDisconnect=1;
    scanCalls=scanFetches=scanStops=scanClears=0;memset(attemptedNames,0,sizeof(attemptedNames));
    memset(&driverConfig,0,sizeof(driverConfig));wifi_Init();
    assert(!connectCalls); // Event task cannot initiate connects or write Flash.
    poll();
}
static void emptySaved(void){memset(&wifiSaveInfo,0,sizeof(wifiSaveInfo));assert(writeWifi());}
static void manual(char *ssid,char *password){resetWIFIConnectInfo(ssid,password);wifi_startConnect();poll();}
int main(void){
    assert(sizeof(WIFISAVEINFO)==972);
    emptySaved();boot();assert(!connectCalls && WifiConnectingState==0);
    char name[]="NewNetwork",password[]="first-password";
    manual(name,password);assert(connectCalls==1 && WifiConnectingState==1 && powerMode==WIFI_PS_NONE);
    memset(name,'x',sizeof(name)-1);memset(password,'x',sizeof(password)-1);
    associated();assert(WifiConnectionDetail==WIFI_DETAIL_DHCP);
    const int before=flashWrites;gotIp();assert(flashWrites==before && !wifiSaveInfo.saveNumAll);
    /* Returning home and reusing the text input cannot lose credentials. */
    poll();assert(WifiConnectingState==3 && IPAdressGet && powerMode==WIFI_PS_MIN_MODEM);
    assert(WifiSaveState==WIFI_SAVE_OK && wifiSaveInfo.saveNumAll==1);
    assert(strcmp(wifiSaveInfo.ssid[0],"NewNetwork")==0 && strcmp(wifiSaveInfo.password[0],"first-password")==0);
    assert(flashWrites==before+1 && weatherRequests==1);
    gotIp();poll();assert(flashWrites==before+1 && weatherRequests==1); // Renewal does not restart weather.
    const int sameCalls=connectCalls,sameDisconnects=disconnectCalls;
    manual("NewNetwork","first-password");
    assert(WifiConnectingState==3 && connectCalls==sameCalls && disconnectCalls==sameDisconnects);
    failed(8);advance(RETRY_DELAY_US);associated();gotIp();poll();
    assert(weatherRequests==2 && flashWrites==before+1); // A real reconnect fetches immediately.
    boot();assert(connectCalls==1 && !strcmp((char *)attemptedNames[0],"NewNetwork"));
    associated();gotIp();poll();assert(flashWrites==before+1);

    assert(writeWIFIInfo("OtherNetwork","other-password"));
    boot();assert(!strcmp((char *)attemptedNames[0],"OtherNetwork"));
    const int savedWrites=flashWrites;
    failed(WIFI_REASON_NO_AP_FOUND);assert(connectCalls==1);advance(RETRY_DELAY_US);
    assert(connectCalls==2 && !strcmp((char *)attemptedNames[1],"OtherNetwork"));
    failed(WIFI_REASON_NO_AP_FOUND);advance(RETRY_DELAY_US);
    assert(connectCalls==3 && !strcmp((char *)attemptedNames[2],"NewNetwork"));
    assert(wifiSaveInfo.selectWifiNow==1 && flashWrites==savedWrites);
    associated();gotIp();poll();assert(wifiSaveInfo.selectWifiNow==0 && flash[1]==0);
    boot();assert(!strcmp((char *)attemptedNames[0],"NewNetwork"));
    /* Neither manual error policy nor persistence depends on the current page. */
    manual("TypedNetwork","bad-password");failed(WIFI_REASON_AUTH_FAIL);
    assert(WifiConnectingState==4 && WifiConnectionFailure==WIFI_FAILURE_AUTH);
    assert(wifiSaveInfo.saveNumAll==2 && wifiSaveInfo.selectWifiNow==0);
    const int authCalls=connectCalls;advance(60000000);assert(connectCalls==authCalls);
    scanError=1;wifi_scan();poll();assert(WifiScanFailed && !ScaningHd);
    scanError=0;advance(RETRY_DELAY_US);
    assert(connectCalls==authCalls+1 && !strcmp((char *)driverConfig.sta.ssid,"NewNetwork"));
    wifi_scan();poll();scanned(0);
    assert(!strcmp((char *)driverConfig.sta.ssid,"NewNetwork"));

    manual("TypedNetwork","correct-password");associated();gotIp();poll();
    assert(wifiSaveInfo.saveNumAll==3 && wifiSaveInfo.selectWifiNow==2);
    boot();assert(!strcmp((char *)attemptedNames[0],"TypedNetwork"));
    /* Existing link survives asynchronous background discovery. */
    associated();gotIp();poll();const int connectedCalls=connectCalls,disconnected=disconnectCalls;
    wifi_scan();assert(ScaningHd && driverScanning && disconnectCalls==disconnected && IPAdressGet);
    assert(scanCalls==1 && !refrushScanHd);
    scanned(0);assert(!ScaningHd && refrushScanHd && wifiListNum==2 && scanFetches==1);
    assert(WifiConnectingState==3 && connectCalls==connectedCalls && IPAdressGet);
    /* A manual choice made while scanning waits, then replaces the old AP. */
    wifi_scan();manual("ChosenDuringScan","new-password");
    assert(driverScanning && connectCalls==connectedCalls);
    gotIp();poll();scanned(0);poll();
    assert(connectCalls==connectedCalls+1 && !strcmp((char *)driverConfig.sta.ssid,"ChosenDuringScan"));
    associated();gotIp();poll();assert(wifiSaveInfo.saveNumAll==4);

    /* Interrupted automatic connection resumes after scan, even off the page. */
    boot();const int interruptedCalls=connectCalls;
    wifi_scan();assert(ScaningHd);poll();assert(driverScanning);
    scanned(0);assert(connectCalls==interruptedCalls+1 && !ScaningHd);
    /* API failures and scan timeouts are recoverable, never ESP_ERROR_CHECK aborts. */
    associated();gotIp();poll();scanError=1;wifi_scan();
    assert(WifiScanFailed && !ScaningHd && IPAdressGet);scanError=0;
    wifi_scan();scanFetchError=1;scanned(0);assert(WifiScanFailed && scanClears==1);scanFetchError=0;
    wifi_scan();advance(SCAN_TIMEOUT_US);assert(WifiScanFailed && !ScaningHd && scanStops==1);
    const int fetches=scanFetches;scanned(1);assert(scanFetches==fetches);

    /* Association without DHCP is bounded and never saved as a success. */
    manual("NoDHCP","password");associated();advance(DHCP_TIMEOUT_US);poll();
    assert(!IPAdressGet && WifiConnectionFailure==WIFI_FAILURE_DHCP);
    advance(RETRY_DELAY_US);associated();advance(DHCP_TIMEOUT_US);poll();
    assert(WifiConnectingState==4 && WifiConnectionFailure==WIFI_FAILURE_DHCP);
    assert(wifiSaveInfo.saveNumAll==4);
    manual("Absent","password");advance(CONNECT_TIMEOUT_US);poll();advance(RETRY_DELAY_US);
    advance(CONNECT_TIMEOUT_US);poll();assert(WifiConnectingState==4 && WifiConnectionFailure==WIFI_FAILURE_TIMEOUT);
    /* Driver configuration errors produce a retryable result. */
    configError=1;manual("ConfigError","password");assert(WifiConnectingState==4);configError=0;
    /* With a lost disconnect notification, the settle deadline releases a switch. */
    manual("SettlingA","password");emitDisconnect=0;manual("SettlingB","password");
    assert(!strcmp((char *)driverConfig.sta.ssid,"SettlingA"));advance(DISCONNECT_TIMEOUT_US);
    assert(!strcmp((char *)driverConfig.sta.ssid,"SettlingB"));emitDisconnect=1;
    /* A stale IP cannot save credentials for a replacement network. */
    driverAssociated=1;memset(&driverAP,0,sizeof(driverAP));strcpy((char *)driverAP.ssid,"SettlingA");
    gotIp();poll();assert(!IPAdressGet && wifiSaveInfo.saveNumAll==4);
    associated();gotIp();poll();assert(wifiSaveInfo.saveNumAll==5);

    emptySaved();boot();
    manual("A","");associated();gotIp();poll();assert(wifiSaveInfo.saveNumAll==1 && !wifiSaveInfo.password[0][0]);
    char exact32[]="中文无线网络12345678901234";assert(strlen(exact32)==32);
    manual(exact32,"password");associated();gotIp();poll();
    assert(!strcmp(wifiSaveInfo.ssid[1],exact32));boot();assert(memcmp(driverConfig.sta.ssid,exact32,32)==0);
    char tooLong[34];memset(tooLong,'x',33);tooLong[33]=0;
    const int invalidCalls=connectCalls;manual(tooLong,"password");assert(connectCalls==invalidCalls && WifiConnectingState==4);

    emptySaved();boot();
    for(unsigned i=0;i<10;++i){char n[33];snprintf(n,sizeof(n),"Saved%u",i);assert(writeWIFIInfo(n,"password"));}
    WIFISAVEINFO full=wifiSaveInfo;const int fullWrites=flashWrites;
    manual("Eleventh","password");associated();gotIp();poll();
    assert(WifiConnectingState==3 && WifiSaveState==WIFI_SAVE_FULL);
    assert(!memcmp(&full,&wifiSaveInfo,sizeof(full)) && flashWrites==fullWrites);
    manual("Saved9","updated-password");associated();gotIp();poll();
    assert(wifiSaveInfo.saveNumAll==10 && !strcmp(wifiSaveInfo.password[9],"updated-password"));
    assert(WifiSaveState==WIFI_SAVE_OK);
    /* Failure while updating a full list is an I/O error, not a full-list error. */
    eraseError=1;manual("Saved9","second-update");associated();gotIp();poll();
    assert(WifiSaveState==WIFI_SAVE_ERROR && wifiSaveDirty);eraseError=0;
    gotIp();poll();assert(WifiSaveState==WIFI_SAVE_OK && !wifiSaveDirty);
    assert(!strcmp(((WIFISAVEINFO *)flash)->password[9],"second-update"));
    deleteSavedWIFI(0);assert(wifiSaveInfo.selectWifiNow==8 && flash[1]==8);
    deleteSavedWIFI(8);assert(wifiSaveInfo.selectWifiNow==0 && flash[1]==0);
    emptySaved();assert(writeWIFIInfo("Saved0","password")); // Deleted row must really be written back.
    boot();assert(connectCalls==1 && !strcmp((char *)driverConfig.sta.ssid,"Saved0"));
    /* Once every remembered AP failed, back off instead of spinning forever. */
    const int roundCalls=connectCalls;
    failed(WIFI_REASON_NO_AP_FOUND);advance(RETRY_DELAY_US);failed(WIFI_REASON_NO_AP_FOUND);
    advance(ROUND_DELAY_US-1);assert(connectCalls==roundCalls+1);advance(1);assert(connectCalls==roundCalls+2);
    /* Forgetting a connecting/connected row cannot resurrect it from queued events. */
    associated();gotIp();poll();
    wifi_forgetSaved(wifiSaveInfo.ssid[0]);deleteSavedWIFI(0);gotIp();poll();
    advance(60000000);assert(!wifiSaveInfo.saveNumAll && WifiConnectingState==0 && !IPAdressGet);
    boot();assert(!connectCalls);
    manual("ResetNetwork","password");associated();gotIp();poll();
    wifi_forgetSaved(NULL);emptySaved();gotIp();poll();advance(60000000);
    assert(!wifiSaveInfo.saveNumAll && !IPAdressGet && WifiConnectingState==0);
    free(allocatedQueue->data);free(allocatedQueue);allocatedQueue=NULL;
    puts("PASS: background credential save/reboot, preferred fallback, no redundant Flash writes, bounded auth/DHCP/association failures, asynchronous scans/resume, late-event switching, UTF-8 and full-list/I/O handling");
}
'''
for key,value in {'__RECORD__':record,'__STORAGE__':storage,'__CONNECTION__':connection,'__ENUMS__':enums}.items():harness=harness.replace(key,value)
with tempfile.TemporaryDirectory(prefix='gview-wifi-reconnect-') as folder:
    source,binary=Path(folder)/'test.c',Path(folder)/'test'
    source.write_text(harness)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
                    '-fsanitize=address,undefined',str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
