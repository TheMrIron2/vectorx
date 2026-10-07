#include "game/flight.h"

void vx_flight_reset(VxFlight *flight) {
    *flight = (VxFlight){0};
    flight->position.y = -2.5f;
}
void vx_flight_update(VxFlight *flight, VxInput input, float dt) {
    if (!isfinite(dt) || dt <= 0.0f || !isfinite(input.horizontal) ||
        !isfinite(input.vertical)) return;
    VxVec2 direction = {vx_clamp(input.horizontal, -1.0f, 1.0f),
                         vx_clamp(input.vertical, -1.0f, 1.0f)};
    const float magnitude = vx_v2_length(direction);
    if (magnitude > 1.0f) direction = vx_v2_scale(direction, 1.0f / magnitude);
    const float response = 1.0f - expf(-18.0f * dt);
    const VxVec2 target = vx_v2_scale(direction, VX_SHIP_SPEED);
    flight->velocity = vx_v2_add(flight->velocity,
        vx_v2_scale(vx_v2_sub(target, flight->velocity), response));
    flight->position = vx_v2_add(flight->position, vx_v2_scale(flight->velocity, dt));
    if (flight->position.x < VX_MIN_X || flight->position.x > VX_MAX_X) {
        flight->position.x = vx_clamp(flight->position.x, VX_MIN_X, VX_MAX_X);
        flight->velocity.x = 0.0f;
    }
    if (flight->position.y < VX_MIN_Y || flight->position.y > VX_MAX_Y) {
        flight->position.y = vx_clamp(flight->position.y, VX_MIN_Y, VX_MAX_Y);
        flight->velocity.y = 0.0f;
    }
    const float bank_response = 1.0f - expf(-9.0f * dt);
    flight->roll += (-direction.x * 0.48f - flight->roll) * bank_response;
    flight->pitch += (-direction.y * 0.12f - flight->pitch) * bank_response;
    flight->yaw += (direction.x * 0.10f - flight->yaw) * bank_response;
    flight->rail_offset = fmodf(flight->rail_offset + dt * 8.0f, 12.0f);
    flight->time = fmodf(flight->time + dt, 120.0f);
}
