"""Keep the sunny-day sun visible through morning, afternoon, and dusk."""

from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "main/PAGE/HOME/home.c").read_bytes().decode("gb18030")
degree_icon = [int(value, 16) for value in re.findall(
    r"0x([0-9a-fA-F]+)",
    (root / "main/pic_data/pic_default/ducHome.h").read_text())]
assert degree_icon[:6] == [1, 1, 0, 12, 0, 12] and len(degree_icon) == 27
degree_ink = [(x, y) for y in range(12) for x in range(12)
              if degree_icon[9 + (y * 12 + x) // 8] >> ((y * 12 + x) & 7) & 1]
assert min(y for _, y in degree_ink) == 1
assert max(y for _, y in degree_ink) == 10
start = source.index("void sunAction(void){")
end = source.index("\n}", start) + 2
position_start = source.index("static void positionWeatherDescription(void){")
position_end = source.index("\n}", position_start) + 2
temperature_start = source.index("void resetHomeTempX(void){")
temperature_end = source.index("\n}", temperature_start) + 2
battery_start = source.index("static void positionHomeBattery(void){")
battery_end = source.index("\n}", battery_start) + 2
plant_start = source.index("static void refreshHomePlantHint(void){")
plant_end = source.index("\n}", plant_start) + 2
production = "\n".join((source[start:end], source[position_start:position_end],
                        source[temperature_start:temperature_end],
                        source[battery_start:battery_end],source[plant_start:plant_end]))
for signature in ("static u8 enabledHomeAlarmCount(void){", "static void refreshHomeAlarmHint(void){"):
    a = source.index(signature)
    z = source.index("\n}", a) + 2
    production += "\n" + source[a:z]
home_start = source.index("void homeEx(void){")
home_end = source.index("\n}", home_start)
home = source[home_start:home_end]
assert home.index("weatherSunny = true;") < home.index("sunAction();")

harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
#define WS_HOME_RIGHT 280
#define WS_HOME_LEFT 40
#define WS_HOME_TOP_LEFT_INSET 6
#define WS_UI_RIGHT 280
#define PAGE_HOME 1
#define PAGE_CLOCK 2
#define HOME_PLANT_HINT_ON 0xD5
#define HOME_PLANT_HINT_OFF 0xD4
#define SOUND_TRACK_COUNT 10
typedef struct { int visible,x,y,width,height; } DisplayObject;
static DisplayObject sun,weather,description,temperature,degree,battery,plant,plantCount;
static DisplayObject *sunObj=&sun,*weatherObj=&weather;
static DisplayObject *weatherDescriptionObj=&description;
static DisplayObject *tempNetObj=&temperature,*tempDegreeObj=&degree;
static DisplayObject *homeBatteryObj=&battery,*homePlantHintObj=&plant;
static DisplayObject *homePlantCountObj=&plantCount;
static char homePlantCountText[4];
static DisplayObject alarm,alarmCount;
static DisplayObject *homeAlarmHintObj=&alarm,*homeAlarmCountObj=&alarmCount;
static char homeAlarmCountText[4];
static struct { u8 homePlantHintConfig; uint16_t alarmClock[5]; u8 alarmClockWeek[5]; }
    dataSaveInfo={.homePlantHintConfig=HOME_PLANT_HINT_ON};
static u8 cloverNumNow;
static u8 DayTime,weatherSunny,weatherCode;
static int pageNow;
static char weatherTxt[20];
static char tempNetTxt[10]="27";
static char homeBatteryText[24];
static unsigned getTxtLength(const char *text){ return strlen(text); }
void resetHomeTempX(void);
__PRODUCTION__
int main(void){
    sun.height=60;
    weather.y=8;
    weather.height=24;
    pageNow=PAGE_HOME;
    strcpy(weatherTxt,"\xc7\xe7"); // One GB2312 character.
    for(int day=0;day<=6;++day){
        DayTime=day;
        weatherSunny=1;
        sunAction();
        assert(sun.visible==(day>=1 && day<=4));
        assert(weather.visible==!sun.visible);
        assert(description.visible);
        assert(temperature.x==70);
        assert(temperature.y==52 && degree.x==temperature.x+18 && degree.y==54);
        weatherSunny=0;
        sunAction();
        assert(!sun.visible && weather.visible);
        assert(description.visible && temperature.x==70);
    }
    weatherCode=39;
    DayTime=0;
    weatherSunny=0;
    sunAction();
    assert(weather.visible && !description.visible && temperature.x==46);
    weatherCode=0;
    sunAction();
    assert(description.visible);
    weather.width=24;
    positionWeatherDescription();
    assert(weather.x==256 && description.x==46 && description.y==52);
    strcpy(weatherTxt,"\xc7\xe7\xc7\xe7\xc7\xe7\xc7\xe7");
    resetHomeTempX();
    assert(temperature.x==118 && degree.x==136);
    pageNow=PAGE_CLOCK;
    resetHomeTempX();
    positionWeatherDescription();
    assert(weather.x==250 && temperature.x==242 && degree.x==260);
    assert(description.x==170 && description.y==221);
    pageNow=PAGE_HOME;
    plant.width=16;
    strcpy(homeBatteryText,"\xb5\xe7\xc1\xbf 100%");
    strcpy(homePlantCountText,"1");
    positionHomeBattery();
    assert(battery.x==194 && plantCount.x==182 && plant.x==164);
    cloverNumNow=9;
    refreshHomePlantHint();
    assert(strcmp(homePlantCountText,"9")==0);
    assert(plant.visible && plantCount.visible && plantCount.x==182 && plant.x==164);
    cloverNumNow=10;
    refreshHomePlantHint();
    assert(strcmp(homePlantCountText,"10")==0);
    assert(!plant.visible && !plantCount.visible && plantCount.x==174 && plant.x==156);
    cloverNumNow=0;
    refreshHomePlantHint();
    assert(!plant.visible && !plantCount.visible);
    cloverNumNow=5;
    refreshHomePlantHint();
    assert(plant.visible && plantCount.visible);
    dataSaveInfo.homePlantHintConfig=HOME_PLANT_HINT_OFF;
    refreshHomePlantHint();refreshHomePlantHint();
    assert(!plant.visible && !plantCount.visible && cloverNumNow==5);
    dataSaveInfo.homePlantHintConfig=HOME_PLANT_HINT_ON;
    refreshHomePlantHint();assert(plant.visible && plantCount.visible);
    refreshHomeAlarmHint();assert(!alarm.visible && !alarmCount.visible);
    dataSaveInfo.alarmClock[0]=((7*60)<<4)|1;dataSaveInfo.alarmClockWeek[0]=1;
    refreshHomeAlarmHint();assert(alarm.visible && alarmCount.visible && strcmp(homeAlarmCountText,"1")==0);
    dataSaveInfo.alarmClock[1]=((8*60)<<4)|2;dataSaveInfo.alarmClockWeek[1]=64;
    refreshHomeAlarmHint();assert(strcmp(homeAlarmCountText,"2")==0);
    dataSaveInfo.alarmClock[2]=((9*60)<<4);dataSaveInfo.alarmClockWeek[2]=127; // Switched off.
    dataSaveInfo.alarmClock[3]=((9*60)<<4)|11;dataSaveInfo.alarmClockWeek[3]=127; // Invalid ring.
    dataSaveInfo.alarmClock[4]=((24*60)<<4)|1;dataSaveInfo.alarmClockWeek[4]=127; // Invalid time.
    assert(enabledHomeAlarmCount()==2);
    dataSaveInfo.alarmClockWeek[1]=128;assert(enabledHomeAlarmCount()==1);
    pageNow=PAGE_CLOCK;refreshHomeAlarmHint();assert(!alarm.visible && !alarmCount.visible);
    pageNow=PAGE_HOME;memset(dataSaveInfo.alarmClockWeek,0,5);
    refreshHomeAlarmHint();assert(!alarm.visible && !alarmCount.visible);
    puts("PASS: home weather/battery, plant visibility toggle, and enabled alarm indicator/count");
}
'''

with tempfile.TemporaryDirectory(prefix="gview-home-sun-") as folder:
    test = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    test.write_text(harness.replace("__PRODUCTION__", production), encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
