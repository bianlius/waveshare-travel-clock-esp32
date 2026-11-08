#ifndef GVIEW_WAVESHARE_AUDIO_H
#define GVIEW_WAVESHARE_AUDIO_H
#include "sdkconfig.h"
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
void waveshare_audio_init(i2c_master_bus_handle_t bus);
void waveshare_audio_set_active(bool active);
void waveshare_audio_set_mp3_active(bool active, bool home_bgm);
void waveshare_audio_set_home_bgm_allowed(bool allowed);
void waveshare_audio_set_volume(uint8_t volume);
bool waveshare_audio_sample_from_isr(int16_t sample);
#endif
