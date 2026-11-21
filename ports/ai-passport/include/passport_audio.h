#pragma once
#include <stdint.h>
#define PASSPORT_AUDIO_STACK_BYTES 24576
void passport_audio_prepare_sleep(void);
typedef struct {
    uint32_t decode_us, peak_us, chunks, stack_free;
    int slot;
    unsigned volume;
} PassportAudioStats;
PassportAudioStats passport_audio_take_stats(void);
// Stored ring counts use the same extra sentinel as the Waveshare runtime.
static inline uint16_t passport_alarm_started(uint16_t count){
    return count && count<=100 ? count-1 : count;
}
static inline uint16_t passport_alarm_finished(uint16_t count){return count==1?0:count;}
