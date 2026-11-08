#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "scale_1bit.h"
#include "FN0.h"
#include "FNM.h"

static int pixel(const uint8_t *bits, size_t index)
{
    return (bits[index >> 3] >> (index & 7)) & 1;
}

static void check_sprite(const uint8_t *sprite, uint16_t dest_width,
                         uint16_t dest_height)
{
    assert(sprite[0] == 1 && sprite[1] == 1);
    const uint16_t source_width = ((uint16_t)sprite[2] << 8) | sprite[3];
    const uint16_t source_height = ((uint16_t)sprite[4] << 8) | sprite[5];
    const uint8_t *source = sprite + 9;
    const size_t bytes = gview_1bit_bytes(dest_width, dest_height);
    uint8_t *guarded = malloc(bytes + 2);
    assert(guarded);
    memset(guarded, 0, bytes + 2);
    guarded[0] = 0x5a;
    guarded[bytes + 1] = 0xa5;

    gview_scale_1bit(source, source_width, source_height, guarded + 1,
                     dest_width, dest_height);

    assert(guarded[0] == 0x5a && guarded[bytes + 1] == 0xa5);
    for (uint16_t y = 0; y < dest_height; ++y) {
        for (uint16_t x = 0; x < dest_width; ++x) {
            const size_t sx = (size_t)x * source_width / dest_width;
            const size_t sy = (size_t)y * source_height / dest_height;
            assert(pixel(guarded + 1, (size_t)y * dest_width + x) ==
                   pixel(source, sy * source_width + sx));
        }
    }
    free(guarded);
}

int main(void)
{
    check_sprite(FN0, 30, 45);
    check_sprite(FNM, 7, 23);
    return 0;
}
