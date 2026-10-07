#ifndef VX_FLIGHT_H
#define VX_FLIGHT_H
#include "core/math.h"

#define VX_FIXED_STEP (1.0 / 120.0)
#define VX_MIN_X (-2.5f)
#define VX_MAX_X (2.5f)
#define VX_MIN_Y (-4.2f)
#define VX_MAX_Y (2.6f)
#define VX_SHIP_SPEED (3.8f)

typedef struct { float horizontal, vertical; } VxInput;
typedef struct {
    VxVec2 position, velocity;
    float roll, pitch, yaw;
    float rail_offset, time;
} VxFlight;

void vx_flight_reset(VxFlight *flight);
void vx_flight_update(VxFlight *flight, VxInput input, float dt);
#endif
