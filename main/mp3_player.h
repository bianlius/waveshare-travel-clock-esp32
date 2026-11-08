#ifndef GVIEW_MP3_PLAYER_H
#define GVIEW_MP3_PLAYER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The file must be 12 kHz, mono MP3, matching the ES8311 output clock.
bool mp3_player_start(const uint8_t *file, size_t length);
bool mp3_player_start_once(const uint8_t *file, size_t length);
void mp3_player_stop(void);
bool mp3_player_is_active(void);
int16_t mp3_player_render(uint8_t volume);

#endif
