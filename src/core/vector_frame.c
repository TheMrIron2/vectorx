#include "core/vector_frame.h"

static bool finite2(VxVec2 v) { return isfinite(v.x) && isfinite(v.y); }
static bool finite3(VxVec3 v) { return isfinite(v.x) && isfinite(v.y) && isfinite(v.z); }

VxCamera vx_camera_default(void) {
    return (VxCamera){{300.0f, 335.0f}, 440.0f, 0.5f, 100.0f, {30, 86, 570, 700}};
}

static bool clip_depth(VxVec3 *a, VxVec3 *b, float plane, bool keep_greater) {
    const bool outside_a = keep_greater ? a->z < plane : a->z > plane;
    const bool outside_b = keep_greater ? b->z < plane : b->z > plane;
    if (outside_a && outside_b) return false;
    if (outside_a != outside_b) {
        const float t = (plane - a->z) / (b->z - a->z);
        const VxVec3 point = vx_v3_add(*a, vx_v3_scale(vx_v3_sub(*b, *a), t));
        if (outside_a) *a = point; else *b = point;
    }
    return true;
}

/* Liang-Barsky clipping preserves lines crossing the view even if both
   endpoints are outside it. Clip depth before dividing by Z. */
bool vx_clip_line(VxVec2 *a, VxVec2 *b, VxRect rect) {
    if (!finite2(*a) || !finite2(*b)) return false;
    const VxVec2 delta = vx_v2_sub(*b, *a);
    const float p[] = {-delta.x, delta.x, -delta.y, delta.y};
    const float q[] = {a->x - rect.left, rect.right - a->x,
                       a->y - rect.top, rect.bottom - a->y};
    float first = 0.0f, last = 1.0f;
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0.0f) {
            if (q[i] < 0.0f) return false;
        } else {
            const float t = q[i] / p[i];
            if (p[i] < 0.0f) first = fmaxf(first, t);
            else last = fminf(last, t);
            if (first > last) return false;
        }
    }
    *b = vx_v2_add(*a, vx_v2_scale(delta, last));
    *a = vx_v2_add(*a, vx_v2_scale(delta, first));
    return true;
}

bool vx_project_line(VxVec3 a, VxVec3 b, const VxCamera *camera, VxVec2 *out_a, VxVec2 *out_b) {
    if (!finite3(a) || !finite3(b) || camera->near_plane <= 0.0f ||
        camera->far_plane <= camera->near_plane) return false;
    if (!clip_depth(&a, &b, camera->near_plane, true) ||
        !clip_depth(&a, &b, camera->far_plane, false)) return false;
    *out_a = (VxVec2){camera->centre.x + a.x * camera->focal_length / a.z,
                      camera->centre.y - a.y * camera->focal_length / a.z};
    *out_b = (VxVec2){camera->centre.x + b.x * camera->focal_length / b.z,
                      camera->centre.y - b.y * camera->focal_length / b.z};
    return vx_clip_line(out_a, out_b, camera->clip);
}

void vx_frame_init(VxVectorFrame *frame, size_t budget) {
    *frame = (VxVectorFrame){0};
    frame->budget = budget < VX_MAX_LINES ? budget : VX_MAX_LINES;
}
void vx_frame_clear(VxVectorFrame *frame) {
    frame->count = 0;
    frame->stats = (VxFrameStats){0};
}
static void submit(VxVectorFrame *frame, VxVec2 a, VxVec2 b, float intensity) {
    if (frame->count >= frame->budget) { ++frame->stats.dropped; return; }
    frame->lines[frame->count++] = (VxLine){a, b, vx_clamp(intensity, 0.0f, 1.0f)};
    frame->stats.total_length += vx_v2_length(vx_v2_sub(b, a));
}
void vx_frame_line(VxVectorFrame *frame, VxVec2 a, VxVec2 b, float intensity) {
    ++frame->stats.requested;
    if (!isfinite(intensity) || !vx_clip_line(&a, &b, (VxRect){0, 0, VX_WIDTH, VX_HEIGHT})) {
        ++frame->stats.clipped;
        return;
    }
    submit(frame, a, b, intensity);
}
void vx_frame_world_line(VxVectorFrame *frame, VxVec3 a, VxVec3 b,
                         const VxCamera *camera, float intensity) {
    ++frame->stats.requested;
    VxVec2 projected_a, projected_b;
    if (!isfinite(intensity) || !vx_project_line(a, b, camera, &projected_a, &projected_b)) {
        ++frame->stats.clipped;
        return;
    }
    submit(frame, projected_a, projected_b, intensity);
}
