#ifndef VX_GL_RENDERER_H
#define VX_GL_RENDERER_H

#include "core/vector_frame.h"
#include "core/render_settings.h"
#include <SDL3/SDL.h>

typedef struct {
    unsigned int program, vertex_array, vertex_buffer;
    int beam_width_uniform, glow_radius_uniform, glow_strength_uniform;
    int beam_colour_uniform, glow_colour_uniform;
} VxGlRenderer;

bool vx_gl_init(VxGlRenderer *renderer);
void vx_gl_shutdown(VxGlRenderer *renderer);
void vx_gl_draw(VxGlRenderer *renderer, const VxVectorFrame *frame,
                int pixel_width, int pixel_height, const VxRenderSettings *settings);
bool vx_gl_save_bmp(const char *path, int width, int height);

#endif
