

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "config.h"
#include "GVIEW/gview.h"
#include "rom.h"
#include "wifi_ssid.h"
#include "PAGE/SHOP/shop_model.h"
#include "./NET/connect.h"
#include "./PAGE/pageDraw.h"
#include "./PAGE/GARDEN/garden.h"
#include "nvs_flash.h"

#define romInitFlag  0x47
#define BRIGHTNESS_PRESET_VERSION 0xB2
#define PREVIOUS_BRIGHTNESS_PRESET_VERSION 0xB1
esp_partition_t* Data_res;
esp_partition_t* Wifi_res;

// WIFISAVEINFOTxt wifiSaveInfoTxt;
WIFISAVEINFO wifiSaveInfo = {0};
static u8 savedWifiSelection=0xFF;

// The native font uses two-byte GB2312 glyphs. Never truncate inside one.
void formatSoundTrackTitle(char *dest,size_t capacity,u8 slot,size_t maxUnits){
    if(!capacity) return;
    const char *title=soundTrackTitle(slot);
    const size_t total=strlen(title);
    const bool shortened=total>maxUnits;
    const size_t limit=shortened && maxUnits>=2 ? maxUnits-2 : maxUnits;
    size_t used=0;
    while(title[used]){
        const bool wide=(unsigned char)title[used]>=0x80;
        if(wide && !title[used+1]) break;
        const size_t glyph=wide ? 2 : 1;
        if(used+glyph>limit || used+glyph>=capacity) break;
        used+=glyph;
    }
    memcpy(dest,title,used);
    if(shortened && used+2<=maxUnits && used+3<=capacity){
        dest[used++]='.';
        dest[used++]='.';
    }
    dest[used]=0;
}

DATASAVEINFO dataSaveInfo = {
    .volumeNum = 90,
    .money = 5000,
    .brightnessPresetVersion = BRIGHTNESS_PRESET_VERSION,
    .homeBgmConfigVersion = HOME_BGM_CONFIG_VERSION,
    .homeBgmSelection = HOME_BGM_DEFAULT_SELECTION,
    .homeBgmPreferredSelection = HOME_BGM_DEFAULT_SELECTION,
    .autoRotationConfigVersion = AUTO_ROTATION_CONFIG_VERSION,
    .autoRotationEnabled = 1,
    .homePlantHintConfig = HOME_PLANT_HINT_ON,
};
// Daily levels for a 09:00-17:00 workday: wake, work, commute, evening, sleep.
#define BRIGHTNESS_SLOT(hour, level) ((((hour) * 60) << 4) | (level))
static const u16 defaultBrightnessSchedule[5] = {
    BRIGHTNESS_SLOT(7,4), BRIGHTNESS_SLOT(9,8),
    BRIGHTNESS_SLOT(17,6), BRIGHTNESS_SLOT(19,4),
    BRIGHTNESS_SLOT(23,2),
};
static const u16 previousBrightnessSchedule[5] = {
    BRIGHTNESS_SLOT(6,4), BRIGHTNESS_SLOT(9,8),
    BRIGHTNESS_SLOT(17,6), BRIGHTNESS_SLOT(20,4),
    BRIGHTNESS_SLOT(22,2),
};
static u8 brightnessPresetSaveNeeded;
static u8 autoRotationSaveNeeded;
static u8 homePlantHintSaveNeeded;
static void migrateAutoRotationSetting(void){
    if(dataSaveInfo.autoRotationConfigVersion != AUTO_ROTATION_CONFIG_VERSION ||
       dataSaveInfo.autoRotationEnabled > 1){
        dataSaveInfo.autoRotationConfigVersion = AUTO_ROTATION_CONFIG_VERSION;
        dataSaveInfo.autoRotationEnabled = 1;
        autoRotationSaveNeeded = 1;
    }
}

static void migrateHomePlantHintSetting(void){
    if(dataSaveInfo.homePlantHintConfig!=HOME_PLANT_HINT_ON &&
       dataSaveInfo.homePlantHintConfig!=HOME_PLANT_HINT_OFF){
        dataSaveInfo.homePlantHintConfig=HOME_PLANT_HINT_ON;
        homePlantHintSaveNeeded=1;
    }
}

static void loadDefaultBrightnessSchedule(void){
    memcpy(dataSaveInfo.brightnessInfo,defaultBrightnessSchedule,
           sizeof(defaultBrightnessSchedule));
}

static u8 hasLegacyBrightnessDefaults(void){
    const u16 oldDefault=BRIGHTNESS_SLOT(12,8);
    for(u8 i=0;i<5;i++)
        if(dataSaveInfo.brightnessInfo[i]!=oldDefault) return 0;
    return 1;
}

static u8 shouldMigrateBrightnessSchedule(void){
    if(hasLegacyBrightnessDefaults()) return 1;
    return dataSaveInfo.brightnessPresetVersion==PREVIOUS_BRIGHTNESS_PRESET_VERSION &&
           memcmp(dataSaveInfo.brightnessInfo,previousBrightnessSchedule,
                  sizeof(previousBrightnessSchedule))==0;
}

ITEMSAVEINFO itemSaveInfo = {
    .itemMonster = 3,
    .itemTent = 1,
    .itemBike = 1,
    .itemHead = 1,
    .itemShirt = 1,
    .itemPants = 1,
    .itemShoes = 1,
    .itemGloves = 1,
    .itemBag = 1,
    .monsterIndex = 0,
    .tentIndex = 0,
    .bikeIndex = 0,
    .headIndex = 0,
    .shirtIndex = 0,
    .pantsIndex = 0,
    .shoesIndex = 0,
    .glovesIndex = 0,
    .bagIndex = 0,
    .legacyRiderConfigVersion = LEGACY_RIDER_CONFIG_VERSION,
    .legacyRiderIndex = 0,
    
};
GARDENSAVEINFO  gardenSaveInfo = {
    .gardenLevel = 1,
    .maxPlantNum = cloverMaxNum,//最大同时生长植物
    .itemGarden = 1,
    .gardenIndex = 0,
};

static u8 legacyRiderSaveNeeded;
static u8 monsterSaveNeeded;
static void migrateLegacyRiderSelection(void){
    if(itemSaveInfo.legacyRiderConfigVersion!=LEGACY_RIDER_CONFIG_VERSION ||
       itemSaveInfo.legacyRiderIndex!=0){
        itemSaveInfo.legacyRiderConfigVersion=LEGACY_RIDER_CONFIG_VERSION;
        itemSaveInfo.legacyRiderIndex=0;
        legacyRiderSaveNeeded=1;
    }
}

void refrushMonsterList(void){
    // monsterBagNum = 0;
    // memset(monsterBag,0,monsterMaxNum);
    // for(u8 i=0;i<monsterMaxNum;i++){
    //     if(dataSaveInfo.monsterBag[i]!=0){
    //         monsterBag[monsterBagNum] = i;
    //         monsterBagNum ++;
    //     }
    // }
}

void initPartition(void){
    Data_res = esp_partition_find_first(0x40,0,NULL);
    //寻找字体分区
    if(Data_res==NULL)
    {
        printf("no partition is found\r\n");
        return;
    }
    else
    {
        printf("partiton Data_res:name:%s size:%"PRIu32"start address:%"PRIu32"\n",Data_res->label,Data_res->size,Data_res->address);//(int)res->size,(int)res->address);
    }

    Wifi_res = esp_partition_find_first(0x50,0,NULL);
    //寻找用户数据分区
    if(Wifi_res==NULL)
    {
        printf("no partition is found\r\n");
        return;
    }
    else
    {
        printf("partiton Wifi_res:name:%s size:%"PRIu32"start address:%"PRIu32"\n",Wifi_res->label,Wifi_res->size,Wifi_res->address);//(int)res->size,(int)res->address);
    }

}


void writeData(void){
#ifdef GVIEW_UI_AUDIT
    return; // The visual audit must never modify saved user data.
#endif
    //擦除分区 168K 6763个汉字 
    if(esp_partition_erase_range(Data_res,0,4096)!=ESP_OK)
    {
        printf("partition erase fail\r\n");
        return ;
    }
    //写数据
    if(esp_partition_write(Data_res,dataInfoAdress, &dataSaveInfo, sizeof(DATASAVEINFO))!=ESP_OK)
    {
        printf("dataInfo write fail\r\n");
        return ;
    }
    if(esp_partition_write(Data_res,itemInfoAdress, &itemSaveInfo, sizeof(ITEMSAVEINFO))!=ESP_OK)
    {
        printf("dataInfo write fail\r\n");
        return ;
    }
    if(esp_partition_write(Data_res,gardenInfoAdress, &gardenSaveInfo, sizeof(GARDENSAVEINFO))!=ESP_OK)
    {
        printf("dataInfo write fail\r\n");
        return ;
    }
}
u8 writeWifi(void){
#ifdef GVIEW_UI_AUDIT
    return 1; // The visual audit must never modify saved user data.
#endif
    //擦除分区 168K 6763个汉字 
    if(esp_partition_erase_range(Wifi_res,0,4096)!=ESP_OK)
    {
        printf("partition erase fail\r\n");
        return 0;
    }
    //写数据
    if(esp_partition_write(Wifi_res,wifiInfoAdress, &wifiSaveInfo, sizeof(WIFISAVEINFO))!=ESP_OK)
    {
        printf("wifiInfo write fail\r\n");
        return 0;
    }
    savedWifiSelection=wifiSaveInfo.selectWifiNow;
    return 1;
}

void updateWifiTxt(void){
    // wifiSaveInfoTxt.saveNumAll = wifiSaveInfo.saveNumAll;
    // //更新文本数据
    // for(u8 i=0;i<wifiSaveInfo.saveNumAll;i++){
    //     wifiSaveInfoTxt.ssid[i] = wifiSaveInfo.ssid[i];
    //     wifiSaveInfoTxt.password[i] = wifiSaveInfo.password[i];
    // }
}


void rememberWIFISelection(u8 index){
    if(index>=wifiSaveInfo.saveNumAll || index>=10) return;
    wifiSaveInfo.selectWifiNow=index;
    if(savedWifiSelection!=index) writeWifi();
}

u8 writeWIFIInfo(char* ssid,char* password){
    const size_t ssidLength=ssid ? strnlen(ssid,33) : 0;
    const size_t passwordLength=password ? strnlen(password,64) : 64;
    if(!ssidLength || ssidLength>32 || passwordLength>63 || wifiSaveInfo.saveNumAll>10) return 0;
    u8 index=0;
    while(index<wifiSaveInfo.saveNumAll && strcmp(wifiSaveInfo.ssid[index],ssid)!=0) ++index;
    if(index==wifiSaveInfo.saveNumAll){
        if(index>=10) return 0; // Never discard an existing network to save an eleventh.
        ++wifiSaveInfo.saveNumAll;
    }
    char ssidCopy[33]={0},passwordCopy[64]={0};
    memcpy(ssidCopy,ssid,ssidLength);
    memcpy(passwordCopy,password,passwordLength);
    memset(wifiSaveInfo.ssid[index],0,sizeof(wifiSaveInfo.ssid[index]));
    memset(wifiSaveInfo.password[index],0,sizeof(wifiSaveInfo.password[index]));
    memcpy(wifiSaveInfo.ssid[index],ssidCopy,ssidLength);
    memcpy(wifiSaveInfo.password[index],passwordCopy,passwordLength);
    wifiSaveInfo.selectWifiNow=index;
    updateWifiTxt();
    return writeWifi();
}

void deleteSavedWIFI(u8 num){
    if(num>=wifiSaveInfo.saveNumAll)return;
    //删除指定WIFI
    for(u8 i=num;i<wifiSaveInfo.saveNumAll-1;i++){
        strcpy(wifiSaveInfo.ssid[i],wifiSaveInfo.ssid[i+1]);
        strcpy(wifiSaveInfo.password[i],wifiSaveInfo.password[i+1]);
    }
    wifiSaveInfo.saveNumAll -- ;
    if(wifiSaveInfo.selectWifiNow>num) --wifiSaveInfo.selectWifiNow;
    else if(wifiSaveInfo.selectWifiNow==num) wifiSaveInfo.selectWifiNow=0;
    if(wifiSaveInfo.selectWifiNow>=wifiSaveInfo.saveNumAll) wifiSaveInfo.selectWifiNow=0;
    updateWifiTxt();
    writeWifi();
}

void deleteAlarmClock(u8 num){
    if(num>=5)return;
    //删除指定WIFI
    for(u8 i=num;i<5-1;i++){
        dataSaveInfo.alarmClock[i] = dataSaveInfo.alarmClock[i+1];
    }
    dataSaveInfo.alarmClock[4] = 0;
    writeData();
}

void resetAllData(void){
    gardenClearPlants();
    //初始化所有参数
    wifiSaveInfo.selectWifiNow = 0;
    for(u8 i=0;i<10;i++){
        memset(wifiSaveInfo.ssid[i],0,sizeof(wifiSaveInfo.ssid[i]));
        memset(wifiSaveInfo.password[i],0,sizeof(wifiSaveInfo.password[i]));
    }
    if(dataSaveInfo.isInit == romInitFlag){
        wifiSaveInfo.saveNumAll = 0;
    }else{
        wifiSaveInfo.saveNumAll = 0;

        memset(&dataSaveInfo,0,sizeof(dataSaveInfo));
        memset(&itemSaveInfo,0,sizeof(itemSaveInfo));
        memset(&gardenSaveInfo,0,sizeof(gardenSaveInfo));
    }

    dataSaveInfo.tomatoTime = 25*60;//专注时间
    dataSaveInfo.tomatoTimeRelax = 5*60;//休息时间
    dataSaveInfo.tomatoTimes = 0;//重复次数
    dataSaveInfo.tomatoRestRing = 0;
    dataSaveInfo.tomatoWorkRing = 0;
    dataSaveInfo.provice = 0;
    dataSaveInfo.city = 0;
    dataSaveInfo.volumeNum = 80;

    dataSaveInfo.money = 1000;

    itemSaveInfo.itemMonster = 1;
    itemSaveInfo.itemTent = 1;
    itemSaveInfo.itemBike = 1;
    itemSaveInfo.itemHead = 1;
    itemSaveInfo.itemShirt = 1;
    itemSaveInfo.itemPants = 1;
    itemSaveInfo.itemShoes = 1;
    itemSaveInfo.itemGloves = 1;
    itemSaveInfo.itemBag = 1;
    itemSaveInfo.monsterIndex = 0;
    itemSaveInfo.tentIndex = 0;
    itemSaveInfo.bikeIndex = 0;
    itemSaveInfo.headIndex = 0;
    itemSaveInfo.shirtIndex = 0;
    itemSaveInfo.pantsIndex = 0;
    itemSaveInfo.shoesIndex = 0;
    itemSaveInfo.glovesIndex = 0;
    itemSaveInfo.bagIndex = 0;
    itemSaveInfo.legacyRiderConfigVersion = LEGACY_RIDER_CONFIG_VERSION;
    itemSaveInfo.legacyRiderIndex = 0;

    
    gardenSaveInfo.itemGarden = 1;
    gardenSaveInfo.gardenIndex = 0;
    gardenSaveInfo.maxPlantNum = cloverMaxNum;

    dataSaveInfo.tempOffset = 0;
    dataSaveInfo.clockCountRing = 0;
    dataSaveInfo.clockCountTimes = 5;
    dataSaveInfo.brightnessPresetVersion = BRIGHTNESS_PRESET_VERSION;
    memset(dataSaveInfo.reservedIdleBrightness,0,
           sizeof(dataSaveInfo.reservedIdleBrightness));
    dataSaveInfo.homeBgmConfigVersion = HOME_BGM_CONFIG_VERSION;
    dataSaveInfo.homeBgmSelection = HOME_BGM_DEFAULT_SELECTION;
    dataSaveInfo.homeBgmPreferredSelection = HOME_BGM_DEFAULT_SELECTION;
    dataSaveInfo.autoRotationConfigVersion = AUTO_ROTATION_CONFIG_VERSION;
    dataSaveInfo.autoRotationEnabled = 1;
    dataSaveInfo.homePlantHintConfig = HOME_PLANT_HINT_ON;
    
    for(u8 i=0;i<5;i++){
        dataSaveInfo.alarmClock[i] = 0;
        dataSaveInfo.alarmClockWeek[i] = 0;
    }
    loadDefaultBrightnessSchedule();
    
    
    
    dataSaveInfo.isInit = romInitFlag;
}

void readData(void){ 
    //读用户数据
    if(esp_partition_read(Data_res,dataInfoAdress, &dataSaveInfo, sizeof(DATASAVEINFO))==ESP_OK)
    {
        if(dataSaveInfo.isInit != romInitFlag){
            resetAllData();
            writeData();
            writeWifi();
        }else{
            //printf("read weather:%d,%d\r\n",dataSaveInfo.provice,dataSaveInfo.city);
            if(dataSaveInfo.provice>32)dataSaveInfo.provice = 0;
            if(dataSaveInfo.city>23)dataSaveInfo.city = 0;
        }
        for(u8 i=0;i<5;i++)if((dataSaveInfo.brightnessInfo[i]&0x0F) > 10)dataSaveInfo.brightnessInfo[i] = (dataSaveInfo.brightnessInfo[i]&0xFFF0)|10;
        if(dataSaveInfo.volumeNum > 100)dataSaveInfo.volumeNum = 100;
        if(dataSaveInfo.clockCountRing >= SOUND_TRACK_COUNT)dataSaveInfo.clockCountRing = 0;
        if(dataSaveInfo.clockCountTimes > 99)dataSaveInfo.clockCountTimes = 1;
        if(dataSaveInfo.tomatoRestRing >= SOUND_TRACK_COUNT)dataSaveInfo.tomatoRestRing = 0;
        if(dataSaveInfo.tomatoWorkRing >= SOUND_TRACK_COUNT)dataSaveInfo.tomatoWorkRing = 0;
        if(dataSaveInfo.brightnessPresetVersion != BRIGHTNESS_PRESET_VERSION){
            // Replace known old presets; preserve any edited schedule.
            if(shouldMigrateBrightnessSchedule()) loadDefaultBrightnessSchedule();
            dataSaveInfo.brightnessPresetVersion = BRIGHTNESS_PRESET_VERSION;
            brightnessPresetSaveNeeded = 1;
        }
        if(dataSaveInfo.homeBgmConfigVersion != HOME_BGM_CONFIG_VERSION){
            const u8 wasSilent=dataSaveInfo.homeBgmSelection==HOME_BGM_SILENT;
            dataSaveInfo.homeBgmConfigVersion = HOME_BGM_CONFIG_VERSION;
            dataSaveInfo.homeBgmSelection = wasSilent ? HOME_BGM_SILENT : HOME_BGM_DEFAULT_SELECTION;
            dataSaveInfo.homeBgmPreferredSelection = HOME_BGM_DEFAULT_SELECTION;
        }
        if(dataSaveInfo.homeBgmSelection > HOME_BGM_TRACK_COUNT)
            dataSaveInfo.homeBgmSelection = HOME_BGM_SILENT;
        if(dataSaveInfo.homeBgmPreferredSelection<1 ||
           dataSaveInfo.homeBgmPreferredSelection>HOME_BGM_TRACK_COUNT)
            dataSaveInfo.homeBgmPreferredSelection = HOME_BGM_DEFAULT_SELECTION;
        migrateAutoRotationSetting();
        migrateHomePlantHintSetting();
    }
    //读item数据
    if(esp_partition_read(Data_res,itemInfoAdress, &itemSaveInfo, sizeof(ITEMSAVEINFO))==ESP_OK)
    {
        //printf("itemInfo read complete\r\n");
    }
    else
    {
        printf("item partition read fail\r\n");
    }

    migrateLegacyRiderSelection();
    monsterSaveNeeded=shopRemoveRetiredMonster(&itemSaveInfo.itemMonster,&itemSaveInfo.monsterIndex);

    if(esp_partition_read(Data_res,gardenInfoAdress, &gardenSaveInfo, sizeof(GARDENSAVEINFO))!=ESP_OK)
    {
        printf("garden read fail\r\n");
        return ;
    }
    gardenSaveInfo.maxPlantNum = cloverMaxNum;


    //读SSID数据
    if(esp_partition_read(Wifi_res,wifiInfoAdress, &wifiSaveInfo, sizeof(WIFISAVEINFO))==ESP_OK)
    {
        savedWifiSelection=wifiSaveInfo.selectWifiNow;
        // Older firmware stored Chinese SSIDs as GB2312 screen text.
        // Restore the original UTF-8 bytes for both display and reconnect.
        bool upgraded=false;
        if(wifiSaveInfo.saveNumAll<=10){
            for(u8 i=0;i<wifiSaveInfo.saveNumAll;++i)
                upgraded|=wifiSsidUpgradeLegacy(wifiSaveInfo.ssid[i]);
        }
        updateWifiTxt();
        if(upgraded) writeWifi();
    }
    else
    {
        printf("partition read fail\r\n");
    }
    
}


void delayed_restart(void) {
    
    showTips("已恢复出厂,重启中...",0);
    rendWin(0,0,winW,winH,BGColor);
    resetAllData();
    writeData();
    writeWifi();
    DelayFunction(esp_restart,1);
    // vTaskDelay(1000 / portTICK_PERIOD_MS); // 等待1秒
    // esp_restart();
    // vTaskDelete(NULL);
}

void romInit(void){
    // NVS is also used by the garden snapshot; initialize it before saved data.
    esp_err_t ret=nvs_flash_init();
    if(ret==ESP_ERR_NVS_NO_FREE_PAGES || ret==ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret=nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    initPartition();
    readData();
    gardenRestorePlants();
    if(brightnessPresetSaveNeeded || autoRotationSaveNeeded || legacyRiderSaveNeeded || monsterSaveNeeded || homePlantHintSaveNeeded) writeData();
}