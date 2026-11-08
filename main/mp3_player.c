#include "mp3_player.h"

#include <limits.h>
#include <stdatomic.h>
#include <string.h>

#define MINIMP3_ONLY_MP3
#define MINIMP3_IMPLEMENTATION
#include "third_party/minimp3/minimp3.h"

#define MP3_OUTPUT_RATE 12000

static const uint8_t *file_start;
static size_t file_size;
static size_t byte_position;
static mp3dec_t decoder;
static int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
static unsigned pcm_count;
static unsigned pcm_position;
static atomic_bool active;
static bool looping;

void mp3_player_stop(void)
{
    atomic_store(&active, false);
    file_start = NULL;
    file_size = 0;
    byte_position = 0;
    pcm_count = 0;
    pcm_position = 0;
}

static bool next_frame(void)
{
    // Permit one wrap at EOF. A malformed file must never trap the audio task.
    unsigned wraps = 0;
    while (wraps < 2) {
        if (byte_position >= file_size) {
            if (!looping) return false;
            byte_position = 0;
            mp3dec_init(&decoder);
            ++wraps;
            continue;
        }
        const size_t available = file_size - byte_position;
        const int input_size = available > INT_MAX ? INT_MAX : (int)available;
        mp3dec_frame_info_t info = {0};
        const int count = mp3dec_decode_frame(&decoder, file_start + byte_position,
                                               input_size, pcm, &info);
        if (info.frame_bytes <= 0 || info.frame_bytes > input_size) {
            byte_position = file_size;
            continue;
        }
        byte_position += (size_t)info.frame_bytes;
        if (count <= 0) continue;
        if (info.channels != 1 || info.hz != MP3_OUTPUT_RATE ||
            count > MINIMP3_MAX_SAMPLES_PER_FRAME)
            return false;
        pcm_count = (unsigned)count;
        pcm_position = 0;
        return true;
    }
    return false;
}

static bool start(const uint8_t *file, size_t length, bool repeat)
{
    mp3_player_stop();
    if (!file || length < 4 || file[0] != 0xff ||
        (file[1] & 0xe0) != 0xe0 || (file[1] & 0x06) != 0x02)
        return false;
    file_start = file;
    file_size = length;
    looping = repeat;
    mp3dec_init(&decoder);
    // First-frame decoding uses a large scratch buffer. Run it in audio_out,
    // whose stack is sized for the decoder, rather than the 4 KB sound task.
    atomic_store(&active, true);
    return true;
}

bool mp3_player_start(const uint8_t *file, size_t length)
{
    return start(file, length, true);
}

bool mp3_player_start_once(const uint8_t *file, size_t length)
{
    return start(file, length, false);
}

bool mp3_player_is_active(void) { return atomic_load(&active); }

int16_t mp3_player_render(uint8_t volume)
{
    if (!mp3_player_is_active()) return 0;
    if (pcm_position >= pcm_count && !next_frame()) {
        mp3_player_stop();
        return 0;
    }
    return (int16_t)((int32_t)pcm[pcm_position++] * volume / 100);
}
