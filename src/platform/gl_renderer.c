#include "platform/gl_renderer.h"
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stb_easy_font.h"

/* Resolve modern GL functions through SDL, after creating the current context. */
#define VX_GL_FUNCTIONS(X) \
    X(PFNGLCREATESHADERPROC, CreateShader) \
    X(PFNGLSHADERSOURCEPROC, ShaderSource) \
    X(PFNGLCOMPILESHADERPROC, CompileShader) \
    X(PFNGLGETSHADERIVPROC, GetShaderiv) \
    X(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog) \
    X(PFNGLDELETESHADERPROC, DeleteShader) \
    X(PFNGLCREATEPROGRAMPROC, CreateProgram) \
    X(PFNGLATTACHSHADERPROC, AttachShader) \
    X(PFNGLLINKPROGRAMPROC, LinkProgram) \
    X(PFNGLGETPROGRAMIVPROC, GetProgramiv) \
    X(PFNGLGETPROGRAMINFOLOGPROC, GetProgramInfoLog) \
    X(PFNGLDELETEPROGRAMPROC, DeleteProgram) \
    X(PFNGLUSEPROGRAMPROC, UseProgram) \
    X(PFNGLGENVERTEXARRAYSPROC, GenVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC, BindVertexArray) \
    X(PFNGLDELETEVERTEXARRAYSPROC, DeleteVertexArrays) \
    X(PFNGLGENBUFFERSPROC, GenBuffers) \
    X(PFNGLBINDBUFFERPROC, BindBuffer) \
    X(PFNGLBUFFERDATAPROC, BufferData) \
    X(PFNGLDELETEBUFFERSPROC, DeleteBuffers) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, EnableVertexAttribArray) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, VertexAttribPointer) \
    X(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation) \
    X(PFNGLUNIFORM1FPROC, Uniform1f) \
    X(PFNGLUNIFORM2FPROC, Uniform2f) \
    X(PFNGLUNIFORM3FPROC, Uniform3f)

#define VX_DECLARE(type, name) static type p_##name;
VX_GL_FUNCTIONS(VX_DECLARE)
#undef VX_DECLARE

static const char *vertex_source =
    "#version 330 core\n"
    "layout(location=0) in vec2 a_position;\n"
    "layout(location=1) in vec2 a_local;\n"
    "layout(location=2) in vec2 a_properties;\n"
    "out vec2 v_local;\n"
    "flat out vec2 v_properties;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "    v_local = a_local; v_properties = a_properties;\n"
    "}\n";

static const char *fragment_source =
    "#version 330 core\n"
    "in vec2 v_local;\n"
    "flat in vec2 v_properties;\n"
    "uniform float u_beam_width_squared;\n"
    "uniform vec2 u_glow_radius_squared;\n"
    "uniform vec2 u_glow_strength;\n"
    "uniform vec3 u_beam_colour;\n"
    "uniform vec3 u_glow_colour;\n"
    "uniform float u_plain;\n"
    "out vec4 colour;\n"
    "void main() {\n"
    "    if (u_plain > 0.5) { colour = vec4(vec3(v_properties.y), 1.0); return; }\n"
    "    float end_distance = max(max(-v_local.x, v_local.x-v_properties.x), 0.0);\n"
    "    float d2 = dot(vec2(end_distance, v_local.y), vec2(end_distance, v_local.y));\n"
    "    float beam = exp(-d2 / u_beam_width_squared);\n"
    "    float halo = dot(u_glow_strength, exp(-vec2(d2) / u_glow_radius_squared));\n"
    "    vec3 phosphor = u_beam_colour * beam + u_glow_colour * halo;\n"
    "    colour = vec4(phosphor * v_properties.y, 1.0);\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *source) {
    const GLuint shader = p_CreateShader(type);
    p_ShaderSource(shader, 1, &source, NULL);
    p_CompileShader(shader);
    GLint okay = 0;
    p_GetShaderiv(shader, GL_COMPILE_STATUS, &okay);
    if (!okay) {
        char message[2048];
        p_GetShaderInfoLog(shader, sizeof(message), NULL, message);
        fprintf(stderr, "OpenGL shader compilation failed: %s\n", message);
        p_DeleteShader(shader);
        return 0;
    }
    return shader;
}

bool vx_gl_init(VxGlRenderer *renderer) {
    *renderer = (VxGlRenderer){0};
#define VX_LOAD(type, name) \
    p_##name = (type)SDL_GL_GetProcAddress("gl" #name); \
    if (!p_##name) { fprintf(stderr, "Missing OpenGL function gl%s\n", #name); return false; }
    VX_GL_FUNCTIONS(VX_LOAD)
#undef VX_LOAD
    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
    if (!vertex) return false;
    const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!fragment) { p_DeleteShader(vertex); return false; }
    renderer->program = p_CreateProgram();
    p_AttachShader(renderer->program, vertex);
    p_AttachShader(renderer->program, fragment);
    p_LinkProgram(renderer->program);
    p_DeleteShader(vertex);
    p_DeleteShader(fragment);
    GLint okay = 0;
    p_GetProgramiv(renderer->program, GL_LINK_STATUS, &okay);
    if (!okay) {
        char message[2048];
        p_GetProgramInfoLog(renderer->program, sizeof(message), NULL, message);
        fprintf(stderr, "OpenGL shader linking failed: %s\n", message);
        vx_gl_shutdown(renderer);
        return false;
    }
    renderer->beam_width_uniform = p_GetUniformLocation(renderer->program, "u_beam_width_squared");
    renderer->glow_radius_uniform = p_GetUniformLocation(renderer->program, "u_glow_radius_squared");
    renderer->glow_strength_uniform = p_GetUniformLocation(renderer->program, "u_glow_strength");
    renderer->beam_colour_uniform = p_GetUniformLocation(renderer->program, "u_beam_colour");
    renderer->glow_colour_uniform = p_GetUniformLocation(renderer->program, "u_glow_colour");
    renderer->plain_uniform = p_GetUniformLocation(renderer->program, "u_plain");
    stb_easy_font_spacing(0);
    p_GenVertexArrays(1, &renderer->vertex_array);
    p_GenBuffers(1, &renderer->vertex_buffer);
    p_BindVertexArray(renderer->vertex_array);
    p_BindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer);
    for (GLuint i = 0; i < 3; ++i) {
        p_EnableVertexAttribArray(i);
        p_VertexAttribPointer(i, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                              (const void *)(size_t)(i * 2 * sizeof(float)));
    }
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    printf("OpenGL %s | %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    return true;
}

void vx_gl_shutdown(VxGlRenderer *renderer) {
    if (renderer->vertex_buffer) p_DeleteBuffers(1, &renderer->vertex_buffer);
    if (renderer->vertex_array) p_DeleteVertexArrays(1, &renderer->vertex_array);
    if (renderer->program) p_DeleteProgram(renderer->program);
    *renderer = (VxGlRenderer){0};
}

typedef struct { float x, y, local_x, local_y, length, intensity; } VxGpuVertex;

static bool begin_frame(VxVec2 canvas, int pixel_width, int pixel_height) {
    glViewport(0, 0, pixel_width, pixel_height);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (pixel_width <= 0 || pixel_height <= 0) return false;
    const float scale = fminf((float)pixel_width / canvas.x, (float)pixel_height / canvas.y);
    const int width = (int)lroundf(canvas.x * scale), height = (int)lroundf(canvas.y * scale);
    glViewport((pixel_width - width) / 2, (pixel_height - height) / 2, width, height);
    return true;
}

void vx_gl_draw(VxGlRenderer *renderer, const VxVectorFrame *frame,
                int pixel_width, int pixel_height, const VxRenderSettings *settings) {
    const VxVec2 canvas = frame->canvas_size;
    if (!begin_frame(canvas, pixel_width, pixel_height)) return;

    const float beam_width = vx_clamp(settings->beam_width, 0.1f, 16.0f);
    const float inner_radius = vx_clamp(settings->inner_glow_radius, 0.1f, 64.0f);
    const float outer_radius = vx_clamp(settings->outer_glow_radius, 0.1f, 64.0f);
    const float padding = 3.0f * (settings->glow_enabled
        ? fmaxf(beam_width, fmaxf(inner_radius, outer_radius)) : beam_width);
    VxGpuVertex vertices[VX_MAX_LINES * 6];
    size_t count = 0;
    for (size_t i = 0; i < frame->count; ++i) {
        const VxLine line = frame->lines[i];
        const VxVec2 delta = vx_v2_sub(line.b, line.a);
        const float length = vx_v2_length(delta);
        const VxVec2 axis = length > 0.0001f ? vx_v2_scale(delta, 1.0f / length) : (VxVec2){1, 0};
        const VxVec2 normal = {-axis.y, axis.x};
        const float corners[4][2] = {
            {-padding, -padding}, {length + padding, -padding},
            {length + padding, padding}, {-padding, padding}
        };
        const int order[] = {0, 1, 2, 0, 2, 3};
        for (int k = 0; k < 6; ++k) {
            const float u = corners[order[k]][0], v = corners[order[k]][1];
            const VxVec2 position = vx_v2_add(line.a,
                vx_v2_add(vx_v2_scale(axis, u), vx_v2_scale(normal, v)));
            vertices[count++] = (VxGpuVertex){position.x * 2.0f / canvas.x - 1.0f,
                1.0f - position.y * 2.0f / canvas.y, u, v, length, line.intensity};
        }
    }
    p_UseProgram(renderer->program);
    p_Uniform1f(renderer->plain_uniform, 0);
    p_Uniform1f(renderer->beam_width_uniform, beam_width * beam_width);
    p_Uniform2f(renderer->glow_radius_uniform, inner_radius * inner_radius, outer_radius * outer_radius);
    p_Uniform2f(renderer->glow_strength_uniform,
        settings->glow_enabled ? vx_clamp(settings->inner_glow_strength, 0, 4) : 0,
        settings->glow_enabled ? vx_clamp(settings->outer_glow_strength, 0, 4) : 0);
    p_Uniform3f(renderer->beam_colour_uniform,
        settings->beam_colour.x, settings->beam_colour.y, settings->beam_colour.z);
    p_Uniform3f(renderer->glow_colour_uniform,
        settings->glow_colour.x, settings->glow_colour.y, settings->glow_colour.z);
    p_BindVertexArray(renderer->vertex_array);
    p_BindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer);
    p_BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * sizeof(VxGpuVertex)), vertices, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)count);
}

void vx_gl_draw_text(VxGlRenderer *renderer, VxVec2 canvas, const VxTextLine *lines,
                     size_t line_count, int pixel_width, int pixel_height) {
    if (!begin_frame(canvas, pixel_width, pixel_height)) return;
    p_UseProgram(renderer->program);
    p_Uniform1f(renderer->plain_uniform, 1);
    p_BindVertexArray(renderer->vertex_array);
    p_BindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer);
    typedef struct { float x, y, z; unsigned char colour[4]; } FontVertex;
    FontVertex glyphs[4096];
    VxGpuVertex vertices[1024 * 6];
    for (size_t line = 0; line < line_count; ++line) {
        char printable[256];
        size_t length = 0;
        for (; lines[line].text[length] && length < sizeof(printable) - 1; ++length) {
            const unsigned char c = (unsigned char)lines[line].text[length];
            printable[length] = c >= 32 && c <= 126 ? (char)c : '?';
        }
        printable[length] = 0;
        const int text_width = stb_easy_font_width(printable);
        const float scale = fminf(lines[line].scale, (canvas.x - 60) / fmaxf(1, (float)text_width));
        const float x = (canvas.x - (float)text_width * scale) * 0.5f;
        const float y = lines[line].y - (float)stb_easy_font_height(printable) * scale * 0.5f;
        const int quads = stb_easy_font_print(0, 0, printable, NULL, glyphs, sizeof(glyphs));
        const int order[] = {0, 1, 2, 0, 2, 3};
        size_t count = 0;
        for (int q = 0; q < quads; ++q) for (int k = 0; k < 6; ++k) {
            const FontVertex point = glyphs[q * 4 + order[k]];
            vertices[count++] = (VxGpuVertex){(x + point.x * scale) * 2 / canvas.x - 1,
                1 - (y + point.y * scale) * 2 / canvas.y, 0, 0, 0, lines[line].brightness};
        }
        p_BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * sizeof(VxGpuVertex)), vertices, GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)count);
    }
}

bool vx_gl_save_bmp(const char *path, int width, int height) {
    if (width <= 0 || height <= 0 || width > 16000 || height > 16000) return false;
    const size_t row_bytes = (size_t)width * 3;
    unsigned char *pixels = malloc(row_bytes * (size_t)height);
    SDL_Surface *surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGB24);
    if (!pixels || !surface) {
        free(pixels);
        SDL_DestroySurface(surface);
        return false;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    for (int y = 0; y < height; ++y)
        memcpy((unsigned char *)surface->pixels + (size_t)y * surface->pitch,
               pixels + (size_t)(height - 1 - y) * row_bytes, row_bytes);
    const bool okay = SDL_SaveBMP(surface, path);
    SDL_DestroySurface(surface);
    free(pixels);
    return okay;
}
