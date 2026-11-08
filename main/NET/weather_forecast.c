#include "weather_forecast.h"
#include "RTC_Internet.h"
#include "connect.h"
#include "../PAGE/HOME/home.h"
#include "../rom.h"
#include "GVIEW/utf8_gb2312_switch.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WEATHER_HTTP_CAPACITY 16384
static const char *TAG = "WeatherForecast";
static portMUX_TYPE forecastMux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t forecastReady;
static uint8_t forecastWeekday[3];
static volatile uint32_t forecastVersion;
static uint32_t requestSerial;
static TaskHandle_t weatherTaskHandle;

typedef struct {
    char body[WEATHER_HTTP_CAPACITY];
    size_t length;
    uint8_t overflow;
} WeatherResponse;

typedef struct {
    char currentCondition[10];
    char currentTemperature[10];
    uint8_t currentIcon;
    char condition[3][10];
    char temperature[3][10];
    char humidity[3][5];
    uint8_t icon[3];
    uint8_t weekday[3];
} WeatherResult;

uint32_t weatherForecastVersion(void){ return forecastVersion; }
uint8_t weatherForecastIsReady(void){
    portENTER_CRITICAL(&forecastMux);
    uint8_t ready=forecastReady;
    portEXIT_CRITICAL(&forecastMux);
    return ready;
}

void weatherForecastCopy(WeatherForecastSnapshot *snapshot){
    if(!snapshot) return;
    portENTER_CRITICAL(&forecastMux);
    memcpy(snapshot->condition,textDayArr,sizeof(snapshot->condition));
    memcpy(snapshot->temperature,tempHightLowArr,sizeof(snapshot->temperature));
    memcpy(snapshot->humidity,humidityNum,sizeof(snapshot->humidity));
    memcpy(snapshot->icon,weatherDayCode,sizeof(snapshot->icon));
    memcpy(snapshot->weekday,forecastWeekday,sizeof(snapshot->weekday));
    snapshot->ready=forecastReady;
    snapshot->version=forecastVersion;
    portEXIT_CRITICAL(&forecastMux);
}

void weatherForecastRequest(uint8_t clear_previous_city){
    portENTER_CRITICAL(&forecastMux);
    ++requestSerial;
    if(clear_previous_city){
        forecastReady=0;
        weatherCode=39;
        strcpy(weatherTxt,"--");
        strcpy(tempNetTxt,"--");
        ++forecastVersion;
    }
    getweather=true;
    portEXIT_CRITICAL(&forecastMux);
    if(weatherTaskHandle) xTaskNotifyGive(weatherTaskHandle);
}

static esp_err_t weatherHttpEvent(esp_http_client_event_t *event){
    if(event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
    WeatherResponse *response=(WeatherResponse *)event->user_data;
    if(!response || event->data_len < 0) return ESP_FAIL;
    if(response->length+(size_t)event->data_len >= sizeof(response->body)){
        response->overflow=1;
        return ESP_FAIL;
    }
    memcpy(response->body+response->length,event->data,event->data_len);
    response->length+=(size_t)event->data_len;
    response->body[response->length]=0;
    return ESP_OK;
}

static cJSON *weatherGetJson(const char *url,WeatherResponse *response){
    response->length=0;
    response->overflow=0;
    response->body[0]=0;
    esp_http_client_config_t config={
        .url=url,
        .method=HTTP_METHOD_GET,
        .event_handler=weatherHttpEvent,
        .user_data=response,
        .crt_bundle_attach=esp_crt_bundle_attach,
        .timeout_ms=10000,
    };
    esp_http_client_handle_t client=esp_http_client_init(&config);
    if(!client) return NULL;
    esp_err_t error=esp_http_client_perform(client);
    int status=esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if(error!=ESP_OK || status!=200 || response->overflow){
        ESP_LOGW(TAG,"HTTP error %s, status %d",esp_err_to_name(error),status);
        return NULL;
    }
    cJSON *json=cJSON_Parse(response->body);
    if(!json) ESP_LOGW(TAG,"Invalid weather JSON");
    return json;
}

static size_t weatherUrlEncode(const char *source,char *target,size_t capacity){
    static const char hex[]="0123456789ABCDEF";
    size_t used=0;
    for(const unsigned char *p=(const unsigned char *)source;*p;++p){
        if(isalnum(*p) && *p<128){
            if(used+1>=capacity) return 0;
            target[used++]=*p;
        }else{
            if(used+3>=capacity) return 0;
            target[used++]='%';
            target[used++]=hex[*p>>4];
            target[used++]=hex[*p&15];
        }
    }
    target[used]=0;
    return used;
}

static int weatherFindCity(const char *city,const char *province,
                           const char *country,WeatherResponse *response,
                           double *latitude,double *longitude){
    char encoded[192],url[320];
    if(!weatherUrlEncode(city,encoded,sizeof(encoded))) return 0;
    snprintf(url,sizeof(url),
        "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=20&language=zh&countryCode=%s",
        encoded,country);
    cJSON *json=weatherGetJson(url,response);
    if(!json) return 0;
    cJSON *results=cJSON_GetObjectItemCaseSensitive(json,"results");
    cJSON *best=NULL;
    double bestPopulation=-1;
    cJSON *item=NULL;
    cJSON_ArrayForEach(item,results){
        cJSON *name=cJSON_GetObjectItemCaseSensitive(item,"name");
        cJSON *admin=cJSON_GetObjectItemCaseSensitive(item,"admin1");
        cJSON *lat=cJSON_GetObjectItemCaseSensitive(item,"latitude");
        cJSON *lon=cJSON_GetObjectItemCaseSensitive(item,"longitude");
        cJSON *population=cJSON_GetObjectItemCaseSensitive(item,"population");
        if(!cJSON_IsString(name) || !cJSON_IsNumber(lat) || !cJSON_IsNumber(lon)) continue;
        if(strncmp(name->valuestring,city,strlen(city))!=0) continue;
        if(province[0] && (!cJSON_IsString(admin) ||
            (!strstr(admin->valuestring,province) && !strstr(province,admin->valuestring)))) continue;
        double size=cJSON_IsNumber(population) ? population->valuedouble : 0;
        if(!best || size>bestPopulation){ best=item; bestPopulation=size; }
    }
    if(best){
        *latitude=cJSON_GetObjectItemCaseSensitive(best,"latitude")->valuedouble;
        *longitude=cJSON_GetObjectItemCaseSensitive(best,"longitude")->valuedouble;
    }
    cJSON_Delete(json);
    return best!=NULL;
}

static int weatherResolveCity(uint8_t provinceIndex,uint8_t cityIndex,
                              WeatherResponse *response,double *latitude,double *longitude){
    if(provinceIndex>=29 || cityIndex>=23) return 0;
    const char *cityGb=getCityTxt(provinceIndex,cityIndex+1);
    const char *provinceGb=getCityTxt(provinceIndex,0);
    if(!cityGb || !provinceGb) return 0;
    char city[96]={0},province[96]={0};
    size_t cityLength=gb2312_to_utf8((const uint8_t *)cityGb,strlen(cityGb),
                                     (uint8_t *)city,sizeof(city)-1);
    if(!cityLength || cityLength>=sizeof(city)) return 0;
    city[cityLength]=0;
    const char *country="CN";
    if(provinceIndex==28){
        country=cityIndex==0 ? "HK" : "MO";
    }else if(provinceIndex!=0){
        size_t provinceLength=gb2312_to_utf8((const uint8_t *)provinceGb,strlen(provinceGb),
                                            (uint8_t *)province,sizeof(province)-1);
        if(provinceLength>=sizeof(province)) return 0;
        province[provinceLength]=0;
    }
    // City names such as 长沙 first match villages; prefer the city-level name.
    if(provinceIndex!=0 && provinceIndex!=28 && cityLength+3<sizeof(city)){
        memcpy(city+cityLength,"\xE5\xB8\x82",4);
        if(weatherFindCity(city,province,country,response,latitude,longitude)) return 1;
        city[cityLength]=0;
    }
    return weatherFindCity(city,province,country,response,latitude,longitude);
}

static const char *weatherDescription(int code){
    switch(code){
        case 0: return "晴";
        case 1: return "晴间多云";
        case 2: return "多云";
        case 3: return "阴";
        case 45: case 48: return "雾";
        case 51: case 53: case 55: return "毛毛雨";
        case 56: case 57: return "冻雨";
        case 61: return "小雨";
        case 63: return "中雨";
        case 65: return "大雨";
        case 66: case 67: return "冻雨";
        case 71: return "小雪";
        case 73: return "中雪";
        case 75: return "大雪";
        case 77: return "雪粒";
        case 80: return "阵雨";
        case 81: return "中阵雨";
        case 82: return "强阵雨";
        case 85: return "阵雪";
        case 86: return "强阵雪";
        case 95: return "雷阵雨";
        case 96: case 99: return "雷阵雨";
        default: return "未知";
    }
}

static uint8_t weatherIcon(int code,uint8_t night){
    switch(code){
        case 0: return night ? 1 : 0;
        case 1: return night ? 3 : 2;
        case 2: return night ? 6 : 5;
        case 3: return 9;
        case 45: case 48: return 30;
        case 51: case 53: case 55: return 13;
        case 56: case 57: case 66: case 67: return 19;
        case 61: return 13;
        case 63: return 14;
        case 65: return 15;
        case 71: return 22;
        case 73: return 23;
        case 75: return 24;
        case 77: return 22;
        case 80: return 10;
        case 81: return 14;
        case 82: return 15;
        case 85: return 21;
        case 86: return 24;
        case 95: return 11;
        case 96: case 99: return 12;
        default: return 39;
    }
}

static int weatherParseForecast(cJSON *json,WeatherResult *forecast,uint8_t nightIcons[3]){
    cJSON *current=cJSON_GetObjectItemCaseSensitive(json,"current");
    cJSON *currentCode=cJSON_GetObjectItemCaseSensitive(current,"weather_code");
    cJSON *currentTemperature=cJSON_GetObjectItemCaseSensitive(current,"temperature_2m");
    cJSON *isDay=cJSON_GetObjectItemCaseSensitive(current,"is_day");
    if(!cJSON_IsNumber(currentCode) || !cJSON_IsNumber(currentTemperature) ||
       !cJSON_IsNumber(isDay)) return 0;
    const char *currentDescription=weatherDescription(currentCode->valueint);
    size_t currentLength=utf8_to_gb2312((const uint8_t *)currentDescription,
        strlen(currentDescription),(uint8_t *)forecast->currentCondition,
        sizeof(forecast->currentCondition)-1);
    if(!currentLength || currentLength>=sizeof(forecast->currentCondition)) return 0;
    forecast->currentCondition[currentLength]=0;
    forecast->currentIcon=weatherIcon(currentCode->valueint,isDay->valueint==0);
    double currentValue=currentTemperature->valuedouble;
    int currentRounded=(int)(currentValue+(currentValue>=0 ? 0.5 : -0.5));
    snprintf(forecast->currentTemperature,sizeof(forecast->currentTemperature),"%d",currentRounded);

    cJSON *daily=cJSON_GetObjectItemCaseSensitive(json,"daily");
    cJSON *codes=cJSON_GetObjectItemCaseSensitive(daily,"weather_code");
    cJSON *highs=cJSON_GetObjectItemCaseSensitive(daily,"temperature_2m_max");
    cJSON *lows=cJSON_GetObjectItemCaseSensitive(daily,"temperature_2m_min");
    cJSON *dates=cJSON_GetObjectItemCaseSensitive(daily,"time");
    cJSON *hourly=cJSON_GetObjectItemCaseSensitive(json,"hourly");
    cJSON *humidities=cJSON_GetObjectItemCaseSensitive(hourly,"relative_humidity_2m");
    if(cJSON_GetArraySize(codes)<3 || cJSON_GetArraySize(highs)<3 ||
       cJSON_GetArraySize(lows)<3 || cJSON_GetArraySize(dates)<3) return 0;
    for(int day=0;day<3;++day){
        cJSON *code=cJSON_GetArrayItem(codes,day);
        cJSON *high=cJSON_GetArrayItem(highs,day);
        cJSON *low=cJSON_GetArrayItem(lows,day);
        cJSON *date=cJSON_GetArrayItem(dates,day);
        if(!cJSON_IsNumber(code) || !cJSON_IsNumber(high) || !cJSON_IsNumber(low) ||
           !cJSON_IsString(date)) return 0;
        int weatherCodeValue=code->valueint;
        forecast->icon[day]=weatherIcon(weatherCodeValue,0);
        nightIcons[day]=weatherIcon(weatherCodeValue,1);
        const char *description=weatherDescription(weatherCodeValue);
        size_t length=utf8_to_gb2312((const uint8_t *)description,strlen(description),
                                     (uint8_t *)forecast->condition[day],
                                     sizeof(forecast->condition[day])-1);
        if(length>=sizeof(forecast->condition[day])) return 0;
        forecast->condition[day][length]=0;
        int max=(int)(high->valuedouble+(high->valuedouble>=0 ? 0.5 : -0.5));
        int min=(int)(low->valuedouble+(low->valuedouble>=0 ? 0.5 : -0.5));
        snprintf(forecast->temperature[day],sizeof(forecast->temperature[day]),"%d~%d",min,max);
        int year,month,monthDay;
        if(sscanf(date->valuestring,"%d-%d-%d",&year,&month,&monthDay)!=3) return 0;
        struct tm dateValue={.tm_year=year-1900,.tm_mon=month-1,.tm_mday=monthDay,.tm_hour=12};
        if(mktime(&dateValue)==(time_t)-1) return 0;
        forecast->weekday[day]=dateValue.tm_wday;
        int sum=0,count=0;
        for(int hour=day*24;hour<(day+1)*24;++hour){
            cJSON *humidity=cJSON_GetArrayItem(humidities,hour);
            if(cJSON_IsNumber(humidity)){ sum+=humidity->valueint; ++count; }
        }
        if(count) snprintf(forecast->humidity[day],sizeof(forecast->humidity[day]),"%d%%",(sum+count/2)/count);
        else snprintf(forecast->humidity[day],sizeof(forecast->humidity[day]),"--");
    }
    return 1;
}

static int weatherFetch(WeatherResponse *response,uint32_t serial){
    const uint8_t province=dataSaveInfo.provice;
    const uint8_t city=dataSaveInfo.city;
    double latitude,longitude;
    if(!weatherResolveCity(province,city,response,&latitude,&longitude)){
        ESP_LOGW(TAG,"Cannot resolve selected city (%u, %u)",province,city);
        return 0;
    }
    char url[384];
    snprintf(url,sizeof(url),
        "https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f"
        "&current=temperature_2m,weather_code,is_day"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min"
        "&hourly=relative_humidity_2m&timezone=Asia%%2FShanghai&forecast_days=3",
        latitude,longitude);
    cJSON *json=weatherGetJson(url,response);
    if(!json) return 0;
    WeatherResult forecast={0};
    uint8_t nightIcons[3]={0};
    int valid=weatherParseForecast(json,&forecast,nightIcons);
    cJSON_Delete(json);
    if(!valid || dataSaveInfo.provice!=province || dataSaveInfo.city!=city){
        ESP_LOGW(TAG,"Incomplete forecast or city changed during request");
        return 0;
    }
    portENTER_CRITICAL(&forecastMux);
    if(requestSerial!=serial){
        portEXIT_CRITICAL(&forecastMux);
        return 0;
    }
    memcpy(textDayArr,forecast.condition,sizeof(forecast.condition));
    memcpy(textNightArr,forecast.condition,sizeof(forecast.condition));
    memcpy(tempHightLowArr,forecast.temperature,sizeof(forecast.temperature));
    memcpy(humidityNum,forecast.humidity,sizeof(forecast.humidity));
    memcpy(weatherDayCode,forecast.icon,sizeof(forecast.icon));
    memcpy(weatherNightCode,nightIcons,sizeof(nightIcons));
    memcpy(forecastWeekday,forecast.weekday,sizeof(forecast.weekday));
    weatherCode=forecast.currentIcon;
    memcpy(weatherTxt,forecast.currentCondition,sizeof(forecast.currentCondition));
    weatherTxt[sizeof(forecast.currentCondition)]=0;
    memcpy(tempNetTxt,forecast.currentTemperature,sizeof(forecast.currentTemperature));
    forecastReady=1;
    ++forecastVersion;
    portEXIT_CRITICAL(&forecastMux);
    ESP_LOGI(TAG,"Forecast loaded for %.4f, %.4f",latitude,longitude);
    return 1;
}

static void weatherTask(void *argument){
    WeatherResponse *response=heap_caps_malloc(sizeof(*response),
                                                MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!response) response=malloc(sizeof(*response));
    if(!response){ ESP_LOGE(TAG,"No memory for forecast response"); vTaskDelete(NULL); }
    while(1){
        // TLS certificate validation needs the clock synchronized first.
        if(IPAdressGet && getweather && time(NULL)>1704067200){
            const uint32_t serial=requestSerial;
            if(weatherFetch(response,serial)){
                portENTER_CRITICAL(&forecastMux);
                if(requestSerial==serial) getweather=false;
                portEXIT_CRITICAL(&forecastMux);
            }else ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(60000));
        }else{
            ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000));
        }
    }
}

void weatherForecastInit(void){
    weatherForecastRequest(0);
    xTaskCreate(weatherTask,"weather_task",6144,NULL,1,&weatherTaskHandle);
}
