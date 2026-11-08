#ifndef __SOUND_H__
#define __SOUND_H__

#include "config.h"
#include <stdbool.h>
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern volatile u16 aClockTimes;
void setSoundVal(u8 volume);
void soundInit(void);
void soundTask(void *arg);
void soundStopAll(void);
void homeBgmSuspend(void);
void homeBgmResume(void);
bool soundPreviewStart(u8 slot);
void soundPreviewStop(void);
int soundPreviewSelection(void);

#endif
