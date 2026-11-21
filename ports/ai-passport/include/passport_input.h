#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct { uint64_t started; bool down, held; } PassportButton;
typedef struct { PassportButton buttons[3]; } PassportInput;
enum { PASSPORT_NO_ACTION=-1, PASSPORT_HOLD_MS=650, PASSPORT_MIDDLE_HOLD_MS=1500 };
// Slots follow the Waveshare layout: left=0, middle=1, right=2.
int passport_input_event(PassportInput *,unsigned slot,bool down,uint64_t milliseconds);
int passport_input_poll(PassportInput *,uint64_t milliseconds);

// Physical positions are left/middle/right in the default landscape direction.
// Reversing the display swaps the side positions; the middle stays in place.
typedef struct {
    PassportInput logical;
    uint8_t rotation, down_mask, suppressed_mask;
} PassportOrientedInput;
int passport_landscape_slot(unsigned position,unsigned rotation);
int passport_oriented_event(PassportOrientedInput *,unsigned position,bool down,
                            unsigned rotation,uint64_t milliseconds);
int passport_oriented_poll(PassportOrientedInput *,unsigned rotation,uint64_t milliseconds);

void passport_dispatch(int action);
