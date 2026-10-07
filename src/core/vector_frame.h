#ifndef VX_VECTOR_FRAME_H
#define VX_VECTOR_FRAME_H

#include "core/math.h"
#include <stdbool.h>
#include <stddef.h>

enum { VX_WIDTH = 600, VX_HEIGHT = 800, VX_MAX_LINES = 768 };
typedef enum { VX_VIEW_PORTRAIT, VX_VIEW_CLASSIC, VX_VIEW_WIDE, VX_VIEW_COUNT } VxViewMode;
typedef struct { float left, top, right, bottom; } VxRect;
typedef struct {
    VxVec2 centre;
    float focal_length, near_plane, far_plane;
    VxRect clip;
    VxVec3 position;
    float pitch, yaw, roll;
} VxCamera;
typedef struct { VxVec2 a, b; float intensity; } VxLine;
typedef struct {
    size_t requested, clipped, dropped;
    float total_length;
} VxFrameStats;

/* Fixed storage makes the vector budget explicit; no allocations during play.
   Submit important geometry first so scenery yields when the budget fills. */
typedef struct {
    VxLine lines[VX_MAX_LINES];
    size_t count, budget;
    VxFrameStats stats;
    VxVec2 canvas_size;
} VxVectorFrame;

VxVec2 vx_view_size(VxViewMode mode);
VxCamera vx_camera_for_view(VxViewMode mode);
VxCamera vx_camera_default(void);
bool vx_clip_line(VxVec2 *a, VxVec2 *b, VxRect rect);
bool vx_project_line(VxVec3 a, VxVec3 b, const VxCamera *camera, VxVec2 *out_a, VxVec2 *out_b);
void vx_frame_init(VxVectorFrame *frame, size_t budget);
void vx_frame_clear(VxVectorFrame *frame);
void vx_frame_line(VxVectorFrame *frame, VxVec2 a, VxVec2 b, float intensity);
void vx_frame_world_line(VxVectorFrame *frame, VxVec3 a, VxVec3 b,
                         const VxCamera *camera, float intensity);

#endif
