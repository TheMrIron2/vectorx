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
    for (int i = 0; i < 18; ++i) {
        vx_flight_update(&straight, (VxInput){1, 0}, (float)VX_FIXED_STEP);
        vx_flight_update(&diagonal, (VxInput){1, 1}, (float)VX_FIXED_STEP);
    }
    CHECK(close_to(straight.velocity.x / VX_SHIP_SPEED,
        vx_v2_length((VxVec2){diagonal.velocity.x / VX_SHIP_SPEED, diagonal.velocity.y / VX_VERTICAL_SPEED})));
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
        CHECK(isfinite(straight.rail_offset) && straight.rail_offset >= 0 && straight.rail_offset < VX_SCENERY_DEPTH);
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
    const float rolls[] = {-VX_MAX_ROLL, 0.0f, VX_MAX_ROLL};
    const float pitches[] = {-VX_MAX_PITCH, VX_MAX_PITCH};
    const float yaws[] = {-VX_MAX_YAW, VX_MAX_YAW};
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

static void forward_motion(void) {
    VxFlight slow, fast, coarse;
    vx_flight_reset(&slow); vx_flight_reset(&fast); vx_flight_reset(&coarse);
    slow.forward_speed = 16;
    for (int i = 0; i < 60; ++i) {
        vx_flight_update(&slow, (VxInput){0, 0}, (float)VX_FIXED_STEP);
        vx_flight_update(&fast, (VxInput){0, 0}, (float)VX_FIXED_STEP);
    }
    for (int i = 0; i < 30; ++i)
        vx_flight_update(&coarse, (VxInput){0, 0}, (float)(VX_FIXED_STEP * 2));
    CHECK(close_to(fast.rail_offset, 4 * slow.rail_offset));
    CHECK(close_to(fast.rail_offset, coarse.rail_offset));
    CHECK(close_to(fast.position.x, 0) && close_to(fast.position.y, -2.5f));

    VxWeapons weapons;
    vx_weapons_reset(&weapons);
    VxVectorFrame frame;
    vx_frame_init(&frame, VX_MAX_LINES);
    const float speeds[] = {4, VX_FORWARD_SPEED, 240};
    for (int s = 0; s < 3; ++s) {
        vx_flight_reset(&fast);
        fast.forward_speed = speeds[s];
        for (int i = 0; i < 3600; ++i) {
            vx_flight_update(&fast, (VxInput){0, 0}, (float)VX_FIXED_STEP);
            CHECK(fast.rail_offset >= 0 && fast.rail_offset < VX_SCENERY_DEPTH);
            if (i % 12 != 0) continue;
            vx_build_scene(&frame, &fast, &weapons);
            CHECK(frame.stats.dropped == 0 && isfinite(frame.stats.total_length));
        }
    }
    vx_flight_reset(&fast);
    CHECK(close_to(fast.rail_offset, 0) && close_to(fast.forward_speed, VX_FORWARD_SPEED));
}

static void handling(void) {
    VxFlight flight;
    vx_flight_reset(&flight);
    for (int i = 0; i < 30; ++i)
        vx_flight_update(&flight, (VxInput){1, 0}, (float)VX_FIXED_STEP);
    CHECK(flight.roll < -0.4f && flight.roll > -VX_MAX_ROLL);
    CHECK(flight.camera_offset.x > 0 && flight.camera_offset.x < flight.position.x);
    CHECK(flight.camera_yaw > 0 && flight.camera_yaw < 0.035f);
    CHECK(fabsf(flight.camera_roll) < 0.035f);
    const float previous_roll = flight.roll;
    vx_flight_update(&flight, (VxInput){-1, 0}, (float)VX_FIXED_STEP);
    CHECK(flight.roll < 0 && fabsf(flight.roll - previous_roll) < 0.03f);
    for (int i = 0; i < 36; ++i)
        vx_flight_update(&flight, (VxInput){-1, 0}, (float)VX_FIXED_STEP);
    CHECK(flight.roll > 0.4f && flight.velocity.x < 0);
    const float released_position = flight.position.x;
    for (int i = 0; i < 120; ++i)
        vx_flight_update(&flight, (VxInput){0, 0}, (float)VX_FIXED_STEP);
    CHECK(fabsf(flight.position.x - released_position) < 0.65f);
    CHECK(fabsf(flight.roll) < 0.001f && fabsf(flight.camera_yaw) < 0.001f);

    const VxInput corners[] = {{1, 1}, {-1, -1}, {1, -1}, {-1, 1}};
    for (int step = 0; step < 2400; ++step) {
        vx_flight_update(&flight, corners[(step / 120) % 4], (float)VX_FIXED_STEP);
        const VxCamera camera = vx_scene_camera(&flight);
        CHECK(fabsf(flight.roll) <= VX_MAX_ROLL && fabsf(flight.camera_roll) < 0.035f);
        for (size_t i = 0; i < VX_SHIP_VERTEX_COUNT; ++i) {
            const VxVec3 vertex = vx_v3_add(vx_rotate(vx_ship_vertices[i], flight.pitch, flight.yaw, flight.roll),
                (VxVec3){flight.position.x, flight.position.y, 8});
            VxVec2 a, b;
            CHECK(vx_project_line(vertex, vertex, &camera, &a, &b));
        }
    }
    VxFlight previous = flight, current = flight;
    previous.rail_offset = VX_SCENERY_DEPTH - 0.2f;
    current.rail_offset = 0.2f;
    previous.time = 119.9f; current.time = 0.1f;
    previous.position.x = -1; current.position.x = 1;
    const VxFlight blended = vx_flight_interpolate(&previous, &current, 0.5f);
    CHECK(close_to(blended.position.x, 0));
    CHECK(blended.rail_offset < 0.001f || blended.rail_offset > VX_SCENERY_DEPTH - 0.001f);
    CHECK(blended.time < 0.001f || blended.time > 119.999f);
    const VxVec3 vector = {2, 3, 4};
    const VxVec3 recovered = vx_inverse_rotate(vx_rotate(vector, 0.2f, -0.1f, 0.7f), 0.2f, -0.1f, 0.7f);
    CHECK(close_to(vector.x, recovered.x) && close_to(vector.y, recovered.y) && close_to(vector.z, recovered.z));
    vx_flight_reset(&flight);
    CHECK(close_to(flight.camera_offset.x, 0) && close_to(flight.camera_yaw, 0));
    CHECK(close_to(vx_v2_length((VxVec2){flight.angular_velocity.x, flight.angular_velocity.z}), 0));
}

static void view_modes(void) {
    const float ratios[] = {3.0f / 4, 4.0f / 3, 16.0f / 9};
    VxFlight flight;
    VxWeapons weapons;
    VxVectorFrame frame;
    vx_weapons_reset(&weapons);
    vx_frame_init(&frame, VX_MAX_LINES);
    for (int mode = 0; mode < VX_VIEW_COUNT; ++mode) {
        vx_flight_reset(&flight);
        vx_flight_set_view(&flight, (VxViewMode)mode);
        const VxVec2 size = vx_view_size((VxViewMode)mode);
        CHECK(close_to(size.x / size.y, ratios[mode]));
        const VxCamera camera = vx_scene_camera(&flight);
        VxVec2 a, b;
        CHECK(vx_project_line((VxVec3){0, 0, 8}, (VxVec3){1, 0, 8}, &camera, &a, &b));
        CHECK(close_to(b.x - a.x, 55)); /* Wider FOV, unchanged geometry scale. */
        const float horizontal_scale = size.x / VX_WIDTH;
        const VxInput directions[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, -1}};
        for (int i = 0; i < 1800; ++i) {
            vx_flight_update(&flight, directions[(i / 150) % 6], (float)VX_FIXED_STEP);
            CHECK(flight.position.x >= VX_MIN_X * horizontal_scale && flight.position.x <= VX_MAX_X * horizontal_scale);
            CHECK(flight.position.y >= VX_MIN_Y && flight.position.y <= VX_MAX_Y);
            const VxCamera moving_camera = vx_scene_camera(&flight);
            for (size_t k = 0; k < VX_SHIP_VERTEX_COUNT; ++k) {
                const VxVec3 vertex = vx_v3_add(vx_rotate(vx_ship_vertices[k], flight.pitch, flight.yaw, flight.roll),
                    (VxVec3){flight.position.x, flight.position.y, 8});
                CHECK(vx_project_line(vertex, vertex, &moving_camera, &a, &b));
            }
            if (i % 30 != 0) continue;
            vx_build_scene(&frame, &flight, &weapons);
            CHECK(close_to(frame.canvas_size.x, size.x) && frame.stats.dropped == 0);
            for (size_t k = 0; k < frame.count; ++k) {
                CHECK(frame.lines[k].a.x >= -0.01f && frame.lines[k].a.x <= size.x + 0.01f);
                CHECK(frame.lines[k].b.x >= -0.01f && frame.lines[k].b.x <= size.x + 0.01f);
            }
        }
    }
    vx_flight_set_view(&flight, VX_VIEW_WIDE);
    flight.position.x = 5.8f; flight.velocity.x = 10;
    vx_flight_set_view(&flight, VX_VIEW_PORTRAIT);
    CHECK(close_to(flight.position.x, VX_MAX_X) && close_to(flight.velocity.x, 0));
    VxFlight horizontal, vertical;
    vx_flight_reset(&horizontal); vx_flight_reset(&vertical);
    for (int i = 0; i < 18; ++i) {
        vx_flight_update(&horizontal, (VxInput){1, 0}, (float)VX_FIXED_STEP);
        vx_flight_update(&vertical, (VxInput){0, 1}, (float)VX_FIXED_STEP);
    }
    CHECK(close_to(vertical.velocity.y / horizontal.velocity.x, VX_VERTICAL_SPEED / VX_SHIP_SPEED));
    CHECK(close_to(vertical.position.y + 2.5f, horizontal.position.x));
    for (int mode = 0; mode < VX_VIEW_COUNT; ++mode) {
        VxFlight reference;
        vx_flight_reset(&reference);
        vx_flight_set_view(&reference, (VxViewMode)mode);
        for (int i = 0; i < 18; ++i)
            vx_flight_update(&reference, (VxInput){1, 0}, (float)VX_FIXED_STEP);
        CHECK(close_to(reference.velocity.x, horizontal.velocity.x));
        CHECK(close_to(reference.position.x, horizontal.position.x));
    }
}

int main(void) {
    clipping(); movement(); scene_and_budget(); ship_visibility(); shooting(); forward_motion(); handling(); view_modes();
    printf("All %d checks passed (clipping, movement, shooting, geometry and budgets).\n", checks);
    return EXIT_SUCCESS;
}
