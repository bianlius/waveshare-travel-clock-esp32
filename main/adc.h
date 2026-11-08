#ifndef __ADC_H__
#define __ADC_H__

#include "config.h"
#include "GVIEW/gview.h"
#include "driver/i2c_master.h"
i2c_master_bus_handle_t waveshare_i2c_bus(void);


extern u16 KeyBtn_Val;      // 功能按键??
extern u16 VoltageVal;      // 电池电压??
extern u8 roundStateGet;



#define keySureTime 2

//版本2.1
// #define keyVal0 1480
// #define keyVal1 1925
// #define keyVal2 2715
// #define keyVal3 1111
// #define keyVal4 1111

// #define KEY3DOWN !gpio_get_level(9)
// #define KEY4DOWN !gpio_get_level(0)

//版本3.0
#define keyVal0 2907
#define keyVal1 3305
#define keyVal2 3917
#define keyVal3 1582
#define keyVal4 205

extern uint8_t longRDown,longLDown;

void adcInit(void);
void requestPowerOff(void);
void adc_task(void *arg);
void keyCheckC();
void keyCheckLR(u16 KeyADCNum);
void keyCheckM(u16 KeyADCNum);

#endif
