#include "GVIEW/viewport.h"
#include "weather.h"
#include "../../rom.h"
#include "../HOME/home.h"
#include "../../config.h"
#include "../../NET/RTC_Internet.h"
#include "../../NET/weather_forecast.h"
#include "LCD/lcd_init.h"
#include <string.h>

extern char *getFont(uint8_t size, uint8_t type, uint8_t line);
extern uint16_t getFont16(uint8_t *s, uint8_t line);

// Weather text and cards use the active target viewport at 1:1 pixels.
// Two bits select transparent, black, blue, or dark gray ink.
static uint8_t wsNativeInk[(LCD_W * LCD_H + 3) / 4];

static int wsPhysicalX(int sourceX){ return sourceX-WS_UI_LEFT; }
static int wsPhysicalY(int sourceY){ return sourceY; }

static void wsSetNativeInk(int x, int y, uint8_t ink){
    if(x < 0 || x >= LCD_W || y < 0 || y >= LCD_H) return;
    const size_t pixel = (size_t)y * LCD_W + x;
    wsNativeInk[pixel >> 2] |= ink << ((pixel & 3) * 2);
}

static void wsDrawNativeText(const char *text, int x, int y, uint8_t ink){
    const uint8_t *p = (const uint8_t *)text;
    while(*p){
        const int chinese = *p > 0x80 && p[1];
        if(!chinese && (*p < 32 || *p > 126)){ ++p; continue; }
        for(int row=0; row<16; ++row){
            uint16_t bits = chinese ? getFont16((uint8_t *)p, row) :
                                      (uint8_t)*getFont(16, *p - 32, row);
            const int width = chinese ? 16 : 8;
            for(int col=0; col<width; ++col){
                if(bits & 1){
                    const int glyphX = chinese ? (col < 8 ? col + 8 : col - 8) : col;
                    wsSetNativeInk(x + glyphX, y + row, ink);
                }
                bits >>= 1;
            }
        }
        x += chinese ? 16 : 8;
        p += chinese ? 2 : 1;
    }
}

// The bitmap font consumes GBK bytes; show a status until the first forecast arrives.
static const char wsWaiting[]="\xB5\xC8\xB4\xFD\xCC\xEC\xC6\xF8";
static uint32_t wsDrawnForecastVersion;

// Center each line separately and never split a two-byte GBK character.
static char wsCondition[PAGE_WEATHER_COLUMNS][2][16];
static const char *const forecastDayNames[3]={"今天","明天","后天"};
static void wsSplitCondition(char dst[2][16], const char *src){
    int line = 0, col[2] = {0}, used[2] = {0};
    dst[0][0] = dst[1][0] = 0;
    while (*src && line < 2){
        const int step = (unsigned char)*src > 0x80 && src[1] ? 2 : 1;
        if (col[line] + step > 8){
            if (++line == 2) break;
        }
        if (used[line] + step >= 16) break;
        for (int j=0; j<step; ++j) dst[line][used[line]++] = *src++;
        col[line] += step;
        dst[line][used[line]] = 0;
    }
}

//???????????????
static void KeyDownM1(void){
    renderHomeInit();
}

static void KeyDownM3(void){
    renderCalendarInit();
}


//-----------------------------------------------------

void weatherEx(void){
    if(wsDrawnForecastVersion != weatherForecastVersion()) weatherInit();
    rendWin(0,0,winW,winH,BGColor);
}



void weatherInit(void){
    homeState = 1;
    BGColor = sceneBGColor;
    pageNow = PAGE_WEATHER;
    resetPageView(&weatherEx);
    LCD_SetPageViewport(&pageNow, PAGE_WEATHER, WS_UI_LEFT, 0, WS_UI_WIDTH, 240);

    LCD_SetNativeOverlay(NULL,NULL,0);
    memset(wsNativeInk,0,sizeof(wsNativeInk));
    WeatherForecastSnapshot forecast;
    weatherForecastCopy(&forecast);
    wsDrawnForecastVersion=forecast.version;
    addChild(9,newChar(12,"Open-Meteo",0xFFFF,WS_UI_MID-40,8));
    for(u8 i=0;i<PAGE_WEATHER_COLUMNS;i++){
        const short cardWidth = PAGE_WEATHER_CARD_WIDTH;
        const short x = WS_UI_LEFT+6+i*PAGE_WEATHER_CARD_PITCH;
        const short y = 32;
        const u8 forecastDay = PAGE_WEATHER_FIRST_DAY+i;
        const char *condition = forecast.ready ? forecast.condition[forecastDay] : wsWaiting;
        const char *temperature = forecast.ready ? forecast.temperature[forecastDay] : "--";
        const char *humidity = forecast.ready ? forecast.humidity[forecastDay] : "--";
        const u8 code = forecast.ready ? forecast.icon[forecastDay] : 39;
        u8 dayIndex = forecast.ready ? forecast.weekday[forecastDay] :
                      tStruct ? (tStruct->tm_wday + forecastDay) % 7 : forecastDay;
        const short tempWidth = getTxtLength(temperature) * 8 + 16;
        const short tempX = x + (cardWidth - tempWidth) / 2;
        const short humidityWidth = 18 + getTxtLength(humidity) * 8;
        const short humidityX = x + (cardWidth - humidityWidth) / 2;
        const int centerX = (wsPhysicalX(x) + wsPhysicalX(x+cardWidth)) / 2;
        addChild(0,newRoundRectDisObj(0xFFFF,cardWidth,196,x,y,6,1));
        if(PAGE_WEATHER_DAY_TITLES)
            wsDrawNativeText(forecastDayNames[forecastDay],
                centerX-getTxtLength(forecastDayNames[forecastDay])*4,y+8,3);
        const short iconY=y+(PAGE_WEATHER_DAY_TITLES ? 30 : 14);
        addChild(0,newNativePic(weatherArr[code],x+cardWidth/2-12,iconY));
        wsSplitCondition(wsCondition[i], condition);
        const int conditionLines = wsCondition[i][1][0] ? 2 : 1;
        const int conditionY = y + 52 + (PAGE_WEATHER_DAY_TITLES ? 10 : 0)
            + (2-conditionLines)*10;
        // Leave 16 pixels after the final 16-pixel condition glyph.
        // Three-day cards share value baselines even when a condition wraps.
        const int temperatureY = PAGE_WEATHER_DAY_TITLES ? y+114 :
            conditionY + conditionLines*20 + 12;
        const int humidityY = temperatureY + (PAGE_WEATHER_DAY_TITLES ? 28 : 30);
        const int weekdayY = humidityY + (PAGE_WEATHER_DAY_TITLES ? 32 : 34);
        for(u8 line=0; line<2; line++){
            if(wsCondition[i][line][0]){
                const int textX = centerX - getTxtLength(wsCondition[i][line]) * 4;
                wsDrawNativeText(wsCondition[i][line],textX,
                                 wsPhysicalY(conditionY)+line*20,1);
            }
        }
        wsDrawNativeText(temperature,wsPhysicalX(tempX),wsPhysicalY(temperatureY),1);
        if(forecast.ready)
            addChild(9,newNativePicSingle(&duc,0,tempX+getTxtLength(temperature)*8,temperatureY-2));
        addChild(9,newNativePicSingle(&water_ico,0x14DF,humidityX,humidityY-2));
        wsDrawNativeText(humidity,wsPhysicalX(humidityX+16)+2,
                         wsPhysicalY(humidityY),2);
        wsDrawNativeText(weekdays[dayIndex],
                         centerX-getTxtLength(weekdays[dayIndex])*4,
                         wsPhysicalY(weekdayY),3);
    }
    LCD_SetNativeOverlay(wsNativeInk,&pageNow,PAGE_WEATHER);

    mainViewBindKeys(KeyDownM3,KeyDownM1);
}
