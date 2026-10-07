#include "game/scene.h"
#include "platform/gl_renderer.h"
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static VxInput keyboard_input(void) {
    const bool *keys = SDL_GetKeyboardState(NULL);
    const bool right = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
    const bool left = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
    const bool up = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
    const bool down = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
    return (VxInput){(float)right - (float)left, (float)up - (float)down};
}

static bool read_setting(const char *value, float minimum, float maximum, float *output) {
    char *end;
    const float parsed = strtof(value, &end);
    if (!*value || *end || !isfinite(parsed) || parsed < minimum || parsed > maximum) return false;
    *output = parsed;
    return true;
}

int main(int argc, char **argv) {
    int frame_limit = 0;
    bool demo = false;
    VxRenderSettings settings = vx_render_settings_default();
    const char *screenshot = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
            char *end;
            const long value = strtol(argv[++i], &end, 10);
            if (*end || value <= 0 || value > 100000) {
                fprintf(stderr, "--frames requires an integer from 1 to 100000\n");
                return EXIT_FAILURE;
            }
            frame_limit = (int)value;
        } else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) {
            screenshot = argv[++i];
        } else if (!strcmp(argv[i], "--demo")) demo = true;
        else if (!strcmp(argv[i], "--no-glow")) settings.glow_enabled = false;
        else if (!strcmp(argv[i], "--glow-strength") && i + 1 < argc) {
            float scale;
            if (!read_setting(argv[++i], 0, 4, &scale)) {
                fprintf(stderr, "--glow-strength requires a multiplier from 0 to 4\n");
                return EXIT_FAILURE;
            }
            const VxRenderSettings defaults = vx_render_settings_default();
            settings.inner_glow_strength = defaults.inner_glow_strength * scale;
            settings.outer_glow_strength = defaults.outer_glow_strength * scale;
        } else if (!strcmp(argv[i], "--glow-radius") && i + 1 < argc) {
            float scale;
            if (!read_setting(argv[++i], 0.1f, 8, &scale)) {
                fprintf(stderr, "--glow-radius requires a multiplier from 0.1 to 8\n");
                return EXIT_FAILURE;
            }
            const VxRenderSettings defaults = vx_render_settings_default();
            settings.inner_glow_radius = defaults.inner_glow_radius * scale;
            settings.outer_glow_radius = defaults.outer_glow_radius * scale;
        }
        else {
            printf("Usage: vectorx [--frames N] [--demo] [--no-glow] "
                   "[--glow-strength 0..4] [--glow-radius 0.1..8] [--screenshot file.bmp]\n");
            return !strcmp(argv[i], "--help") ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    }
    if (screenshot && !frame_limit) frame_limit = 1;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
#ifdef __APPLE__
    /* Apple's core profiles jump from 3.2 to 4.1; GLSL 330 needs the latter. */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (frame_limit) flags |= SDL_WINDOW_HIDDEN;
    SDL_Window *window = SDL_CreateWindow("VectorX", VX_WIDTH, VX_HEIGHT, flags);
    if (!window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }
    SDL_SetWindowMinimumSize(window, 300, 400);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        fprintf(stderr, "An OpenGL 3.3 context is required: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }
    VxGlRenderer renderer;
    if (!vx_gl_init(&renderer)) {
        SDL_GL_DestroyContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }
    const bool vsync = SDL_GL_SetSwapInterval(frame_limit ? 0 : 1);
    VxFlight flight;
    vx_flight_reset(&flight);
    VxWeapons weapons;
    vx_weapons_reset(&weapons);
    VxVectorFrame frame;
    vx_frame_init(&frame, VX_MAX_LINES);
    bool running = true, paused = false;
    int rendered_frames = 0, result = EXIT_SUCCESS;
    Uint64 last = SDL_GetPerformanceCounter();
    const double frequency = (double)SDL_GetPerformanceFrequency();
    double accumulator = 0.0, simulation_time = 0.0;

    while (running) {
        const Uint64 now = SDL_GetPerformanceCounter();
        double elapsed = (double)(now - last) / frequency;
        last = now;
        /* Bounded catch-up after a breakpoint, resize or long scheduling stall. */
        elapsed = fmin(elapsed, 0.1);
        if (frame_limit) elapsed = 1.0 / 60.0;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                switch (event.key.scancode) {
                case SDL_SCANCODE_ESCAPE: running = false; break;
                case SDL_SCANCODE_P: paused = !paused; accumulator = 0; break;
                case SDL_SCANCODE_G: settings.glow_enabled = !settings.glow_enabled; break;
                case SDL_SCANCODE_F:
                    if (!SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)))
                        fprintf(stderr, "Fullscreen change failed: %s\n", SDL_GetError());
                    break;
                case SDL_SCANCODE_R:
                    vx_flight_reset(&flight); vx_weapons_reset(&weapons); accumulator = 0;
                    break;
                default: break;
                }
            }
        }
        if (!running) break;
        const bool focused = frame_limit || (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS);
        if (!paused && focused) {
            accumulator += elapsed;
            while (accumulator >= VX_FIXED_STEP) {
                VxInput input = keyboard_input();
                bool firing = SDL_GetKeyboardState(NULL)[SDL_SCANCODE_SPACE];
                if (demo) {
                    const int phase = (int)(simulation_time / 0.75) % 4;
                    const VxInput directions[] = {{1, 1}, {-1, 0}, {0, -1}, {0, 0}};
                    input = directions[phase];
                    firing = true;
                }
                vx_flight_update(&flight, input, (float)VX_FIXED_STEP);
                vx_weapons_update(&weapons, &flight, firing, (float)VX_FIXED_STEP);
                simulation_time += VX_FIXED_STEP;
                accumulator -= VX_FIXED_STEP;
            }
        } else accumulator = 0;

        vx_build_scene(&frame, &flight, &weapons);
        int width = 0, height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        vx_gl_draw(&renderer, &frame, width, height, &settings);
        ++rendered_frames;
        if (frame_limit && rendered_frames >= frame_limit) {
            if (screenshot && !vx_gl_save_bmp(screenshot, width, height)) {
                fprintf(stderr, "Screenshot failed: %s\n", SDL_GetError());
                result = EXIT_FAILURE;
            }
            running = false;
        }
        if (!SDL_GL_SwapWindow(window)) {
            fprintf(stderr, "Present failed: %s\n", SDL_GetError());
            result = EXIT_FAILURE;
            break;
        }
        if (!frame_limit && !vsync) {
            const double frame_seconds = (double)(SDL_GetPerformanceCounter() - now) / frequency;
            if (frame_seconds < 1.0 / 60.0)
                SDL_Delay((Uint32)((1.0 / 60.0 - frame_seconds) * 1000.0));
        }
        if (!frame_limit && (paused || !focused)) SDL_Delay(16);
    }
    printf("Frames: %d | vectors: %zu/%zu | clipped: %zu | dropped: %zu | X: %.3f Y: %.3f\n",
           rendered_frames, frame.count, frame.budget, frame.stats.clipped, frame.stats.dropped,
           flight.position.x, flight.position.y);
    vx_gl_shutdown(&renderer);
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
