#include "game/scene.h"
#include "game/ship.h"

static void ship(VxVectorFrame *frame, const VxFlight *flight, const VxCamera *camera) {
    VxVec3 vertices[VX_SHIP_VERTEX_COUNT];
    const VxVec3 translation = {flight->position.x, flight->position.y, 8.0f};
    for (size_t i = 0; i < VX_SHIP_VERTEX_COUNT; ++i)
        vertices[i] = vx_v3_add(vx_rotate(vx_ship_vertices[i], flight->pitch, flight->yaw, flight->roll), translation);
    for (size_t i = 0; i < VX_SHIP_EDGE_COUNT; ++i) {
        const VxEdge edge = vx_ship_edges[i];
        vx_frame_world_line(frame, vertices[edge.a], vertices[edge.b], camera, 1.0f);
    }
    const float tail = 0.90f + 0.10f * sinf(flight->time * 32.0f);
    for (int side = -1; side <= 1; side += 2) {
        const VxVec3 tip = vx_v3_add(vx_rotate((VxVec3){side * 0.65f, 0.13f, -0.83f - tail},
            flight->pitch, flight->yaw, flight->roll), translation);
        for (int direction = -1; direction <= 1; direction += 2) {
            const VxVec3 start = vx_v3_add(vx_rotate((VxVec3){side * 0.65f + direction * 0.10f, 0.13f, -0.86f},
                flight->pitch, flight->yaw, flight->roll), translation);
            vx_frame_world_line(frame, start, tip, camera, 0.60f);
        }
    }
}

static void corridor(VxVectorFrame *frame, const VxFlight *flight, const VxCamera *camera) {
    const float floor_y = -6.5f;
    const float rails[] = {-8.0f, -4.0f, 4.0f, 8.0f};
    for (size_t i = 0; i < 4; ++i)
        vx_frame_world_line(frame, (VxVec3){rails[i], floor_y, 1}, (VxVec3){rails[i], floor_y, 100}, camera, 0.22f);
    for (int i = 0; i < 8; ++i) {
        const float z = 6.0f + (float)i * VX_CORRIDOR_SPACING
            - fmodf(flight->rail_offset, VX_CORRIDOR_SPACING);
        if (z <= camera->near_plane) continue;
        const float intensity = 0.07f + 0.20f * (1.0f - z / 100.0f);
        vx_frame_world_line(frame, (VxVec3){-8, floor_y, z}, (VxVec3){8, floor_y, z}, camera, intensity);
        vx_frame_world_line(frame, (VxVec3){-8, floor_y, z}, (VxVec3){-8, -3.5f, z}, camera, intensity);
        vx_frame_world_line(frame, (VxVec3){8, floor_y, z}, (VxVec3){8, -3.5f, z}, camera, intensity);
    }
    /* Actual 3D fly-by geometry: near points sweep outwards much faster than
       distant ones. Fixed seeds and bounded depth make reset/replay repeatable.
       A short depth trail supplies a speed cue without full-screen blur. */
    for (int i = 0; i < 48; ++i) {
        const float angle = (float)i * 2.399963f;
        const float radius = 7.0f + (float)((i * 17) % 7);
        const float x = cosf(angle) * radius;
        const float y = sinf(angle) * radius * 0.70f;
        const float seed_z = (float)((i * 37 + 11) % 96);
        const float z = 1.0f + fmodf(seed_z - flight->rail_offset + VX_SCENERY_DEPTH, VX_SCENERY_DEPTH);
        const float trail = flight->forward_speed * 0.025f;
        const float intensity = 0.12f + 0.32f * (1.0f - z / (VX_SCENERY_DEPTH + 1.0f));
        vx_frame_world_line(frame, (VxVec3){x, y, z}, (VxVec3){x, y, z + trail}, camera, intensity);
    }
}

VxCamera vx_scene_camera(const VxFlight *flight) {
    VxCamera camera = vx_camera_for_view(flight->view_mode);
    camera.position = (VxVec3){flight->camera_offset.x, flight->camera_offset.y, 0};
    camera.pitch = flight->camera_pitch;
    camera.yaw = flight->camera_yaw;
    camera.roll = flight->camera_roll;
    return camera;
}

void vx_build_scene(VxVectorFrame *frame, const VxFlight *flight, const VxWeapons *weapons) {
    vx_frame_clear(frame);
    frame->canvas_size = vx_view_size(flight->view_mode);
    const VxCamera camera = vx_scene_camera(flight);
    ship(frame, flight, &camera); /* Player gets first use of the segment budget. */
    for (int i = 0; i < VX_MAX_PROJECTILES; ++i) {
        const VxProjectile projectile = weapons->projectiles[i];
        if (!projectile.active) continue;
        vx_frame_world_line(frame, projectile.position,
            vx_v3_add(projectile.position, (VxVec3){0, 0, 1.8f}), &camera, 0.9f);
    }
    corridor(frame, flight, &camera);
}
