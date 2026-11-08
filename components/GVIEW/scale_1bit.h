#ifndef GVIEW_SCALE_1BIT_H
#define GVIEW_SCALE_1BIT_H

#include <stddef.h>
#include <stdint.h>

static inline size_t gview_1bit_bytes(uint16_t width, uint16_t height)
{
    return ((size_t)width * height + 7) / 8;
}

/* Both source and destination use the engine's contiguous, LSB-first pixels. */
static inline void gview_scale_1bit(const uint8_t *source, uint16_t source_width,
                                    uint16_t source_height, uint8_t *destination,
                                    uint16_t dest_width, uint16_t dest_height)
{
    if (!source_width || !source_height || !dest_width || !dest_height) return;

    for (uint16_t y = 0; y < dest_height; ++y) {
        const size_t source_y = (size_t)y * source_height / dest_height;
        for (uint16_t x = 0; x < dest_width; ++x) {
            const size_t source_x = (size_t)x * source_width / dest_width;
            const size_t source_pixel = source_y * source_width + source_x;
            if (source[source_pixel >> 3] & (1u << (source_pixel & 7))) {
                const size_t dest_pixel = (size_t)y * dest_width + x;
                destination[dest_pixel >> 3] |= 1u << (dest_pixel & 7);
            }
        }
    }
}

#endif
