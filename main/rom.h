#ifndef __ROM_H__
#define __ROM_H__

#include <stddef.h>
#include "sound.h"
#include "sound_catalog.h"
#include "config.h"

//数据保存地址
#define wifiInfoAdress 0
#define dataInfoAdress 0
#define itemInfoAdress 1024
#define gardenInfoAdress 2048
#define HOME_BGM_CONFIG_VERSION 0xB8
#define HOME_BGM_TRACK_COUNT SOUND_TRACK_COUNT
#define HOME_BGM_DEFAULT_SELECTION (SOUND_DEFAULT_HOME_TRACK + 1)
#define HOME_BGM_SILENT 0

typedef struct DATASAVEINFO{
    uint8_t isInit;//是否已保存过数据
    uint8_t provice;//省份编号
    uint8_t city;//城市编号
    uint32_t tomatoTime;//番茄时钟专注时间
    uint32_t tomatoTimeRelax;//番茄时钟休息时间
    u8 tomatoTimes;//番茄时钟重复次数
    u8 tomatoRestRing;//番茄闹钟休息铃声
    u8 tomatoWorkRing;//番茄闹钟专注铃声
    u16 brightnessInfo[5];//高5位 时 中5位 分 低6位 背光亮度
    u8 volumeNum;//音量
    u16 alarmClock[5];//闹钟设置时间,第1-4位为对应铃声编号,,第5-15位代表分种
    u8 alarmClockWeek[5];//闹钟对应星期开启
    u32 money;//金钱
    // u8 eggProcess;//孵蛋进度
    signed char tempOffset;//温度补偿
    u8 clockCountRing;//计时钟铃声
    u8 clockCountTimes;//计时钟铃声响铃次数
    u8 brightnessPresetVersion;//定时亮度预置版本，沿用旧闲置设置字节
    u8 reservedIdleBrightness[2];//保留旧数据布局，避免改变后续字段偏移
    u8 homeBgmConfigVersion;//首页背景音乐设置版本
    u8 homeBgmSelection;//0 静音, 1-10 为 MP3 曲目
    u8 homeBgmPreferredSelection;//快捷开关关闭时记住的曲目
}DATASAVEINFO;

typedef struct ITEMSAVEINFO{
    uint64_t itemMonster;//已获得的怪物列表,二进制表示是否拥有
    uint64_t itemTent;//已获得的帐篷
    uint64_t itemBike;//已获得的帐篷
    uint64_t itemHead;//已获得的头饰
    uint64_t itemShirt;//已获得的上衣
    uint64_t itemPants;//已获得的裤子
    uint64_t itemShoes;//已获得的鞋子
    uint64_t itemGloves;//已获得的手套
    uint64_t itemBag;//已获得的背包
    
    u8 monsterIndex;//当前选择的怪物索引
    u8 tentIndex;
    u8 bikeIndex;
    u8 headIndex;
    u8 shirtIndex;
    u8 pantsIndex;
    u8 shoesIndex;
    u8 glovesIndex;
    u8 bagIndex;
    
}ITEMSAVEINFO;

typedef struct GARDENSAVEINFO{
    u8 gardenLevel;//花园等级
    u8 maxPlantNum;//最大同时生长植物
    uint64_t itemGarden;//已获得的花园植物
    u8 gardenIndex;
}GARDENSAVEINFO;

typedef struct WIFISAVEINFO{
    uint8_t saveNumAll;//已保存的wifi数量
    uint8_t selectWifiNow;//当前连接的WIFI
    char ssid[10][33];//最多保存10个wifi
    char password[10][64];
}WIFISAVEINFO;

typedef struct WIFISAVEINFOTxt{
    uint8_t saveNumAll;//已保存的wifi数量
    char* ssid[10];//最多保存10个wifi
    char* password[10];
}WIFISAVEINFOTxt;

extern WIFISAVEINFOTxt wifiSaveInfoTxt;
extern WIFISAVEINFO wifiSaveInfo;
extern DATASAVEINFO dataSaveInfo;
extern ITEMSAVEINFO itemSaveInfo;
extern GARDENSAVEINFO gardenSaveInfo;
void romInit(void);
void readData(void);
void initPartition(void);
void writeWIFIInfo(char* ssid, char* password);
void writeSetData(void);
void deleteSavedWIFI(u8 num);
void deleteAlarmClock(u8 num);
void delayed_restart(void);
void writeData(void);
void formatSoundTrackTitle(char *dest,size_t capacity,u8 slot,size_t maxUnits);
void refrushMonsterList(void);
#endif
