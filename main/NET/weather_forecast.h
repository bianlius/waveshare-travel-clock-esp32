#ifndef GVIEW_WEATHER_FORECAST_H
#define GVIEW_WEATHER_FORECAST_H

#include <stdint.h>

typedef struct {
    char condition[3][10];
    char temperature[3][10];
    char humidity[3][5];
    uint8_t icon[3];
    uint8_t weekday[3];
    uint8_t ready;
    uint32_t version;
} WeatherForecastSnapshot;

void weatherForecastInit(void);
void weatherForecastRequest(uint8_t clear_previous_city);
void weatherForecastCopy(WeatherForecastSnapshot *snapshot);
uint32_t weatherForecastVersion(void);
uint8_t weatherForecastIsReady(void);

#endif
