#ifndef VX_MATH_H
#define VX_MATH_H

#include <math.h>

typedef struct { float x, y; } VxVec2;
typedef struct { float x, y, z; } VxVec3;

static inline float vx_clamp(float value, float low, float high) {
    return fminf(fmaxf(value, low), high);
}
static inline VxVec2 vx_v2_add(VxVec2 a, VxVec2 b) { return (VxVec2){a.x + b.x, a.y + b.y}; }
static inline VxVec2 vx_v2_sub(VxVec2 a, VxVec2 b) { return (VxVec2){a.x - b.x, a.y - b.y}; }
static inline VxVec2 vx_v2_scale(VxVec2 a, float s) { return (VxVec2){a.x * s, a.y * s}; }
static inline VxVec3 vx_v3_add(VxVec3 a, VxVec3 b) { return (VxVec3){a.x + b.x, a.y + b.y, a.z + b.z}; }
static inline VxVec3 vx_v3_sub(VxVec3 a, VxVec3 b) { return (VxVec3){a.x - b.x, a.y - b.y, a.z - b.z}; }
static inline VxVec3 vx_v3_scale(VxVec3 a, float s) { return (VxVec3){a.x * s, a.y * s, a.z * s}; }
static inline float vx_v2_length(VxVec2 v) { return sqrtf(v.x * v.x + v.y * v.y); }

/* Camera convention: +X right, +Y up, +Z into the screen. */
static inline VxVec3 vx_rotate(VxVec3 v, float pitch, float yaw, float roll) {
    const float cp = cosf(pitch), sp = sinf(pitch);
    const float cy = cosf(yaw), sy = sinf(yaw);
    const float cr = cosf(roll), sr = sinf(roll);
    v = (VxVec3){v.x, v.y * cp - v.z * sp, v.y * sp + v.z * cp};
    v = (VxVec3){v.x * cy + v.z * sy, v.y, -v.x * sy + v.z * cy};
    return (VxVec3){v.x * cr - v.y * sr, v.x * sr + v.y * cr, v.z};
}

static inline VxVec3 vx_inverse_rotate(VxVec3 v, float pitch, float yaw, float roll) {
    /* Undo roll, yaw, then pitch: the reverse of vx_rotate's composition. */
    const float cr = cosf(roll), sr = sinf(roll);
    const float cy = cosf(yaw), sy = sinf(yaw);
    const float cp = cosf(pitch), sp = sinf(pitch);
    v = (VxVec3){v.x * cr + v.y * sr, -v.x * sr + v.y * cr, v.z};
    v = (VxVec3){v.x * cy - v.z * sy, v.y, v.x * sy + v.z * cy};
    return (VxVec3){v.x, v.y * cp + v.z * sp, -v.y * sp + v.z * cp};
}

#endif
