#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
// #include "nvs_flash.h"
// #include "protocol_examples_common.h"
#include "GVIEW/utf8_gb2312_switch.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "sdkconfig.h"
#include "weather_forecast.h"

//------------RTC-----------
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_cntl_struct.h"
#include "esp_system.h"

#include <cJSON.h>
#include "esp_http_client.h"
#include <time.h>
#include "../PAGE/HOME/home.h"
#include "GVIEW/gview.h"
#include "../rom.h"
#include "../PAGE/GARDEN/garden.h"
#include <mbedtls/md.h>
static const char *TAG = "RTCInfo";
#define MAX_HTTP_OUTPUT_BUFFER 2048  // http最大数据接收大小
u8 gettime = true;                // 是否从网络请求时间
volatile u8 getweather = false;             // 是否从网络请求天气
u8 resetTime = true;
char textDayArr[3][10] = {"未知","未知","未知"};
char textNightArr[3][10] = {"未知","未知","未知"};
char tempHightLowArr[3][10] = {"未知","未知","未知"};
char tempHightArr[3][5] = {"未知","未知","未知"};
char tempLowArr[3][5] = {"未知","未知","未知"};
u8 weatherNightCode[3];
u8 weatherDayCode[3];
char humidityNum[3][5] = {"未知","未知","未知"};

char weekdays[7][5] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};


int getLenNow = 0;//断包拼接长度
char getDataNow[MAX_HTTP_OUTPUT_BUFFER];//断包拼接数据缓存
//获取时间回调
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA)//收到数据事件
    {
        memccpy(getDataNow+getLenNow, evt->data, evt->data_len, evt->data_len);
        getLenNow += evt->data_len;
        if(getLenNow>=64){
            printf("Splice:%s\n", getDataNow);
            getLenNow = 0;
            cJSON *json = cJSON_Parse(getDataNow);//cJSON解析json格式数据
            if(!json){
                printf("JSON error\n");
            }else{
                printf("%.*s", sizeof(getDataNow), (char *)getDataNow);//{"api":"time","code":"1","currentTime": 1755959210277,"msg":""}1755978871059
                cJSON *timestamp = cJSON_GetObjectItem(json, "currentTime");
                if(timestamp){
                    if (timestamp->type == cJSON_Number)
                    {
                        //timestamp_to_beijing_time((long long)timestamp->valuedouble, 8);

                        struct timeval now = {.tv_sec = (timestamp->valuedouble)/1000+3600*8};//从网上获取到的时间戳要除1000
                        settimeofday(&now, NULL);//ESPIDF官方提供的设置时间函数
                        update_rtc_Txt();
                        gettime = false;
                        refrushDayTime();
                        checkDayTime();
                        if(pageNow == PAGE_HOME){
                            //更新场景背景
                            changeSceneColor();
                        }
                    }
                }
            }
            cJSON_Delete(json);
        }else{
            printf("Waiting for splice data...\n");
        }
    }
    return ESP_OK;
}


const char timeUrl[] = "http://f.m.suning.com/api/ct.do";
static void http_gettime(void *pvParameters)
{
    getLenNow = 0;
    memset(getDataNow, 0, MAX_HTTP_OUTPUT_BUFFER);
    //阻塞，直到获取到网络时间
    while (gettime)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
        esp_http_client_config_t config = {
            .method = HTTP_METHOD_GET,//get方式请求数据
            .url = timeUrl,
            .event_handler = _http_event_handler,
            .user_data = local_response_buffer,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d",
                     esp_http_client_get_status_code(client),
                     (int)esp_http_client_get_content_length(client));
            //gettime = false;
        }
        else
        {
            ESP_LOGI(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        }

        ESP_LOGI(TAG, "%s\n", local_response_buffer);
        esp_http_client_cleanup(client);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }


}

char timeString[10] = "00 00";
char dateString[30] = "7月08日 周五";
struct tm *tStruct = NULL;
// 提取日期时间信息
int year; // tm_year 从1900年开始
int month;     // tm_mon 范围是 0-11
int day;
int hour;        // 设置时区后，这里就是+8后的北京时间
int minute;
int second;
int weekday;
u8 timeFlashHd = 0;//时间闪烁标志

void refrushDayTime(void){
    if(hour == 5&&minute==0&&!second){
        //凌晨
        DayTime = 0;
        changeSceneColor();
    }else if(hour == 7&&minute==0&&!second){
        //早上
        DayTime = 1;
        changeSceneColor();
    }else if(hour == 10&&!minute&&!second){
        //中午
        DayTime = 2;
        changeSceneColor();
    }else if(hour == 16&&!minute&&!second){
        //午后
        DayTime = 3;
        changeSceneColor();
    }else if(hour == 17&&!minute&&!second){
        //傍晚
        DayTime = 4;
        changeSceneColor();
    }else if(hour == 18&&!minute&&!second){
        //夜晚
        DayTime = 5;
        changeSceneColor();
    }else if(hour == 19&&!minute&&!second){
        //深夜
        DayTime = 6;
        changeSceneColor();
    }
    
}

void update_rtc_Txt(void) {
    // 获取RTC时间
    time_t tt;
    time(&tt);

    tStruct=localtime(&tt);

    // 提取日期时间信息
    year   = tStruct->tm_year + 1900; // tm_year 从1900年开始
    month  = tStruct->tm_mon + 1;     // tm_mon 范围是 0-11
    day    = tStruct->tm_mday;
    hour   = tStruct->tm_hour;        // 设置时区后，这里就是+8后的北京时间
    minute = tStruct->tm_min;
    second = tStruct->tm_sec;

    if(year<2026&&resetTime){
        resetTime = false;
        struct timeval now = {.tv_sec = (234523452345)/1000};//从网上获取到的时间戳要除1000    晚上+3600*16
        settimeofday(&now, NULL);//ESPIDF官方提供的设置时间函数
    }
    
    // 获取星期几 (0=周日, 1=周一, ..., 6=周六)
    weekday = tStruct->tm_wday;
    
    // 打印结果
    if(pageNow == PAGE_HOME || pageNow == PAGE_CALENDAR){
        sprintf(timeString, "%02d %02d", tStruct->tm_hour, tStruct->tm_min);
    }else if(pageNow == PAGE_CLOCK){
        updateTTime(hour*60+minute);
    }

    if(pageNow == PAGE_CALENDAR){
        sprintf(dateString,"%02d月%02d日",  month, day);
    }else
    if(pageNow == PAGE_TCLCK || pageNow == PAGE_CLOCKCOUNT){
        sprintf(timeString, timeFlashHd?"%02d:%02d":"%02d %02d", tStruct->tm_hour, tStruct->tm_min);
        sprintf(dateString,"%02d/%02d %s",  month, day,  weekdays[weekday]);
        timeFlashHd = !timeFlashHd;
    }else
    if(pageNow == PAGE_WEATHER){
        sprintf(timeString, timeFlashHd?"%02d:%02d":"%02d %02d", tStruct->tm_hour, tStruct->tm_min);
        sprintf(dateString,"%02d/%02d",  month, day);
        timeFlashHd = !timeFlashHd;
    }else{
        sprintf(dateString,"%02d月%02d日 %s",  month, day,  weekdays[weekday]);
    }
    
    //if(second==0){
        //增加金币
        // u8 moneyPerMinute = 200;//每分钟增加的金币数
        // dataSaveInfo.money += moneyPerMinute;
    //}
    
    if((hour == 0&&minute==0&&!second)||(hour == 12&&minute==0&&!second)){
        printf("refrush time\n");
        gettime = true;
    }
    // Fetch once on each five-minute boundary, even if the one-second task
    // runs after second zero. Startup and Wi-Fi connection request immediately.
    static time_t lastWeatherRefreshSlot = (time_t)-1;
    if(tt > 1704067200){
        const time_t currentWeatherSlot = tt / 300;
        if(lastWeatherRefreshSlot == (time_t)-1 ||
           currentWeatherSlot < lastWeatherRefreshSlot){
            lastWeatherRefreshSlot = currentWeatherSlot;
        }else if(currentWeatherSlot > lastWeatherRefreshSlot){
            lastWeatherRefreshSlot = currentWeatherSlot;
            weatherForecastRequest(0);
        }
    }

    refrushDayTime();
    //printf("%4d年%02d月%02d日 %02d:%02d:%02d\n",t->tm_year+1900,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec);

    // Keep scene color on its ten-minute schedule.
    if(minute % 10 == 0 && second == 0) changeSceneColor();

    // Detect the five-minute slot transition even if the one-second task
    // runs after second zero. Initialize from the current slot at boot.
    static time_t lastGardenGrowthSlot = (time_t)-1;
    const time_t gardenGrowthSlot = tt / 300;
    if(lastGardenGrowthSlot == (time_t)-1 ||
       gardenGrowthSlot < lastGardenGrowthSlot){
        lastGardenGrowthSlot = gardenGrowthSlot;
    }else if(gardenGrowthSlot > lastGardenGrowthSlot){
        lastGardenGrowthSlot = gardenGrowthSlot;
        if(cloverNumNow < cloverMaxNum) addCloverToFloor();
    }
}


/**
 * @brief 获取指定年月的相关信息
 * @param year 年份，如 2026
 * @param month 月份，1-12
 * @return uint16_t 高8位: 1号星期几(0-6, 0=星期日), 低8位: 当月天数(28-31)
 *         若月份无效，返回 0xFFFF
 */
uint16_t get_month_info(int year, int month)
{
    // 校验月份
    if (month < 1 || month > 12) {
        return 0xFFFF;
    }
    
    struct tm timeinfo = { 0 };
    
    // 构造该月第一天的日期
    timeinfo.tm_year = year - 1900;   // tm_year 从 1900 开始计
    timeinfo.tm_mon  = month - 1;     // tm_mon 范围 0-11
    timeinfo.tm_mday = 1;             // 当月第一天
    timeinfo.tm_hour = 12;            // 设为中午避免夏令时边界问题
    timeinfo.tm_min  = 0;
    timeinfo.tm_sec  = 0;
    
    // 调用 mktime 填充 tm_wday（星期）字段
    if (mktime(&timeinfo) == -1) {
        return 0xFFFF;
    }
    
    // 获取当月天数
    int days_in_month;
    
    // 下个月的第0天就是本月的最后一天
    struct tm next_month = timeinfo;
    next_month.tm_mon++;              // 下个月
    next_month.tm_mday = 0;           // 设置为0, mktime会计算出上个月的最后一天
    
    if (mktime(&next_month) == -1) {
        return 0xFFFF;
    }
    
    days_in_month = next_month.tm_mday;
    
    // 组合返回值: 高8位存星期几，低8位存天数
    uint16_t result = ((uint16_t)(timeinfo.tm_wday & 0xFF) << 8) | (days_in_month & 0xFF);
    
    return result;
}

void time_task(void *pt){
    

    while (1)
    {
        if(IPAdressGet>0){//IPAdressGet>0表示板子已经联网，联网后开始请求数据
            //设置为true为重新同步时间
            //gettime = true;
            http_gettime(NULL);
            //vTaskDelay(5000 / portTICK_PERIOD_MS);
            // printf("IS geting time\n");
        }else{
            // printf("no Internet to gettime\n");
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);//延时一秒
    }
} 

void rtcInit(void)
{
    update_rtc_Txt();
    xTaskCreate(time_task,"time_task",1024*6,NULL,1,NULL);
    weatherForecastInit();
}

