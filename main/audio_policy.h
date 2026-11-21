#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SOUND_MODE_NONE, SOUND_MODE_HOME, SOUND_MODE_PREVIEW, SOUND_MODE_ALARM
} SoundPlaybackMode;
typedef struct { int slot; SoundPlaybackMode mode; } SoundPlayback;

int soundAlarmSlot(void);
bool soundPreviewCanStart(uint8_t slot, bool available);
SoundPlayback soundPlaybackSelect(bool available, unsigned volume,
                                 bool home_allowed, int preview_slot);

// A finite count includes one extra sentinel; values above 100 repeat forever.
static inline uint16_t soundAlarmStarted(uint16_t count){
    return count && count <= 100 ? count - 1 : count;
}
static inline uint16_t soundAlarmFinished(uint16_t count){
    return count == 1 ? 0 : count;
}
