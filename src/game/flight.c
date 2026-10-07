#include "game/flight.h"

/* Exact critically damped spring: preserve angular momentum through reversals
   while returning to level without oscillation or timestep-dependent wobble. */
static void spring(float *value, float *velocity, float target, float frequency, float dt) {
    const float offset = *value - target;
    const float step = (*velocity + frequency * offset) * dt;
    const float decay = expf(-frequency * dt);
    *value = target + (offset + step) * decay;
    *velocity = (*velocity - frequency * step) * decay;
}

void vx_flight_reset(VxFlight *flight) {
    *flight = (VxFlight){0};
    flight->position.y = -2.5f;
    flight->forward_speed = VX_FORWARD_SPEED;
}
void vx_flight_set_view(VxFlight *flight, VxViewMode mode) {
    flight->view_mode = mode >= VX_VIEW_PORTRAIT && mode < VX_VIEW_COUNT ? mode : VX_VIEW_PORTRAIT;
    const float scale = vx_view_size(flight->view_mode).x / VX_WIDTH;
    const float x = vx_clamp(flight->position.x, VX_MIN_X * scale, VX_MAX_X * scale);
    if (x != flight->position.x) flight->velocity.x = 0;
    flight->position.x = x;
    flight->camera_offset.x = vx_clamp(flight->camera_offset.x, VX_MIN_X * scale * 0.20f,
        VX_MAX_X * scale * 0.20f);
}
void vx_flight_update(VxFlight *flight, VxInput input, float dt) {
    if (!isfinite(dt) || dt <= 0.0f || !isfinite(input.horizontal) ||
        !isfinite(input.vertical)) return;
    VxVec2 direction = {vx_clamp(input.horizontal, -1.0f, 1.0f),
                         vx_clamp(input.vertical, -1.0f, 1.0f)};
    const float magnitude = vx_v2_length(direction);
    if (magnitude > 1.0f) direction = vx_v2_scale(direction, 1.0f / magnitude);
    const float horizontal_scale = vx_view_size(flight->view_mode).x / VX_WIDTH;
    const float horizontal_speed = VX_SHIP_SPEED;
    const float dot = direction.x * flight->velocity.x / horizontal_speed
        + direction.y * flight->velocity.y / VX_VERTICAL_SPEED;
    const float rate = magnitude < 0.001f ? 24.0f : (dot < 0 ? 22.0f : 16.0f);
    const float response = 1.0f - expf(-rate * dt);
    const VxVec2 target = {direction.x * horizontal_speed, direction.y * VX_VERTICAL_SPEED};
    flight->velocity = vx_v2_add(flight->velocity,
        vx_v2_scale(vx_v2_sub(target, flight->velocity), response));
    flight->position = vx_v2_add(flight->position, vx_v2_scale(flight->velocity, dt));
    if (flight->position.x < VX_MIN_X * horizontal_scale || flight->position.x > VX_MAX_X * horizontal_scale) {
        flight->position.x = vx_clamp(flight->position.x, VX_MIN_X * horizontal_scale, VX_MAX_X * horizontal_scale);
        flight->velocity.x = 0.0f;
    }
    if (flight->position.y < VX_MIN_Y || flight->position.y > VX_MAX_Y) {
        flight->position.y = vx_clamp(flight->position.y, VX_MIN_Y, VX_MAX_Y);
        flight->velocity.y = 0.0f;
    }
    /* Input anticipates the turn; actual velocity gives it weight and carries
       a small amount of bank through release/braking. The view stays restrained. */
    const VxVec2 normalized_velocity = {flight->velocity.x / horizontal_speed, flight->velocity.y / VX_VERTICAL_SPEED};
    const VxVec2 steering = vx_v2_add(vx_v2_scale(direction, 0.65f), vx_v2_scale(normalized_velocity, 0.35f));
    spring(&flight->roll, &flight->angular_velocity.z, -steering.x * VX_MAX_ROLL, 18, dt);
    spring(&flight->pitch, &flight->angular_velocity.x, -steering.y * VX_MAX_PITCH, 20, dt);
    spring(&flight->yaw, &flight->angular_velocity.y, steering.x * VX_MAX_YAW, 20, dt);

    const float follow = 1.0f - expf(-7.0f * dt);
    const VxVec2 camera_target = {flight->position.x * 0.20f, (flight->position.y + 2.5f) * 0.14f};
    flight->camera_offset = vx_v2_add(flight->camera_offset,
        vx_v2_scale(vx_v2_sub(camera_target, flight->camera_offset), follow));
    flight->camera_yaw += (steering.x * 0.032f - flight->camera_yaw) * follow;
    flight->camera_pitch += (-steering.y * 0.025f - flight->camera_pitch) * follow;
    flight->camera_roll += (flight->roll * 0.045f - flight->camera_roll) * follow;
    flight->rail_offset = fmodf(flight->rail_offset + dt * flight->forward_speed, VX_SCENERY_DEPTH);
    flight->time = fmodf(flight->time + dt, 120.0f);
}

VxFlight vx_flight_interpolate(const VxFlight *previous, const VxFlight *current, float alpha) {
    /* Render-only snapshot. Gameplay continues using the authoritative state. */
    alpha = vx_clamp(alpha, 0, 1);
    VxFlight result = *current;
    result.position = vx_v2_add(previous->position,
        vx_v2_scale(vx_v2_sub(current->position, previous->position), alpha));
    result.camera_offset = vx_v2_add(previous->camera_offset,
        vx_v2_scale(vx_v2_sub(current->camera_offset, previous->camera_offset), alpha));
#define VX_BLEND(field) result.field = previous->field + (current->field - previous->field) * alpha
    VX_BLEND(roll); VX_BLEND(pitch); VX_BLEND(yaw);
    VX_BLEND(camera_pitch); VX_BLEND(camera_yaw); VX_BLEND(camera_roll);
#undef VX_BLEND
    float distance = current->rail_offset - previous->rail_offset;
    if (distance < 0) distance += VX_SCENERY_DEPTH;
    result.rail_offset = fmodf(previous->rail_offset + distance * alpha, VX_SCENERY_DEPTH);
    float elapsed = current->time - previous->time;
    if (elapsed < 0) elapsed += 120;
    result.time = fmodf(previous->time + elapsed * alpha, 120);
    return result;
}
