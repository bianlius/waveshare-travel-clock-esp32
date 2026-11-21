// Daily brightness policy shared by every device and the desktop host.
#include "rom.h"
#include "LCD/backLight.h"
#include "NET/RTC_Internet.h"

static int currentScheduleIndex = -1;

int findLastAlarmBeforeCurrent(int timeNumTemp, int alarmTime[5]) {
    int maxAlarm = -1;
    int maxIndex = -1;
    for (int i = 0; i < 5; ++i) {
        if (alarmTime[i] <= timeNumTemp && alarmTime[i] > maxAlarm) {
            maxAlarm = alarmTime[i];
            maxIndex = i;
        }
    }
    if (maxIndex < 0) {
        maxIndex = 0;
        for (int i = 1; i < 5; ++i) {
            if (alarmTime[i] > alarmTime[maxIndex]) maxIndex = i;
        }
    }
    return maxIndex;
}

static int scheduledBrightnessIndex(void){
    int alarmTime[5];
    for (u8 i = 0; i < 5; ++i) alarmTime[i] = dataSaveInfo.brightnessInfo[i] >> 4;
    const int minute = tStruct->tm_hour * 60 + tStruct->tm_min;
    return findLastAlarmBeforeCurrent(minute, alarmTime);
}

void resetBackLightVal(void){
    currentScheduleIndex = scheduledBrightnessIndex();
    setBackLightVal(dataSaveInfo.brightnessInfo[currentScheduleIndex] & 0x0F);
}

void backLightScheduleTick(void){
    // Keep the user's preview while editing a schedule entry.
    if (pageNow == PAGE_BGLIGHT_SCHEDULE) return;
    const int index = scheduledBrightnessIndex();
    if (index != currentScheduleIndex) {
        currentScheduleIndex = index;
        setBackLightVal(dataSaveInfo.brightnessInfo[index] & 0x0F);
    }
}
