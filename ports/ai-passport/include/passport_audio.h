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
