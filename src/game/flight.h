#ifndef VX_FLIGHT_H
#define VX_FLIGHT_H
#include "core/vector_frame.h"

#define VX_FIXED_STEP (1.0 / 120.0)
#define VX_MIN_X (-2.5f)
#define VX_MAX_X (2.5f)
#define VX_MIN_Y (-4.2f)
#define VX_MAX_Y (2.6f)
#define VX_SHIP_SPEED (13.75f)
#define VX_VERTICAL_SPEED (13.75f)
#define VX_FORWARD_SPEED (64.0f)
#define VX_SCENERY_DEPTH (96.0f)
#define VX_CORRIDOR_SPACING (12.0f)
#define VX_MAX_ROLL (0.72f)
#define VX_MAX_PITCH (0.22f)
#define VX_MAX_YAW (0.18f)

typedef struct { float horizontal, vertical; } VxInput;
typedef struct {
    VxVec2 position, velocity;
    float roll, pitch, yaw;
    float rail_offset, time;
    float forward_speed;
    VxVec3 angular_velocity;
    VxVec2 camera_offset;
    float camera_pitch, camera_yaw, camera_roll;
    VxViewMode view_mode;
} VxFlight;

void vx_flight_reset(VxFlight *flight);
void vx_flight_set_view(VxFlight *flight, VxViewMode mode);
void vx_flight_update(VxFlight *flight, VxInput input, float dt);
VxFlight vx_flight_interpolate(const VxFlight *previous, const VxFlight *current, float alpha);
#endif
