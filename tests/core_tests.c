#include "game/scene.h"
#include "game/ship.h"
#include <stdio.h>
#include <stdlib.h>

static int checks = 0;
#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); exit(1); } \
} while (0)
static bool close_to(float a, float b) { return fabsf(a - b) < 0.001f; }

static void clipping(void) {
    const VxRect rect = {0, 0, 1, 1};
    VxVec2 a = {-2, 0.5f}, b = {3, 0.5f};
    CHECK(vx_clip_line(&a, &b, rect));
    CHECK(close_to(a.x, 0) && close_to(b.x, 1));
    CHECK(close_to(a.y, 0.5f) && close_to(b.y, 0.5f));
    a = (VxVec2){-1, 0}; b = (VxVec2){-1, 1};
    CHECK(!vx_clip_line(&a, &b, rect));
    a = (VxVec2){0.5f, -3}; b = (VxVec2){0.5f, 2};
    CHECK(vx_clip_line(&a, &b, rect));
    CHECK(close_to(a.y, 0) && close_to(b.y, 1));
    a = (VxVec2){NAN, 0};
    CHECK(!vx_clip_line(&a, &b, rect));
    const VxCamera camera = vx_camera_default();
    CHECK(vx_project_line((VxVec3){0, 0, -1}, (VxVec3){0.1f, 0.1f, 2}, &camera, &a, &b));
    CHECK(isfinite(a.x) && isfinite(a.y) && isfinite(b.x) && isfinite(b.y));
    CHECK(!vx_project_line((VxVec3){0, 0, -2}, (VxVec3){0, 0, -1}, &camera, &a, &b));
    CHECK(!vx_project_line((VxVec3){0, 0, 101}, (VxVec3){0, 0, 200}, &camera, &a, &b));
    CHECK(vx_project_line((VxVec3){0, 0, 50}, (VxVec3){1, 0, 200}, &camera, &a, &b));
    CHECK(close_to(b.x, camera.centre.x + (1.0f / 3.0f) * camera.focal_length / 100.0f));
}

static void movement(void) {
    VxFlight straight, diagonal;
    vx_flight_reset(&straight); vx_flight_reset(&diagonal);
    for (int i = 0; i < 30; ++i) {
        vx_flight_update(&straight, (VxInput){1, 0}, (float)VX_FIXED_STEP);
        vx_flight_update(&diagonal, (VxInput){1, 1}, (float)VX_FIXED_STEP);
    }
    CHECK(close_to(vx_v2_length(straight.velocity), vx_v2_length(diagonal.velocity)));
    CHECK(straight.position.x > 0 && diagonal.position.y > -2.5f);
    CHECK(straight.roll < 0);
    for (int i = 0; i < 120; ++i)
        vx_flight_update(&straight, (VxInput){0, 0}, (float)VX_FIXED_STEP);
    CHECK(vx_v2_length(straight.velocity) < 0.001f);
    CHECK(fabsf(straight.roll) < 0.001f);
    const VxInput corners[] = {{1, 1}, {-1, -1}, {1, -1}, {-1, 1}};
    for (size_t j = 0; j < 4; ++j) {
        for (int i = 0; i < 2400; ++i)
            vx_flight_update(&straight, corners[j], (float)VX_FIXED_STEP);
        CHECK(straight.position.x >= VX_MIN_X && straight.position.x <= VX_MAX_X);
        CHECK(straight.position.y >= VX_MIN_Y && straight.position.y <= VX_MAX_Y);
        CHECK(isfinite(straight.rail_offset) && straight.rail_offset >= 0 && straight.rail_offset < 12);
    }
    const VxVec2 before = straight.position;
    vx_flight_update(&straight, (VxInput){NAN, 0}, (float)VX_FIXED_STEP);
    CHECK(close_to(before.x, straight.position.x) && close_to(before.y, straight.position.y));
    vx_flight_reset(&straight);
    CHECK(close_to(straight.position.x, 0) && close_to(straight.position.y, -2.5f));
    CHECK(close_to(straight.roll, 0) && close_to(vx_v2_length(straight.velocity), 0));
}

static void scene_and_budget(void) {
    VxVectorFrame frame;
    VxFlight flight;
    vx_flight_reset(&flight);
    VxWeapons weapons;
    vx_weapons_reset(&weapons);
    vx_frame_init(&frame, 1);
    vx_frame_line(&frame, (VxVec2){0, 0}, (VxVec2){3, 4}, 2);
    vx_frame_line(&frame, (VxVec2){1, 1}, (VxVec2){2, 2}, 1);
    CHECK(frame.count == 1 && frame.stats.dropped == 1 && frame.stats.requested == 2);
    CHECK(close_to(frame.stats.total_length, 5) && close_to(frame.lines[0].intensity, 1));
    vx_frame_init(&frame, 0);
    vx_build_scene(&frame, &flight, &weapons);
    CHECK(frame.count == 0 && frame.stats.dropped > 0);
    vx_frame_init(&frame, VX_MAX_LINES + 100);
    CHECK(frame.budget == VX_MAX_LINES);
    for (size_t i = 0; i < VX_SHIP_EDGE_COUNT; ++i)
        CHECK(vx_ship_edges[i].a < VX_SHIP_VERTEX_COUNT && vx_ship_edges[i].b < VX_SHIP_VERTEX_COUNT);
    /* Sample movement and sustained shooting: no invalid segments or budget loss. */
    for (int i = 0; i < 1440; ++i) {
        const VxInput directions[] = {{1, 1}, {-1, 0}, {0, -1}, {1, 0}};
        vx_flight_update(&flight, directions[(i / 360) % 4], (float)VX_FIXED_STEP);
        vx_weapons_update(&weapons, &flight, true, (float)VX_FIXED_STEP);
        if (i % 12 != 0) continue;
        vx_build_scene(&frame, &flight, &weapons);
        CHECK(frame.count >= VX_SHIP_EDGE_COUNT && frame.stats.dropped == 0);
        CHECK(frame.stats.requested == frame.count + frame.stats.clipped + frame.stats.dropped);
        for (size_t k = 0; k < frame.count; ++k) {
            const VxLine line = frame.lines[k];
            CHECK(isfinite(line.a.x) && isfinite(line.b.y));
            CHECK(line.a.x >= -0.01f && line.a.x <= VX_WIDTH + 0.01f);
            CHECK(line.b.x >= -0.01f && line.b.x <= VX_WIDTH + 0.01f);
            CHECK(line.a.y >= -0.01f && line.a.y <= VX_HEIGHT + 0.01f);
            CHECK(line.b.y >= -0.01f && line.b.y <= VX_HEIGHT + 0.01f);
        }
    }
}

static void ship_visibility(void) {
    const VxCamera camera = vx_camera_default();
    const float x_positions[] = {VX_MIN_X, VX_MAX_X};
    const float y_positions[] = {VX_MIN_Y, VX_MAX_Y};
    const float rolls[] = {-0.48f, 0.0f, 0.48f};
    const float pitches[] = {-0.12f, 0.12f};
    const float yaws[] = {-0.10f, 0.10f};
    for (int x = 0; x < 2; ++x) for (int y = 0; y < 2; ++y)
    for (int r = 0; r < 3; ++r) for (int p = 0; p < 2; ++p) for (int w = 0; w < 2; ++w) {
        for (size_t i = 0; i < VX_SHIP_VERTEX_COUNT; ++i) {
            VxVec3 vertex = vx_rotate(vx_ship_vertices[i], pitches[p], yaws[w], rolls[r]);
            vertex = vx_v3_add(vertex, (VxVec3){x_positions[x], y_positions[y], 8});
            const float screen_x = camera.centre.x + vertex.x * camera.focal_length / vertex.z;
            const float screen_y = camera.centre.y - vertex.y * camera.focal_length / vertex.z;
            CHECK(screen_x >= camera.clip.left && screen_x <= camera.clip.right);
            CHECK(screen_y >= camera.clip.top && screen_y <= camera.clip.bottom);
        }
    }
    /* A low rendering budget preserves all ship edges before other geometry. */
    VxVectorFrame frame;
    VxFlight flight;
    vx_flight_reset(&flight);
    VxWeapons weapons;
    vx_weapons_reset(&weapons);
    vx_frame_init(&frame, VX_SHIP_EDGE_COUNT);
    vx_build_scene(&frame, &flight, &weapons);
    CHECK(frame.count == VX_SHIP_EDGE_COUNT && frame.stats.dropped > 0);
    for (size_t i = 0; i < frame.count; ++i) CHECK(close_to(frame.lines[i].intensity, 1.0f));
}

static int active_projectiles(const VxWeapons *weapons) {
    int count = 0;
    for (int i = 0; i < VX_MAX_PROJECTILES; ++i) count += weapons->projectiles[i].active;
    return count;
}

static void shooting(void) {
    VxFlight flight;
    vx_flight_reset(&flight);
    VxWeapons weapons;
    vx_weapons_reset(&weapons);
    vx_weapons_update(&weapons, &flight, false, (float)VX_FIXED_STEP);
    CHECK(active_projectiles(&weapons) == 0);
    vx_weapons_update(&weapons, &flight, true, (float)VX_FIXED_STEP);
    CHECK(active_projectiles(&weapons) == 2);
    CHECK(close_to(weapons.projectiles[0].position.x, -0.65f));
    CHECK(close_to(weapons.projectiles[1].position.x, 0.65f));
    const VxVec3 origin = weapons.projectiles[0].position;
    vx_weapons_update(&weapons, &flight, true, (float)VX_FIXED_STEP);
    CHECK(active_projectiles(&weapons) == 2); /* Cooldown, not frame rate, governs firing. */
    CHECK(close_to(weapons.projectiles[0].position.z, origin.z + VX_PROJECTILE_SPEED * (float)VX_FIXED_STEP));
    flight.position.x = 1.0f;
    vx_weapons_update(&weapons, &flight, true, VX_FIRE_INTERVAL);
    CHECK(active_projectiles(&weapons) == 4);
    CHECK(close_to(weapons.projectiles[2].position.x, 0.35f));
    CHECK(close_to(weapons.projectiles[0].position.x, origin.x));
    for (int i = 0; i < 360; ++i)
        vx_weapons_update(&weapons, &flight, false, (float)VX_FIXED_STEP);
    CHECK(active_projectiles(&weapons) == 0); /* Released shots retire beyond the far plane. */
    for (int i = 0; i < 6000; ++i) {
        vx_weapons_update(&weapons, &flight, true, (float)VX_FIXED_STEP);
        const int count = active_projectiles(&weapons);
        CHECK(count <= VX_MAX_PROJECTILES && count % 2 == 0);
    }
    vx_weapons_reset(&weapons);
    for (int i = 0; i < VX_MAX_PROJECTILES - 1; ++i)
        weapons.projectiles[i] = (VxProjectile){{0, 0, 8}, true};
    vx_weapons_update(&weapons, &flight, true, (float)VX_FIXED_STEP);
    CHECK(active_projectiles(&weapons) == VX_MAX_PROJECTILES - 1); /* No partial pair. */
    vx_weapons_reset(&weapons);
    CHECK(active_projectiles(&weapons) == 0 && close_to(weapons.cooldown, 0));
}

int main(void) {
    clipping(); movement(); scene_and_budget(); ship_visibility(); shooting();
    printf("All %d checks passed (clipping, movement, shooting, geometry and budgets).\n", checks);
    return EXIT_SUCCESS;
}
