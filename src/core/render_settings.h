#ifndef VX_RENDER_SETTINGS_H
#define VX_RENDER_SETTINGS_H

#include "core/math.h"
#include <stdbool.h>

/* Runtime values, passed to the renderer as uniforms. A future settings file
   or UI can update these without changing the shader or game geometry. */
typedef struct {
    bool glow_enabled;
    float beam_width;
    float inner_glow_radius, outer_glow_radius;
    float inner_glow_strength, outer_glow_strength;
    VxVec3 beam_colour, glow_colour;
} VxRenderSettings;

static inline VxRenderSettings vx_render_settings_default(void) {
    return (VxRenderSettings){
        true, 0.67082f, 2.23607f, 5.29150f, 0.20f, 0.065f,
        {0.62f, 1.0f, 0.76f}, {0.40f, 0.95f, 0.57f}
    };
}

#endif
