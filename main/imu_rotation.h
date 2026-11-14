#ifndef IMU_ROTATION_H
#define IMU_ROTATION_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c_master.h"

// Returns 0..3 clockwise screen rotations, or -1 when the board is too flat
// or moving too much to choose a reliable upright edge.
int imuRotationFromAcceleration(int16_t x, int16_t y, int16_t z);
void imuRotationSetEnabled(bool enabled);
void imuRotationStart(i2c_master_bus_handle_t bus);

#endif
