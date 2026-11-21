#pragma once
#include <stdint.h>
// Called once per rendered strip before the transport consumes it.
void passport_draw_overlay(void);
void passport_display_start(void);
// App-task-only profiling counter; reading resets the accumulated DMA wait.
uint64_t passport_display_take_wait_us(void);
