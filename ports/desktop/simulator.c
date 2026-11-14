/* Host services for the current firmware. UI and asset sources are compiled
 * unchanged; ESP storage is represented by separate files on this computer. */
#include "simulator.h"
#include "compat.h"
#include "config.h"
#include "rom.h"
#include "sound.h"
#include "sound_catalog.h"
#include "NET/connect.h"
#include "NET/RTC_Internet.h"
#include "NET/weather_forecast.h"
#include "PAGE/HOME/home.h"
#include "PAGE/GARDEN/garden.h"
#include "PAGE/SHOP/shop.h"
#include "PAGE/INPUTTXT/inputtxt.h"
#include "GVIEW/utf8_gb2312_switch.h"
#include "LCD/lcd_init.h"
#include "LCD/viewport.h"
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

u8 pageNow=255,homeState,alarmClockNow;
u16 BGColor=0x9EBF,backHomeTime=120,VoltageVal=4100,sleepTimeNum;
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
u16 wifiListNum;
char *passWordSTxt,*ssidTxt;

static char state_directory[2048],resource_directory[2048],status[256];
static uint32_t pixels[240*240];
static const uint8_t *extra_ink;
static const volatile uint8_t *extra_page;
static uint8_t extra_page_id;
static int scene_hour=12,scene_day_index,weather_index,preview_slot=-1,restarting;
static uint32_t forecast_version=1,frame_counter;
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
extern void checkAlarmClock(void);

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
uint32_t esp_random(void){return arc4random();}
void vTaskDelay(unsigned ms){usleep(ms*1000);}
void DelayMs(u16 ms){vTaskDelay(ms);}
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

void LCD_SetNativeOverlay(const uint8_t *ink,const volatile uint8_t *page,uint8_t id){extra_ink=ink;extra_page=page;extra_page_id=id;}
static uint32_t rgba(uint16_t color){return 0xFF000000u|(((color>>11)*255/31)<<16)|((((color>>5)&63)*255/63)<<8)|((color&31)*255/31);}
const uint32_t *desktop_pixels(void){
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
uint8_t weatherForecastIsReady(void){return 1;}
void weatherForecastCopy(WeatherForecastSnapshot *s){
    memset(s,0,sizeof(*s));s->ready=1;s->version=forecast_version;
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
    time_t now=time(NULL);tStruct=localtime(&now);year=tStruct->tm_year+1900;month=tStruct->tm_mon+1;
    day=tStruct->tm_mday;weekday=tStruct->tm_wday;hour=scene_hour;minute=tStruct->tm_min;second=tStruct->tm_sec;
    strftime(dateString,sizeof(dateString),"%m-%d %a",tStruct);
    strftime(timeString,sizeof(timeString),pageNow==PAGE_HOME?"%H %M":"%H:%M",tStruct);
    if(pageNow==PAGE_CLOCK)updateTTime(tStruct->tm_hour*60+minute);
}
void rtcInit(void){update_rtc_Txt();}
void refrushDayTime(void){checkDayTime();}
void backToHomePage(void){
    if(homeState==1)weatherInit();else if(homeState==2)clockPageInit();
    else if(homeState==3)renderCalendarInit();else renderHomeInit();
}
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
        case 6:homeState=2;clockPageInit();break;
        case 7:homeState=1;weatherInit();break;
        case 8:clockSettingInit();break;
        case 9:setSystemInit();break;
    }
    update_rtc_Txt();update();
}
void desktop_key(int button,int held){
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
        if(pageNow==PAGE_HOME || pageNow==PAGE_CLOCK || pageNow==PAGE_WEATHER || pageNow==PAGE_CALENDAR)
            doKeyFun(dx<0?KMU1:KMU3);
        else desktop_key(dx<0?2:0,0);
    }else if(y<24 && softKeyHeaderVisible)desktop_key(x/80,0);
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
        itemSaveInfo.itemMonster|=UINT64_C(1)<<id;
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
    viewInit(320,240,10,1200,50,40,25);gviewSetNativeTextHooks(LCD_NativeTextBegin,LCD_NativeTextDraw);
    romInit();soundCatalogInit();update_rtc_Txt();refresh_forecast();
    if(fresh){
        dataSaveInfo.money=100000;itemSaveInfo.legacyRiderIndex=0;
        itemSaveInfo.monsterIndex=26;itemSaveInfo.itemMonster|=UINT64_C(1)<<26;
        itemSaveInfo.itemHead=7;itemSaveInfo.itemShirt=7;itemSaveInfo.itemPants=7;
        itemSaveInfo.headIndex=2;itemSaveInfo.shirtIndex=1;itemSaveInfo.pantsIndex=1;writeData();
    }
    last_second=time(NULL);desktop_open(0);
}
void desktop_tick(void){
    time_t now=time(NULL);
    if(now!=last_second){
        last_second=now;update_rtc_Txt();checkAlarmClock();
        if(aClockTimes){
            if(!alarm_elapsed){int slot=alarmClockNow<5?(dataSaveInfo.alarmClock[alarmClockNow]&15)-1:0;
                if(slot>=0)desktop_audio_play(slot,0,dataSaveInfo.volumeNum/100.0f);}
            if(++alarm_elapsed>8){aClockTimes=0;alarm_elapsed=0;desktop_audio_stop();hideTips();}
        }
    }
    if(restarting){restarting=0;romInit();desktop_open(0);}
    keyState=0;update();++frame_counter;
}
const char *desktop_status(void){
    static const char *names[]={"白天","午后","傍晚","夜晚","凌晨","早晨"};
    snprintf(status,sizeof(status),"%s · %s · 金币 %u",names[scene_day_index],weather_options[weather_index].name,dataSaveInfo.money);
    return status;
}
void desktop_shutdown(void){save_pending();writeData();soundStopAll();}

int desktop_smoke(void (*capture)(const char *)){
    assert(soundCatalogReady());
    for(int p=0;p<10;p++){
        desktop_open(p);for(int i=0;i<60;i++)desktop_tick();char name[40];snprintf(name,sizeof(name),"page-%d",p);capture(name);
    }
    /* Real shop callbacks, with the same 100-minimum prices and save layout. */
    assert(getShopListArrNum(0)==28);
    for(u8 id=1;id<28;id++)if(shopMonsterAvailable(id))assert(ShopListMonsterCost[id]>=100);
    itemSaveInfo.itemMonster&=~(UINT64_C(1)<<24);itemSaveInfo.monsterIndex=26;
    dataSaveInfo.money=100000;desktop_open(2);
    for(int attempt=0;attempt<28;attempt++){
        DisplayObject *layer=&stage.movieclipArr[5].disObj;
        DisplayObject *p=layer->childrenNum?getChildAt(layer,0):NULL;
        if(p && p->ac && p->ac->p==followerAron)break;
        desktop_key(2,0);
    }
    desktop_key(1,0);assert(dataSaveInfo.money==99700 && itemSaveInfo.monsterIndex==26);
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
    /* Quick cycling previews every companion without spending simulated coins. */
    const uint32_t balance=dataSaveInfo.money;
    const uint64_t previouslyOwned=itemSaveInfo.itemMonster;
    const u8 rider=itemSaveInfo.legacyRiderIndex;
    itemSaveInfo.monsterIndex=13;desktop_open(2);desktop_cycle_companion(1);
    assert(pageNow==PAGE_HOME && itemSaveInfo.monsterIndex==15);
    assert(strstr(desktop_companion_label(),"傲骨燕"));
    desktop_cycle_companion(-1);assert(itemSaveInfo.monsterIndex==13);
    itemSaveInfo.monsterIndex=27;desktop_cycle_companion(1);assert(itemSaveInfo.monsterIndex==1);
    desktop_cycle_companion(-1);assert(itemSaveInfo.monsterIndex==27);
    itemSaveInfo.monsterIndex=0;desktop_cycle_companion(-1);assert(itemSaveInfo.monsterIndex==27);
    itemSaveInfo.monsterIndex=0;desktop_cycle_companion(1);assert(itemSaveInfo.monsterIndex==1);
    uint64_t visited=0,expected=0;
    for(u8 id=1;id<getShopListArrNum(0);id++)if(shopMonsterAvailable(id))expected|=UINT64_C(1)<<id;
    for(unsigned i=0;i<26;i++){
        desktop_cycle_companion(1);
        visited|=UINT64_C(1)<<itemSaveInfo.monsterIndex;
        assert(dataSaveInfo.money==balance && itemSaveInfo.legacyRiderIndex==rider);
    }
    assert(visited==expected && itemSaveInfo.monsterIndex==1);
    assert((itemSaveInfo.itemMonster&previouslyOwned)==previouslyOwned);
    const uint64_t allOwned=itemSaveInfo.itemMonster;
    readData();assert(itemSaveInfo.monsterIndex==1 && itemSaveInfo.itemMonster==allOwned);
    assert(dataSaveInfo.money==balance && itemSaveInfo.legacyRiderIndex==rider);
    capture("quick-companion");
    puts("PASS: quick companion cycling, both directions, wrap, retired skip, free unlock and saved selection");
    itemSaveInfo.monsterIndex=26;itemSaveInfo.legacyRiderIndex=0;desktop_open(0);
    for(int i=0;i<120;i++)desktop_tick();capture("home-final");
    puts("PASS: current pages, 26 companions, 100-minimum prices, real purchase/clothing callbacks and local save reload");return 0;
}
