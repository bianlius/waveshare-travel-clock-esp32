#ifndef DESKTOP_SIMULATOR_H
#define DESKTOP_SIMULATOR_H
#include <stdint.h>
void desktop_init(const char *state_dir,const char *resources);
void desktop_tick(void);
void desktop_open(int page);
void desktop_key(int button,int held);
void desktop_touch(int x,int y,int end_x,int end_y);
void desktop_day(void);
void desktop_weather(void);
void desktop_credit(void);
void desktop_cycle_companion(int direction);
const char *desktop_companion_label(void);
void desktop_shutdown(void);
const uint32_t *desktop_pixels(void);
const char *desktop_status(void);
int desktop_smoke(void (*capture)(const char *));
void desktop_audio_play(int slot,int loop,float volume);
void desktop_audio_stop(void);
void desktop_audio_volume(float volume);
#endif
