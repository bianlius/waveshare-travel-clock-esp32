#ifndef __RTC_INTERNET_H__
#define __RTC_INTERNET_H__

#include "../config.h"
#include <time.h>

extern char textDayArr[3][10];
extern char textNightArr[3][10];
extern char tempHightLowArr[3][10];
extern u8 weatherNightCode[3];
extern u8 weatherDayCode[3]; 
extern char humidityNum[3][5];

void rtcInit(void);
void refrushDayTime(void);
uint16_t get_month_info(int year, int month);

extern u8 gettime;                // 是否从网络请求时间
extern volatile u8 getweather;                // 是否从网络请求天气
extern char timeString[10];
extern char dateString[30];
extern struct tm *tStruct;
extern char weekdays[7][5];

extern int year; // tm_year 从1900年开始
extern int month;     // tm_mon 范围是 0-11
extern int day;
extern int hour;        // 设置时区后，这里就是+8后的北京时间
extern int minute;
extern int second;
extern int weekday;
#endif