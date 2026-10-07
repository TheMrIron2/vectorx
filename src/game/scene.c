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
    const float tail = 0.52f + 0.08f * sinf(flight->time * 24.0f);
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
        const float z = 6.0f + (float)i * 12.0f - flight->rail_offset;
        if (z <= camera->near_plane) continue;
        const float intensity = 0.07f + 0.20f * (1.0f - z / 100.0f);
        vx_frame_world_line(frame, (VxVec3){-8, floor_y, z}, (VxVec3){8, floor_y, z}, camera, intensity);
        vx_frame_world_line(frame, (VxVec3){-8, floor_y, z}, (VxVec3){-8, -3.5f, z}, camera, intensity);
        vx_frame_world_line(frame, (VxVec3){8, floor_y, z}, (VxVec3){8, -3.5f, z}, camera, intensity);
    }
    /* Fixed-seed points keep scenery repeatable for visual comparisons. */
    for (int i = 0; i < 28; ++i) {
        const float x = (float)((i * 73 + 19) % 540) + 30;
        const float y = (float)((i * 47 + 29) % 224) + 90;
        vx_frame_line(frame, (VxVec2){x, y}, (VxVec2){x + 0.5f, y}, 0.18f);
    }
}

void vx_build_scene(VxVectorFrame *frame, const VxFlight *flight, const VxWeapons *weapons) {
    vx_frame_clear(frame);
    const VxCamera camera = vx_camera_default();
    ship(frame, flight, &camera); /* Player gets first use of the segment budget. */
    for (int i = 0; i < VX_MAX_PROJECTILES; ++i) {
        const VxProjectile projectile = weapons->projectiles[i];
        if (!projectile.active) continue;
        vx_frame_world_line(frame, projectile.position,
            vx_v3_add(projectile.position, (VxVec3){0, 0, 1.8f}), &camera, 0.9f);
    }
    corridor(frame, flight, &camera);
}
