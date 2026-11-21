/* Host services for the current firmware. UI and asset sources are compiled
 * unchanged; ESP storage is represented by separate files on this computer. */
#include "simulator.h"
#include "compat.h"
#include "config.h"
#include "rom.h"
#include "power_status.h"
#include "sound.h"
#include "sound_catalog.h"
#include "NET/connect.h"
#include "NET/RTC_Internet.h"
#include "NET/weather_forecast.h"
#include "PAGE/HOME/home.h"
#include "PAGE/GARDEN/garden.h"
#include "PAGE/SHOP/shop.h"
#include "PAGE/INPUTTXT/inputtxt.h"
#include "PAGE/WIFI/wifiScanPage.h"
#include "PAGE/WIFI/wifi_layout.h"
#include "PAGE/TCLOCK/tclock.h"
#include "GVIEW/utf8_gb2312_switch.h"
#include "LCD/lcd_init.h"
#include "GVIEW/viewport.h"
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

// Tests can change the mock state without hardware or extra UI polling.
PowerStatus desktopPowerStatus=POWER_STATUS_NONE;
PowerStatus powerStatusGet(void){return desktopPowerStatus;}
u16 VoltageVal=4100;
u8 WifiConnectingState=3,backLightNum=8,roundStateGet;
u16 KeyBtn_Val;
uint16_t KEY0Temp,KEY1Temp,KEY2Temp,KEY3Temp,KEY4Temp;
uint32_t IPAdressGet;
volatile u16 aClockTimes;
u8 gettime;
volatile u8 getweather;
char timeString[10],dateString[30];
char textDayArr[3][10],textNightArr[3][10],tempHightLowArr[3][10],humidityNum[3][5];
u8 weatherNightCode[3],weatherDayCode[3];
char weekdays[7][5]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
int year,month,day,hour=12,minute,second,weekday;
struct tm *tStruct;
wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
u8 refrushScanHd,ScaningHd;
u8 WifiConnectionDetail,WifiConnectionFailure,WifiSaveState,WifiScanFailed;
u16 wifiListNum;
char *passWordSTxt,*ssidTxt;

static char state_directory[2048],resource_directory[2048],status[256];
static uint32_t pixels[DESKTOP_WIDTH*DESKTOP_HEIGHT];
#if defined(CONFIG_GVIEW_AI_PASSPORT)
#include "passport_display.h"
#include "passport_input.h"
int passportBatteryPercent=78;
static uint16_t passport_frame[320*240];
static bool full_frame_check;
static void pixel_fixture_tick(void){rendWin(0,0,winW,winH,BGColor);}
static void capture_strip(uint16_t x,uint16_t y,uint16_t width,uint16_t height,const uint16_t *data){
    assert(x+width<=320 && y+height<=240);
    for(unsigned row=0;row<height;row++)memcpy(&passport_frame[(y+row)*320+x],data+row*width,width*2);
}
#endif
static time_t desktop_now(void){
    const char *fixed=getenv("GVIEW_TEST_TIME");return fixed?(time_t)strtoll(fixed,NULL,10):time(NULL);
}
static const uint8_t *extra_ink;
static const volatile uint8_t *extra_page;
static uint8_t extra_page_id;
static int scene_hour=12,scene_day_index,weather_index,preview_slot=-1,restarting;
static uint32_t forecast_version=1,frame_counter;
static uint8_t forecast_ready=1;
static time_t last_second;
static unsigned alarm_elapsed;
static esp_partition_t partitions[]={{"userdata",4096,0x310000,0x40},
    {"wifidata",4096,0x311000,0x50},{"sounddata",0,0x312000,0x60}};
static const int scene_hours[]={12,16,18,22,4,7};
static const struct {uint8_t code;const char *name;} weather_options[]={
    {0,"晴"},{4,"多云"},{9,"阴"},{13,"小雨"},{14,"中雨"},{15,"大雨"},{23,"小雪"}};
extern void doKeyFun(KeyName);
extern void startSenlinBgMovie(void);
extern void stopSenlinBgMovie(void);
extern u16 treeShowTime;
extern DisplayObject *phlPkm;
extern const u16 ShopListMonsterCost[];
extern const char *ShopListMonsterTxt[];
extern void (*keyFun[])(void);
#include "app_runtime.h"

static void gb(char *to,size_t size,const char *from){
    memset(to,0,size);
    utf8_to_gb2312((const uint8_t *)from,strlen(from),(uint8_t *)to,size-1);
}
static void partition_path(const esp_partition_t *p,char *path,size_t size){
    if(p->type==0x60) snprintf(path,size,"%s/sounddata.bin",resource_directory);
    else snprintf(path,size,"%s/%s.bin",state_directory,p->label);
}
int64_t esp_timer_get_time(void){
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
    return (int64_t)t.tv_sec*1000000+t.tv_nsec/1000;
}
uint32_t esp_random(void){
    static uint32_t test_seed=0x12345678;
    if(getenv("GVIEW_TEST_TIME")){test_seed=test_seed*1664525u+1013904223u;return test_seed;}
    return arc4random();
}
void vTaskDelay(unsigned ms){usleep(ms*1000);}
void esp_restart(void){restarting=1;}
void requestPowerOff(void){desktop_shutdown();exit(0);}
esp_partition_t *esp_partition_find_first(int type,int subtype,const char *label){
    (void)subtype;(void)label;
    for(unsigned i=0;i<3;i++) if(partitions[i].type==type){
        if(type==0x60){char path[4096];struct stat st;partition_path(&partitions[i],path,sizeof(path));
            if(stat(path,&st)) return NULL;partitions[i].size=(uint32_t)st.st_size;}
        return &partitions[i];
    }
    return NULL;
}
int esp_partition_read(const esp_partition_t *p,size_t offset,void *data,size_t count){
    if(!p || offset+count>p->size) return ESP_FAIL;
    char path[4096];partition_path(p,path,sizeof(path));FILE *f=fopen(path,"rb");
    if(!f){memset(data,0xFF,count);return p->type==0x60 ? ESP_FAIL : ESP_OK;}
    int result=fseek(f,(long)offset,SEEK_SET)==0 && fread(data,1,count,f)==count ? ESP_OK : ESP_FAIL;
    fclose(f);return result;
}
int esp_partition_write(const esp_partition_t *p,size_t offset,const void *data,size_t count){
    if(!p || p->type==0x60 || offset+count>p->size) return ESP_FAIL;
    char path[4096];partition_path(p,path,sizeof(path));FILE *f=fopen(path,"r+b");
    if(!f) f=fopen(path,"w+b");if(!f)return ESP_FAIL;
    int result=fseek(f,(long)offset,SEEK_SET)==0 && fwrite(data,1,count,f)==count ? ESP_OK : ESP_FAIL;
    fclose(f);return result;
}
int esp_partition_erase_range(const esp_partition_t *p,size_t offset,size_t count){
    uint8_t *blank=malloc(count);if(!blank)return ESP_FAIL;memset(blank,0xFF,count);
    int result=esp_partition_write(p,offset,blank,count);free(blank);return result;
}
int esp_partition_mmap(const esp_partition_t *p,size_t offset,size_t count,int mode,
                       const void **data,esp_partition_mmap_handle_t *handle){
    (void)mode;void *buffer=malloc(count);if(!buffer)return ESP_FAIL;
    if(esp_partition_read(p,offset,buffer,count)!=ESP_OK){free(buffer);return ESP_FAIL;}
    *data=buffer;*handle=(uintptr_t)buffer;return ESP_OK;
}
void esp_partition_munmap(esp_partition_mmap_handle_t handle){free((void *)handle);}
int nvs_flash_init(void){return ESP_OK;}
int nvs_flash_erase(void){return ESP_OK;}
static void garden_path(char *path){snprintf(path,4096,"%s/garden.bin",state_directory);}
int nvs_open(const char *name,int mode,nvs_handle_t *handle){(void)name;(void)mode;*handle=1;return ESP_OK;}
int nvs_set_blob(nvs_handle_t handle,const char *key,const void *data,size_t count){
    (void)handle;(void)key;char path[4096];garden_path(path);FILE *f=fopen(path,"wb");
    if(!f)return ESP_FAIL;int ok=fwrite(data,1,count,f)==count;fclose(f);return ok?ESP_OK:ESP_FAIL;
}
int nvs_get_blob(nvs_handle_t handle,const char *key,void *data,size_t *count){
    (void)handle;(void)key;char path[4096];garden_path(path);FILE *f=fopen(path,"rb");
    if(!f)return ESP_ERR_NVS_NOT_FOUND;fseek(f,0,SEEK_END);size_t length=(size_t)ftell(f);rewind(f);
    if(!data){*count=length;fclose(f);return ESP_OK;}
    if(*count<length){fclose(f);return ESP_FAIL;}
    int ok=fread(data,1,length,f)==length;*count=length;fclose(f);return ok?ESP_OK:ESP_FAIL;
}
int nvs_erase_key(nvs_handle_t handle,const char *key){(void)handle;(void)key;char path[4096];garden_path(path);return unlink(path)?ESP_ERR_NVS_NOT_FOUND:ESP_OK;}
int nvs_commit(nvs_handle_t h){(void)h;return ESP_OK;}
void nvs_close(nvs_handle_t h){(void)h;}

void setBackLightVal(u8 n){backLightNum=n;}
void updateBackLight(u8 n){setBackLightVal(n);}
void backLightInit(void){}
void resetBackLightVal(void){backLightNum=8;}
void backLightScheduleTick(void){}
void imuRotationSetEnabled(bool enabled){(void)enabled;}
void soundStopAll(void){desktop_audio_stop();preview_slot=-1;}
void setSoundVal(u8 volume){desktop_audio_volume(volume/100.0f);}
void homeBgmSuspend(void){if(preview_slot<0)desktop_audio_stop();}
void homeBgmResume(void){
    if(dataSaveInfo.homeBgmSelection)desktop_audio_play(dataSaveInfo.homeBgmSelection-1,1,dataSaveInfo.volumeNum/100.0f);
}
bool soundPreviewStart(u8 slot){
    if(slot>=SOUND_TRACK_COUNT || !dataSaveInfo.volumeNum)return false;
    preview_slot=slot;desktop_audio_play(slot,1,dataSaveInfo.volumeNum/100.0f);return true;
}
void soundPreviewStop(void){preview_slot=-1;desktop_audio_stop();}
int soundPreviewSelection(void){return preview_slot;}

#if !defined(CONFIG_GVIEW_AI_PASSPORT)
void LCD_SetNativeOverlay(const uint8_t *ink,const volatile uint8_t *page,uint8_t id){extra_ink=ink;extra_page=page;extra_page_id=id;}
#endif
static uint32_t rgba(uint16_t color){return 0xFF000000u|(((color>>11)*255/31)<<16)|((((color>>5)&63)*255/63)<<8)|((color&31)*255/31);}
const uint32_t *desktop_pixels(void){
#if defined(CONFIG_GVIEW_AI_PASSPORT)
    if(full_frame_check)capture_strip(rendSX,rendSY,rendW,rendH,picCache);
    for(unsigned i=0;i<320*240;i++){
        uint16_t wire=passport_frame[i];pixels[i]=rgba((wire>>8)|(wire<<8));
    }
    return pixels;
#else
    const uint8_t *ink=LCD_NativeTextPixels();const uint16_t *palette=LCD_NativeTextPalette();
    const LCDViewport viewport=LCD_GetViewport();
    for(int y=0;y<240;y++)for(int x=0;x<240;x++){
        const int sx=viewport.left+x*viewport.width/240-rendSX;
        const int sy=viewport.top+y*viewport.height/240-rendSY;
        uint16_t color=BGColor;
        if(sx>=0 && sx<rendW && sy>=0 && sy<rendH){
            uint16_t wire=picCache[sy*rendW+sx];color=(wire>>8)|(wire<<8);
        }
        size_t i=y*240+x;
        if(extra_ink && extra_page && *extra_page==extra_page_id){
            unsigned value=(extra_ink[i/4]>>((i%4)*2))&3;
            if(value)color=value==1?BLACK:value==2?BLUE:GRAY;
        }
        if(ink && ink[i])color=palette[ink[i]];
        pixels[i]=rgba(color);
    }
    return pixels;
#endif
}
void wifiProcessSavedConnection(void){}
void wifi_Init(void){}
int esp_wifi_disconnect(void){WifiConnectingState=0;return ESP_OK;}
void wifi_scan(void){
    memset(ap_info,0,sizeof(ap_info));strcpy((char *)ap_info[0].ssid,"Desktop preview");
    ap_info[0].rssi=-45;ap_info[0].authmode=WIFI_AUTH_OPEN;
    wifiListNum=1;ScaningHd=0;refrushScanHd=1;
}
void resetWIFIConnectInfo(char *ssid,char *pass){ssidTxt=ssid;passWordSTxt=pass;}
void wifi_startConnect(void){WifiConnectingState=3;}
void wifi_connect_default(void){WifiConnectingState=3;}
void wifi_forgetSaved(char *ssid){(void)ssid;}
void selectWIFIConnectInfo(void){
    ssidTxt=wifiSaveInfo.ssid[wifiSaveInfo.saveNumAll<10?wifiSaveInfo.saveNumAll:0];
    passWordSTxt=wifiSaveInfo.password[wifiSaveInfo.saveNumAll<10?wifiSaveInfo.saveNumAll:0];
}
static void refresh_forecast(void){
    weatherCode=weather_options[weather_index].code;
    gb(weatherTxt,20,weather_options[weather_index].name);strcpy(tempNetTxt,"27");
    for(int i=0;i<3;i++){
        gb(textDayArr[i],10,weather_options[weather_index].name);gb(textNightArr[i],10,"多云");
        snprintf(tempHightLowArr[i],10,"%d/%d",19+i,27+i);strcpy(humidityNum[i],"65%");
        weatherDayCode[i]=weatherCode;weatherNightCode[i]=4;
    }
    ++forecast_version;
}
void weatherForecastInit(void){refresh_forecast();}
void weatherForecastRequest(uint8_t clear){(void)clear;refresh_forecast();}
uint32_t weatherForecastVersion(void){return forecast_version;}
uint8_t weatherForecastIsReady(void){return forecast_ready;}
void weatherForecastCopy(WeatherForecastSnapshot *s){
    memset(s,0,sizeof(*s));s->ready=forecast_ready;s->version=forecast_version;
    for(int i=0;i<3;i++){
        memcpy(s->condition[i],textDayArr[i],10);memcpy(s->temperature[i],tempHightLowArr[i],10);
        memcpy(s->humidity[i],humidityNum[i],5);s->icon[i]=weatherDayCode[i];s->weekday[i]=(weekday+i)%7;
    }
}
uint16_t get_month_info(int y,int m){
    struct tm first={.tm_year=y-1900,.tm_mon=m-1,.tm_mday=1,.tm_hour=12};mktime(&first);
    struct tm last=first;last.tm_mon=m;last.tm_mday=0;mktime(&last);
    return ((uint16_t)first.tm_wday<<8)|last.tm_mday;
}
void update_rtc_Txt(void){
    time_t now=desktop_now();tStruct=localtime(&now);year=tStruct->tm_year+1900;month=tStruct->tm_mon+1;
    day=tStruct->tm_mday;weekday=tStruct->tm_wday;hour=scene_hour;minute=tStruct->tm_min;second=tStruct->tm_sec;
    strftime(dateString,sizeof(dateString),"%m-%d %a",tStruct);
    strftime(timeString,sizeof(timeString),pageNow==PAGE_HOME?"%H %M":"%H:%M",tStruct);
}
void rtcInit(void){update_rtc_Txt();}
void refrushDayTime(void){checkDayTime();}
static void save_pending(void){
    if(pageNow==PAGE_SOUND_SET)saveSoundSettingsIfDirty();
    if(pageNow==PAGE_SYSTEM_SET)saveSystemSettingsIfDirty();
    if(pageNow==PAGE_CLOCKCOUNT_SET)saveTimerSoundSettingsIfDirty();
    if(pageNow==PAGE_CLOCK_SET || pageNow==PAGE_BGLIGHT_SCHEDULE)leaveClockSettings();
}
void desktop_open(int page){
    save_pending();backHomeTime=120;
    switch(page){
        case 0:treeShowTime=0;homeState=0;renderHomeInit();break;
        case 1:homeState=0;renderHomeInit();startSenlinBgMovie();treeShowTime=60000;break;
        case 2:renderShopInit();break;
        case 3:renderGardenInit();break;
        case 4:renderMenuInit();break;
        case 5:homeState=3;renderCalendarInit();break;
        case 6:homeState=1;weatherInit();break;
        case 7:clockSettingInit();break;
        case 8:setSystemInit();break;
    }
    update_rtc_Txt();update();
}
void desktop_key(int button,int held){
#if defined(CONFIG_GVIEW_AI_PASSPORT)
    passport_dispatch(button+(held?3:0));return;
#endif
    backHomeTime=120;
    if(aClockTimes){aClockTimes=0;soundStopAll();hideTips();return;}
    if(button==1 && held){renderPowerMenuInit();return;}
    int action=button==0?2:button==2?0:1;
    if(held)action=button==0?3:4;
    else if(softKeyHeaderVisible && button!=1)action=pageUIKeyAction(button);
    static const KeyName actions[]={KM1,KM2,KM3,KML,KMR};
    if(action>=0 && action<5)doKeyFun(actions[action]);
}
void desktop_touch(int x,int y,int end_x,int end_y){
    backHomeTime=120;
    if(pageNow==PAGE_WIFI_PW){inputPageGesture(x,y,end_x,end_y);return;}
    const int dx=end_x-x,dy=end_y-y;
    if(abs(dx)>35 && abs(dx)>abs(dy)){
        if(pageNow==PAGE_HOME || pageNow==PAGE_WEATHER || pageNow==PAGE_CALENDAR)
            doKeyFun(dx<0?KMU1:KMU3);
        else desktop_key(dx<0?2:0,0);
    }else if(y<WS_UI_HEADER_HEIGHT && softKeyHeaderVisible){
        int cell=x/WS_UI_KEY_WIDTH;
        if(x>=0 && cell<3)desktop_key(cell,0);
    }
}
void desktop_day(void){scene_day_index=(scene_day_index+1)%6;scene_hour=scene_hours[scene_day_index];update_rtc_Txt();desktop_open(0);}
void desktop_weather(void){weather_index=(weather_index+1)%7;refresh_forecast();if(pageNow==PAGE_HOME)refreshWeatherDisplay();else if(pageNow==PAGE_WEATHER)weatherInit();}
void desktop_credit(void){dataSaveInfo.money+=50000;writeData();if(pageNow==PAGE_SHOP)renderShopInit();}
void desktop_cycle_companion(int direction){
    if(!direction)return;
    const int count=getShopListArrNum(0),step=direction<0?-1:1;
    int id=itemSaveInfo.monsterIndex;
    if(id<1 || id>=count)id=step<0?1:0;
    for(int attempt=0;attempt<count;attempt++){
        id+=step;
        if(id<1)id=count-1;
        if(id>=count)id=1;
        if(!shopMonsterAvailable((u8)id))continue;
        save_pending();
        uint32_t balance=dataSaveInfo.money;
        shopApplyExtended(&itemSaveInfo.itemMonster,&itemSaveInfo.itemMonsterExtra,NULL,(u8)id,0,&balance);
        itemSaveInfo.monsterIndex=(u8)id;
        writeData();desktop_open(0);return;
    }
}
const char *desktop_companion_label(void){
    static char label[160];char name[128]={0};
    u8 id=itemSaveInfo.monsterIndex;
    if(!shopMonsterAvailable(id))id=0;
    unsigned total=0,position=0;
    for(u8 i=1;i<getShopListArrNum(0);i++)if(shopMonsterAvailable(i)){
        ++total;if(i==id)position=total;
    }
    gb2312_to_utf8((const uint8_t *)ShopListMonsterTxt[id],strlen(ShopListMonsterTxt[id]),
        (uint8_t *)name,sizeof(name)-1);
    if(id)snprintf(label,sizeof(label),"%s  %u/%u",name,position,total);
    else snprintf(label,sizeof(label),"%s",name);
    return label;
}
void desktop_init(const char *directory,const char *resources){
    snprintf(state_directory,sizeof(state_directory),"%s",directory);mkdir(directory,0700);
    snprintf(resource_directory,sizeof(resource_directory),"%s",resources);
    char path[4096];snprintf(path,sizeof(path),"%s/userdata.bin",directory);bool fresh=access(path,F_OK)!=0;
#if defined(CONFIG_GVIEW_AI_PASSPORT)
    full_frame_check=getenv("GVIEW_TEST_FULL_FRAME")!=NULL;
    if(!full_frame_check)gviewSetStripRenderer(16,capture_strip,passport_draw_overlay);
    viewInit(320,240,10,PASSPORT_OBJECT_POOL,50,40,25);
#else
    viewInit(320,240,10,1200,50,40,25);
#endif
gviewSetNativeTextHooks(LCD_NativeTextBegin,LCD_NativeTextDraw);
    romInit();soundCatalogInit();update_rtc_Txt();refresh_forecast();
    if(fresh){
        dataSaveInfo.money=100000;itemSaveInfo.legacyRiderIndex=0;
        itemSaveInfo.monsterIndex=26;itemSaveInfo.itemMonster|=UINT64_C(1)<<26;
        itemSaveInfo.itemHead=7;itemSaveInfo.itemShirt=7;itemSaveInfo.itemPants=7;
        itemSaveInfo.headIndex=2;itemSaveInfo.shirtIndex=1;itemSaveInfo.pantsIndex=1;writeData();
    }
    last_second=desktop_now();desktop_open(0);
}
void desktop_tick(void){
    time_t now=desktop_now();
    if(now!=last_second){
        last_second=now;update_rtc_Txt();checkAlarmClock();
        if(aClockTimes){
            if(!alarm_elapsed){int slot=alarmClockNow<5?(dataSaveInfo.alarmClock[alarmClockNow]&15)-1:0;
                if(slot>=0)desktop_audio_play(slot,0,dataSaveInfo.volumeNum/100.0f);}
            if(++alarm_elapsed>8){aClockTimes=0;alarm_elapsed=0;desktop_audio_stop();hideTips();}
        }
    }
    if(restarting){restarting=0;romInit();desktop_open(0);}
    keyState=0;update();
#if defined(CONFIG_GVIEW_AI_PASSPORT)
    if(full_frame_check)passport_draw_overlay();
    else{
        GVIEWRenderStats stats=gviewGetRenderStats();
        assert(stats.scenePasses==1 && stats.strips==15);
        assert(stats.commands<=PASSPORT_OBJECT_POOL);
        assert(stats.sceneLookups<=gviewUsedObjectCount()+10);
        assert(stats.numberFormats<=stats.commands);
        assert(stats.drawCalls<=(uint32_t)stats.commands*stats.strips);
    }
#else
    drawPageReturnHint();
#endif
    ++frame_counter;
}
const char *desktop_status(void){
    static const char *names[]={"白天","午后","傍晚","夜晚","凌晨","早晨"};
    snprintf(status,sizeof(status),"%s · %s · 金币 %u",names[scene_day_index],weather_options[weather_index].name,dataSaveInfo.money);
    return status;
}
void desktop_shutdown(void){save_pending();writeData();soundStopAll();}

static void desktop_check_main_view_keys(void){
    const int pages[]={PAGE_HOME,PAGE_CALENDAR,PAGE_WEATHER};
    const int openPages[]={0,5,6};
    desktop_open(0);
    for(unsigned i=0;i<6;i++){
        assert(pageNow==pages[i%3]);
        desktop_key(0,1);assert(pageNow==pages[i%3]);
        desktop_key(2,1);assert(pageNow==pages[i%3]);
        desktop_key(1,0);assert(pageNow==pages[(i+1)%3]);
    }
    for(unsigned i=0;i<3;i++){
        desktop_open(openPages[i]);
        desktop_key(0,0);assert(pageNow==PAGE_SHOP);
        desktop_key(0,1);assert(pageNow==pages[i]);
        desktop_key(2,0);assert(pageNow==PAGE_GARDEN);
        desktop_key(0,1);assert(pageNow==pages[i]);
        desktop_touch(200,110,100,110);assert(pageNow==pages[(i+1)%3]);
        desktop_touch(100,110,200,110);assert(pageNow==pages[i]);
        desktop_key(1,1);assert(pageNow==PAGE_POWER_MENU);
    }
    puts("PASS: middle clicks cycle all three main views; side holds stay put; shop/garden return, swipes and middle hold work");
}

static void desktop_wifi_capture(void (*capture)(const char *),const char *name){
    for(unsigned i=0;i<25;i++)desktop_tick();
    const LCDViewport viewport=LCD_GetViewport();
    assert(viewport.left==WS_UI_LEFT && viewport.width==WS_UI_WIDTH && viewport.height==240);
    if(pageNow==PAGE_WIFI_HOME || (pageNow==PAGE_WIFI_SAVED && wifiSaveInfo.saveNumAll)){
        DisplayObject *list=getChildAt(&stage.movieclipArr[2].disObj,0);
        DisplayObject *background=getChildAt(list,0);
        assert(background->width==WIFI_MENU_WIDTH+5);
    }
    if(pageNow==PAGE_WIFI_SCAN && !ScaningHd && wifiListNum){
        const MovieClip *layer=&stage.movieclipArr[0];
        const DisplayObject *selection=getChildAt((DisplayObject *)&layer->disObj,0);
        assert(selection->visible && selection->y+layer->y>=WS_WIFI_LIST_TOP);
        assert(selection->y+layer->y+selection->height<=WS_UI_FOOTER_TOP);
    }
    if(pageNow==PAGE_WIFI_PW){
        const unsigned layers[]={2,4,6};
        DisplayObject *panel=getChildAt(&stage.movieclipArr[0].disObj,0);
        assert(panel->width==WIFI_CONTENT_WIDTH);
        for(unsigned i=0;i<3;i++){
            DisplayObject *layer=&stage.movieclipArr[layers[i]].disObj;
            for(unsigned j=0;j<layer->childrenNum;j++){
                DisplayObject *key=getChildAt(layer,j);
                if(!key->visible)continue;
                assert(key->x>=WS_UI_LEFT && key->x+key->width<=WS_UI_RIGHT);
                assert(key->y>=KEYBOARD_TABS_TOP && key->y+key->height<=WS_UI_FOOTER_TOP);
            }
        }
#if defined(CONFIG_GVIEW_AI_PASSPORT)
        DisplayObject *password=getChildAt(&stage.movieclipArr[8].disObj,0);
        DisplayObject *count=getChildAt(&stage.movieclipArr[8].disObj,1);
        unsigned lines=1;
        for(const char *p=password->f;*p;p++)if(*p=='\n')++lines;
        assert(password->y+16+(lines-1)*20<=count->y);
#endif
    }
#if defined(CONFIG_GVIEW_AI_PASSPORT)
    assert(gviewUsedObjectCount()<PASSPORT_OBJECT_POOL);
#endif
    capture(name);
}

static unsigned desktop_wifi_confirm_count;
static void desktop_wifi_confirmed(void){++desktop_wifi_confirm_count;}

static void desktop_weather_capture(void (*capture)(const char *),const char *name){
    weatherInit();
    for(unsigned i=0;i<5;i++)desktop_tick();
    DisplayObject *layer=&stage.movieclipArr[0].disObj;
    assert(layer->childrenNum==2*PAGE_WEATHER_COLUMNS);
    for(unsigned i=0;i<PAGE_WEATHER_COLUMNS;i++){
        DisplayObject *card=getChildAt(layer,2*i);
        assert(card->x==WS_UI_LEFT+6+i*PAGE_WEATHER_CARD_PITCH);
        assert(card->width==PAGE_WEATHER_CARD_WIDTH);
        assert(card->x>=WS_UI_LEFT && card->x+card->width<=WS_UI_RIGHT);
        if(forecast_ready){
            DisplayObject *degree=getChildAt(&stage.movieclipArr[9].disObj,1+2*i);
            unsigned day=PAGE_WEATHER_FIRST_DAY+i;
            unsigned textWidth=strlen(tempHightLowArr[day])*8;
            assert(degree->x==card->x+(card->width-textWidth-16)/2+textWidth);
            assert(degree->x+degree->width<=card->x+card->width);
        }
    }
    capture(name);
}

static void desktop_check_weather_cards(void (*capture)(const char *)){
    const char *conditions[]={"晴","阵雨","小雪"};
    const uint8_t codes[]={0,13,23};
    for(unsigned i=0;i<3;i++){
        gb(textDayArr[i],sizeof(textDayArr[i]),conditions[i]);
        weatherDayCode[i]=codes[i];
    }
    forecast_ready=1;++forecast_version;
    desktop_weather_capture(capture,"weather-three-days");
    strcpy(textDayArr[1],"Rain+Snow");
    for(unsigned i=0;i<3;i++){
        strcpy(tempHightLowArr[i],"-100/-100");strcpy(humidityNum[i],"100%");
    }
    ++forecast_version;desktop_weather_capture(capture,"weather-edge-values");
    forecast_ready=0;++forecast_version;
    desktop_weather_capture(capture,"weather-waiting");
    forecast_ready=1;refresh_forecast();
    puts("PASS: target weather columns/day indices, distinct daily data, wide temperatures and waiting states");
}

static void desktop_layout_capture(void (*capture)(const char *),const char *name){
    for(unsigned i=0;i<25;i++)desktop_tick();
    const LCDViewport viewport=LCD_GetViewport();
    assert(viewport.left==WS_UI_LEFT && viewport.width==WS_UI_WIDTH && viewport.height==240);
    if(pageNow==PAGE_SOUND_SET || pageNow==PAGE_CLOCKCOUNT_SET || pageNow==PAGE_TCLCK_SET)
        assert(getChildAt(&stage.movieclipArr[0].disObj,0)->width==PAGE_FORM_WIDTH);
    if(pageNow==PAGE_SYSTEM_SET)
        assert(getChildAt(&stage.movieclipArr[0].disObj,0)->width==PAGE_CONTENT_WIDTH);
    if(pageNow==PAGE_CALENDAR)
        assert(getChildAt(&stage.movieclipArr[4].disObj,0)->width==PAGE_CONTENT_WIDTH);
    if(pageNow==PAGE_CITY){
        assert(stage.movieclipArr[2].Mask->x==PAGE_CITY_PROVINCE_LEFT);
        assert(stage.movieclipArr[3].Mask->x==PAGE_CITY_CITY_LEFT);
        assert(stage.movieclipArr[2].Mask->width==PAGE_CITY_COLUMN_WIDTH);
    }
#if defined(CONFIG_GVIEW_AI_PASSPORT)
    assert(gviewUsedObjectCount()<PASSPORT_OBJECT_POOL);
#endif
    capture(name);
}

static void desktop_check_page_layouts(void (*capture)(const char *)){
    extern void helpInit(void),renderFactory(void);
    DATASAVEINFO saved=dataSaveInfo;
    renderCalendarInit();desktop_layout_capture(capture,"layout-calendar");
    setSoundInit();desktop_layout_capture(capture,"layout-sound");
    desktop_key(2,0);desktop_key(1,0);assert(pageNow==PAGE_BGM_SET);
    desktop_layout_capture(capture,"layout-bgm");
    for(unsigned i=0;i<10;i++)desktop_key(2,0);
    desktop_layout_capture(capture,"layout-bgm-last");
    setSystemInit();desktop_layout_capture(capture,"layout-system");
    for(unsigned i=0;i<5;i++){
        dataSaveInfo.alarmClock[i]=((420+i*60)<<4)|10;
        dataSaveInfo.alarmClockWeek[i]=0x7f;
    }
    clockSettingInit();desktop_layout_capture(capture,"layout-alarm");
    desktop_key(1,0);desktop_layout_capture(capture,"layout-alarm-edit");
    setBGLightScheduleInit();desktop_layout_capture(capture,"layout-light");
    CitySelectInit();desktop_layout_capture(capture,"layout-city-province");
    desktop_key(1,0);desktop_layout_capture(capture,"layout-city-city");
    tomatoPageSet();desktop_layout_capture(capture,"layout-tomato-settings");
    clockCountPageInit();desktop_key(2,1);
    if(pageNow==PAGE_CLOCKCOUNT)desktop_key(2,1);
    assert(pageNow==PAGE_CLOCKCOUNT_SET);
    desktop_layout_capture(capture,"layout-timer-settings");
    helpInit();desktop_layout_capture(capture,"layout-help");
    renderFactory();desktop_layout_capture(capture,"layout-reset-confirm");
    renderShopInit();desktop_layout_capture(capture,"layout-shop");
    renderGardenInit();desktop_layout_capture(capture,"layout-garden");
    renderGardenShopInit();desktop_layout_capture(capture,"layout-garden-shop");
    extern DisplayObject *tipsTxt;
    showTips("Landscape notices use full width",0);
    assert(tipsTxt->childrenNum==(WS_UI_WIDTH==320 ? 1 : 2));
    desktop_layout_capture(capture,"layout-notice-wide");hideTips();
    dataSaveInfo=saved;
    puts("PASS: 18 page layout states use target viewports, panel widths and notice wrapping");
}

static void desktop_check_wifi_pages(void (*capture)(const char *)){
    extern void resetScanView(void);
    extern u8 wifiMenuListNum;
    WIFISAVEINFO saved=wifiSaveInfo;
    wifiHomePageInit();desktop_wifi_capture(capture,"wifi-home");
    int right=2*WS_UI_KEY_WIDTH+WS_UI_KEY_WIDTH/2,left=WS_UI_KEY_WIDTH/2;
    int middle=WS_UI_KEY_WIDTH+WS_UI_KEY_WIDTH/2;
    desktop_touch(right,10,right,10);assert(pageNow==PAGE_WIFI_HOME && wifiMenuListNum==1);
    desktop_touch(left,10,left,10);assert(pageNow==PAGE_WIFI_HOME && wifiMenuListNum==0);
    desktop_touch(middle,10,middle,10);assert(pageNow==PAGE_WIFI_SCAN);
    desktop_tick();hideTips();
    wifiListNum=20;ScaningHd=0;
    for(unsigned i=0;i<wifiListNum;i++)
        snprintf((char *)ap_info[i].ssid,sizeof(ap_info[i].ssid),"Preview-%02u-abcdefghijklmnopqrstuv",i);
    strcpy((char *)ap_info[0].ssid,"预览网络-客厅WiFi-01");
    resetScanView();
    // Publish through the same completion path used by a network scan.
    refrushScanHd=1;desktop_tick();
    desktop_wifi_capture(capture,"wifi-scan-top");
    for(unsigned i=0;i<19;i++)desktop_key(2,0);
    desktop_wifi_capture(capture,"wifi-scan-bottom");
    wifiListNum=0;refrushScanHd=1;desktop_tick();
    desktop_wifi_capture(capture,"wifi-scan-empty");
    ScaningHd=1;
    char scanning[40];gb(scanning,sizeof(scanning),"正在搜索中...");showTips(scanning,0);
    desktop_wifi_capture(capture,"wifi-scanning");ScaningHd=0;
    memset(&wifiSaveInfo,0,sizeof(wifiSaveInfo));
    wifiSavedInit();desktop_wifi_capture(capture,"wifi-saved-empty");
    wifiSaveInfo.saveNumAll=10;
    for(unsigned i=0;i<10;i++){
        snprintf(wifiSaveInfo.ssid[i],sizeof(wifiSaveInfo.ssid[i]),"Saved-%02u-abcdefghijklmnopqrstuvwx",i);
        memset(wifiSaveInfo.password[i],'a'+i,63);wifiSaveInfo.password[i][63]=0;
    }
    strcpy(wifiSaveInfo.ssid[0],"演示网络-Preview-01");
    wifiSavedInit();desktop_wifi_capture(capture,"wifi-saved-top");
    for(unsigned i=0;i<9;i++)desktop_key(2,0);
    assert(wifiMenuListNum==9);
    desktop_wifi_capture(capture,"wifi-saved-bottom");
    desktop_key(1,0);assert(pageNow==PAGE_WIFI_SAVED_ONE);
    desktop_wifi_capture(capture,"wifi-saved-detail");
    desktop_key(2,0);desktop_wifi_capture(capture,"wifi-saved-delete");
    inputTxtTemp[0]=0;inputPageInit(desktop_wifi_confirmed,0);
    for(unsigned i=0;i<4;i++){desktop_key(1,0);desktop_key(2,1);}
    assert(strcmp(inputTxtTemp,"aA1!")==0);
    desktop_key(0,0);desktop_key(1,0);assert(desktop_wifi_confirm_count==1);
    desktop_key(0,0);desktop_key(0,0);desktop_key(1,0);assert(strcmp(inputTxtTemp,"aA1")==0);
    desktop_key(2,0);desktop_key(1,0);assert(strcmp(inputTxtTemp,"aA1 ")==0);
    inputTxtTemp[0]=0;inputPageInit(wifiConnectPageInit,0);
    desktop_wifi_capture(capture,"wifi-keyboard-empty");
    memset(inputTxtTemp,'p',63);inputTxtTemp[63]=0;
    inputPageInit(wifiConnectPageInit,0);
    const char *names[]={"wifi-keyboard-lower","wifi-keyboard-upper","wifi-keyboard-digits","wifi-keyboard-symbols"};
    for(unsigned i=0;i<4;i++){
        assert(!pageUIKeyEnabled(1));
        desktop_wifi_capture(capture,names[i]);
        desktop_key(2,1);assert(pageNow==PAGE_WIFI_PW && strlen(inputTxtTemp)==63);
    }
    wifiConnectPageInit_save();WifiConnectingState=1;
    desktop_wifi_capture(capture,"wifi-connecting");
    WifiConnectingState=3;desktop_wifi_capture(capture,"wifi-connected");
    WifiConnectingState=4;desktop_wifi_capture(capture,"wifi-failed");
    wifiSaveInfo=saved;inputTxtTemp[0]=0;
    puts("PASS: Wi-Fi home, 20-network scan and empty states, 10 saved networks, long details, four keyboards and connection states");
}

#if defined(CONFIG_GVIEW_AI_PASSPORT)
static void desktop_board_key(unsigned position,unsigned rotation,bool held){
    PassportOrientedInput input={0};
    unsigned slot=(unsigned)passport_landscape_slot(position,rotation);
    assert(passport_oriented_event(&input,position,true,rotation,0)==PASSPORT_NO_ACTION);
    if(held){
        unsigned threshold=slot==1?PASSPORT_MIDDLE_HOLD_MS:PASSPORT_HOLD_MS;
        passport_dispatch(passport_oriented_poll(&input,rotation,threshold));
        assert(passport_oriented_event(&input,position,false,rotation,threshold+1)==PASSPORT_NO_ACTION);
    }else passport_dispatch(passport_oriented_event(&input,position,false,rotation,10));
}
static void desktop_check_landscape_keys(void){
    for(unsigned rotation=0;rotation<=2;rotation+=2){
        unsigned left=rotation==0?0:2,right=2-left;
        desktop_open(0);desktop_board_key(left,rotation,false);assert(pageNow==PAGE_SHOP);
        desktop_board_key(left,rotation,true);assert(pageNow==PAGE_HOME);
        desktop_board_key(right,rotation,false);assert(pageNow==PAGE_GARDEN);
        desktop_board_key(left,rotation,true);assert(pageNow==PAGE_HOME);
        const int pages[]={PAGE_HOME,PAGE_CALENDAR,PAGE_WEATHER};
        for(unsigned i=0;i<6;i++){
            assert(pageNow==pages[i%3]);
            desktop_board_key(left,rotation,true);assert(pageNow==pages[i%3]);
            desktop_board_key(right,rotation,true);assert(pageNow==pages[i%3]);
            desktop_board_key(1,rotation,false);assert(pageNow==pages[(i+1)%3]);
        }
        desktop_board_key(1,rotation,true);assert(pageNow==PAGE_POWER_MENU);
        desktop_board_key(1,rotation,false);assert(pageNow==PAGE_MENU);
    }
    puts("PASS: physical left/middle/right keys and middle-click view cycling in both landscape directions");
}
#endif

int desktop_smoke(void (*capture)(const char *)){
    assert(soundCatalogReady());
    for(int p=0;p<9;p++){
        desktop_open(p);for(int i=0;i<60;i++)desktop_tick();char name[40];snprintf(name,sizeof(name),"page-%d",p);capture(name);
    }
    /* Reload every retired selection and preserve all unrelated save fields. */
    const ITEMSAVEINFO beforeRetirement=itemSaveInfo;
    const DATASAVEINFO settingsBeforeRetirement=dataSaveInfo;
    const GARDENSAVEINFO gardenBeforeRetirement=gardenSaveInfo;
    const u8 retiredIds[]={12,14,17,20,21,22,23,45};
    uint64_t retiredMask=0;
    for(unsigned i=0;i<sizeof(retiredIds);++i)retiredMask|=UINT64_C(1)<<retiredIds[i];
    itemSaveInfo.itemMonster|=(UINT64_C(1)<<44)|(UINT64_C(1)<<46);
    itemSaveInfo.itemMonsterExtra|=3;
    const uint64_t retainedOwned=itemSaveInfo.itemMonster & ~retiredMask;
    const uint64_t retainedExtra=itemSaveInfo.itemMonsterExtra;
    for(unsigned i=0;i<sizeof(retiredIds);++i){
        itemSaveInfo.itemMonster|=retiredMask;
        itemSaveInfo.monsterIndex=retiredIds[i];
        writeData();readData();
        assert(itemSaveInfo.monsterIndex==0 && itemSaveInfo.itemMonster==retainedOwned);
        assert(itemSaveInfo.itemMonsterExtra==retainedExtra);
        assert(memcmp(&dataSaveInfo,&settingsBeforeRetirement,sizeof(dataSaveInfo))==0);
        assert(memcmp(&gardenSaveInfo,&gardenBeforeRetirement,sizeof(gardenSaveInfo))==0);
        assert(!shopMonsterAvailable(retiredIds[i]));
    }
    writeData();readData();
    assert(itemSaveInfo.monsterIndex==0 && itemSaveInfo.itemMonster==retainedOwned);
    assert(itemSaveInfo.itemMonsterExtra==retainedExtra);
    itemSaveInfo=beforeRetirement;writeData();
    puts("PASS: all retired selections/ownership migrate and reload; other records unchanged");
    /* Real shop callbacks, with the same 100-minimum prices and save layout. */
    assert(getShopListArrNum(0)==71);
    for(u8 id=1;id<getShopListArrNum(0);id++)if(shopMonsterAvailable(id))assert(ShopListMonsterCost[id]>=100);
    itemSaveInfo.itemMonster&=~(UINT64_C(1)<<24);itemSaveInfo.monsterIndex=26;
    dataSaveInfo.money=100000;desktop_open(2);
    for(int attempt=0;attempt<getShopListArrNum(0);attempt++){
        DisplayObject *layer=&stage.movieclipArr[5].disObj;
        DisplayObject *p=layer->childrenNum?getChildAt(layer,0):NULL;
        if(p && p->ac && p->ac->p==followerAron)break;
        desktop_key(2,0);
    }
    desktop_key(1,0);assert(dataSaveInfo.money==100000-ShopListMonsterCost[24] && itemSaveInfo.monsterIndex==26);
    desktop_key(1,0);assert(itemSaveInfo.monsterIndex==24);
    const uint64_t owned=itemSaveInfo.itemMonster;
    readData();assert(itemSaveInfo.itemMonster==owned && itemSaveInfo.monsterIndex==24);
    assert(getShopListArrNum(1)==2 && getShopListArrNum(2)==9);
    assert(getShopListArrNum(9)==0 && itemSaveInfo.legacyRiderIndex==0);
    for(u8 id=1;id<getShopListArrNum(0);id++){
        if(!shopMonsterAvailable(id))continue;
        itemSaveInfo.monsterIndex=id;desktop_open(0);
        for(int i=0;i<25;i++)desktop_tick();
    }
    /* Flight actions retain their cadence and repeat without stopping. */
    const struct {u8 id,ticks;const char *name;} flightChecks[]={
        {16,4,"Wingull"},{25,4,"Metang"},{19,3,"Tropius"}
    };
    for(unsigned species=0;species<sizeof(flightChecks)/sizeof(flightChecks[0]);species++){
        itemSaveInfo.monsterIndex=flightChecks[species].id;desktop_open(0);
        unsigned changes=0,lastChange=0,wraps=0;
        u8 lastFrame=phlPkm->ac->currentFrame;
        uint32_t visitedFrames=UINT32_C(1)<<lastFrame;
        for(unsigned tick=1;tick<=600;tick++){
            desktop_tick();
            assert(phlPkm->ac->frameMultiple==1 && phlPkm->ac->playing);
            const u8 frame=phlPkm->ac->currentFrame;
            if(frame!=lastFrame){
                assert(frame==(lastFrame+1)%phlPkm->ac->totallFrame);
                if(changes)assert(tick-lastChange==flightChecks[species].ticks);
                if(!frame)++wraps;
                visitedFrames|=UINT32_C(1)<<frame;
                lastChange=tick;lastFrame=frame;++changes;
            }
        }
        assert(changes>=600/flightChecks[species].ticks-1 && wraps>=2);
        printf("PASS: %s keeps a %u ms frame interval across 600 live home ticks\n",
            flightChecks[species].name,flightChecks[species].ticks*40);
        if(flightChecks[species].id==19){
            assert(phlPkm->ac->totallFrame==12 && visitedFrames==0xFFF);
            assert(getPicOffset(11,(u8 *)followerTropius)==73566);
            puts("PASS: Tropius repeats all 12 frames, wraps 11->0 and reads its last frame beyond 64K");
        }
    }
    const u8 quadrupedIds[]={11,13,24,31,34,37,40,42,54,57,61};
    for(u8 id=SHOP_FIRST_ADDED_FOLLOWER;id<getShopListArrNum(0);++id){
        if(!shopMonsterAvailable(id))continue;
        bool faster=false;
        for(unsigned i=0;i<sizeof(quadrupedIds);++i)if(id==quadrupedIds[i])faster=true;
        if(id<SHOP_FIRST_DOWNLOADED_FOLLOWER && !faster)continue;
        itemSaveInfo.monsterIndex=id;desktop_open(0);
        DisObjAction *action=phlPkm->ac;
        const u8 count=action->totallFrame;
        unsigned cycleTicks=0;
        for(u8 frame=0;frame<count;++frame)cycleTicks+=action->p[8+7*frame]+1;
        if(faster)assert(cycleTicks*2==count*5); // 1.2x: average 100 ms instead of 120 ms.
        unsigned wraps=0,changes=0,lastChange=0;
        u8 previous=action->currentFrame;
        assert(count>=3);
        for(unsigned tick=1;tick<=cycleTicks*3+1;++tick){
            desktop_tick();assert(action->playing && action->frameMultiple==1);
            const u8 frame=action->currentFrame;
            if(frame==previous)continue;
            assert(frame==(previous+1)%count);
            if(changes)assert(tick-lastChange==(unsigned)action->p[8+7*previous]+1);
            if(!frame)++wraps;
            previous=frame;lastChange=tick;++changes;
        }
        assert(wraps>=2);
    }
    puts("PASS: all 11 quadrupeds run at 1.2x; imported companions preserve per-frame timing and repeated loops");
    /* Quick cycling previews every companion without spending simulated coins. */
    const uint32_t balance=dataSaveInfo.money;
    const uint64_t previouslyOwned=itemSaveInfo.itemMonster;
    const u8 rider=itemSaveInfo.legacyRiderIndex;
    itemSaveInfo.monsterIndex=13;desktop_open(2);desktop_cycle_companion(1);
    assert(pageNow==PAGE_HOME && itemSaveInfo.monsterIndex==15);
    assert(strstr(desktop_companion_label(),"傲骨燕"));
    desktop_cycle_companion(-1);assert(itemSaveInfo.monsterIndex==13);
    const u8 lastCompanion=getShopListArrNum(0)-1;
    itemSaveInfo.monsterIndex=lastCompanion;desktop_cycle_companion(1);assert(itemSaveInfo.monsterIndex==1);
    desktop_cycle_companion(-1);assert(itemSaveInfo.monsterIndex==lastCompanion);
    itemSaveInfo.monsterIndex=0;desktop_cycle_companion(-1);assert(itemSaveInfo.monsterIndex==lastCompanion);
    itemSaveInfo.monsterIndex=0;desktop_cycle_companion(1);assert(itemSaveInfo.monsterIndex==1);
    uint8_t visited[SHOP_MONSTER_CAPACITY]={0};
    unsigned companionCount=0;
    for(u8 id=1;id<getShopListArrNum(0);id++)if(shopMonsterAvailable(id))++companionCount;
    const uint64_t previouslyOwnedExtra=itemSaveInfo.itemMonsterExtra;
    for(unsigned i=0;i<companionCount;i++){
        desktop_cycle_companion(1);
        assert(!visited[itemSaveInfo.monsterIndex]);visited[itemSaveInfo.monsterIndex]=1;
        assert(shopHasItem(itemSaveInfo.itemMonster,itemSaveInfo.itemMonsterExtra,itemSaveInfo.monsterIndex));
        assert(dataSaveInfo.money==balance && itemSaveInfo.legacyRiderIndex==rider);
    }
    for(u8 id=1;id<getShopListArrNum(0);id++)assert(visited[id]==shopMonsterAvailable(id));
    assert(companionCount==61 && itemSaveInfo.monsterIndex==1);
    assert((itemSaveInfo.itemMonster&previouslyOwned)==previouslyOwned);
    assert((itemSaveInfo.itemMonsterExtra&previouslyOwnedExtra)==previouslyOwnedExtra);
    const uint64_t allOwned=itemSaveInfo.itemMonster,allOwnedExtra=itemSaveInfo.itemMonsterExtra;
    readData();assert(itemSaveInfo.monsterIndex==1 && itemSaveInfo.itemMonster==allOwned && itemSaveInfo.itemMonsterExtra==allOwnedExtra);
    assert(dataSaveInfo.money==balance && itemSaveInfo.legacyRiderIndex==rider);
    capture("quick-companion");
    puts("PASS: quick companion cycling, both directions, wrap, retired skip, free unlock and saved selection");
    itemSaveInfo.monsterIndex=26;itemSaveInfo.legacyRiderIndex=0;desktop_open(0);
    for(int i=0;i<120;i++)desktop_tick();capture("home-final");
    desktop_check_main_view_keys();
    desktop_check_page_layouts(capture);
    desktop_check_weather_cards(capture);
    desktop_check_wifi_pages(capture);
#if defined(CONFIG_GVIEW_AI_PASSPORT)
    BGColor=WHITE;resetPageView(pixel_fixture_tick);
    addChild(0,newRoundRectDisObj(BLUE,296,58,12,70,6,1));
    desktop_tick();capture("round-rect-wide");
    assert(desktop_pixels()[100*320+306]==rgba(BLUE));
    assert(desktop_pixels()[70*320+12]==rgba(WHITE));
    // Passport uses the production device dispatcher for all checks above.
    desktop_open(0);desktop_key(1,1);assert(pageNow==PAGE_POWER_MENU);
    for(int i=0;i<5;i++)desktop_tick();capture("power-menu");
    desktop_open(2);
    for(unsigned category=0;category<9;category++){
        for(int tick=0;tick<5;tick++)desktop_tick();
        char name[32];snprintf(name,sizeof(name),"shop-category-%u",category);capture(name);
        desktop_key(2,1);assert(pageNow==PAGE_SHOP);
    }
    for(unsigned weather=0;weather<7;weather++){
        weather_index=weather;refresh_forecast();desktop_open(0);
        for(int tick=0;tick<350;tick++)desktop_tick();
        assert(gviewIsReady() && gviewUsedObjectCount()<PASSPORT_OBJECT_POOL);
        char name[32];snprintf(name,sizeof(name),"home-weather-%u",weather);capture(name);
    }
    gardenClearPlants();gardenSaveInfo.itemGarden=0x7ff;desktop_open(3);
    for(unsigned i=0;i<cloverMaxNum;i++)addCloverToFloor();
    for(unsigned i=0;i<30;i++)desktop_tick();
    assert(cloverNumNow==30 && gviewUsedObjectCount()<PASSPORT_OBJECT_POOL);capture("garden-full");
    desktop_key(2,0);assert(pageNow==PAGE_GARDEN);
    desktop_key(2,1);assert(pageNow==PAGE_GARDENSHOP);
    for(unsigned i=0;i<10;i++)desktop_tick();capture("garden-shop");
    tomatoPageInit();for(unsigned i=0;i<10;i++)desktop_tick();capture("tomato");
    clockCountPageInit();for(unsigned i=0;i<10;i++)desktop_tick();capture("countdown");
    setSoundInit();for(unsigned i=0;i<10;i++)desktop_tick();capture("sound");
    wifiHomePageInit();for(unsigned i=0;i<10;i++)desktop_tick();capture("wifi");
    assert(gviewUsedObjectCount()<PASSPORT_OBJECT_POOL);
    _Alignas(4) static const uint8_t pixelFixture[]={16,1,0,2,0,2,127,127,4,0,248,224,7,31,0,1,0};
    BGColor=WHITE;resetPageView(pixel_fixture_tick);
    DisplayObject *pixelSprite=getNewMvAction(pixelFixture,160,120);
    AcGotoAndStop(pixelSprite->ac,0);addChild(0,pixelSprite);
    desktop_tick();capture("pixel-format-16");
    assert(desktop_pixels()[119*320+159]==rgba(RED));
    pixelSprite->ac->scaleX=200;pixelSprite->ac->scaleY=200;
    desktop_tick();capture("pixel-format-16-scaled");
    assert(pixelSprite->p[0]==16 && pixelSprite->width==4 && pixelSprite->height==4);
    const uint32_t *fixturePixels=desktop_pixels();
    assert(fixturePixels[118*320+158]==rgba(RED) && fixturePixels[119*320+159]==rgba(RED));
    assert(fixturePixels[118*320+160]==rgba(GREEN) && fixturePixels[120*320+158]==rgba(BLUE));
    assert(fixturePixels[120*320+160]==rgba(WHITE));
    pixelSprite->ac->scaleX=pixelSprite->ac->scaleY=100;
    desktop_tick();rotaAction(pixelSprite->ac,90);desktop_tick();capture("pixel-format-16-rotated");
    assert(pixelSprite->p[0]==16);
    addChild(0,newPic4((uint8_t *)pixelFixture,4,4,120,80));
    desktop_tick();capture("pixel-format-16-corners");
    assert(desktop_pixels()[80*320+120]==rgba(RED));
    // Multiple numbers in separate strips must retain distinct formatted text.
    // Nested translations, a mask and movement exercise the per-frame draw list.
    BGColor=WHITE;resetPageView(pixel_fixture_tick);
    DisplayObject *root=newEmpty(20,15);addChild(2,root);
    DisplayObject *one=newNumber(16,1111,0,BLACK,40,20);addChildToObj(root,one);
    DisplayObject *two=newNumber(16,8888,0,BLUE,110,65);addChildToObj(root,two);
    DisplayObject *nested=newEmpty(40,115);addChildToObj(root,nested);
    addChildToObj(nested,newNumber(12,321,0,RED,0,0));
    addChildToObj(nested,newNumber(18,7,0,BLACK,100,40));
    Rect mask={35,20,260,200};stage.movieclipArr[2].Mask=&mask;
    desktop_tick();assert(gviewGetRenderStats().numberFormats==4);
    capture("render-nested-numbers");
    one->num=99;two->num=123456;root->x+=7;root->y+=3;
    desktop_tick();assert(gviewGetRenderStats().numberFormats==4);
    capture("render-nested-numbers-moved");
    stage.movieclipArr[2].Mask=NULL;
    desktop_check_landscape_keys();
    puts("PASS: one scene traversal and four numeric formats per frame, stable nested/masked draw order");
    printf("PASS: Passport 320x240, shared Waveshare keys, nine categories, weather and full garden within %u objects\n",PASSPORT_OBJECT_POOL);
#endif
    puts("PASS: current pages, 61 companions/forms, 100-minimum prices, real purchase/clothing callbacks and local save reload");return 0;
}
