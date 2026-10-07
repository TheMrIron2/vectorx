#include "platform/track_menu.h"
#include <stdio.h>
#include <stdlib.h>

static int compare_tracks(const void *a, const void *b) {
    return SDL_strcasecmp(*(const char *const *)a, *(const char *const *)b);
}

bool vx_track_menu_open(VxTrackMenu *menu) {
    *menu = (VxTrackMenu){0};
    const char *base = SDL_GetBasePath();
    char *directory = NULL;
    if (!base || SDL_asprintf(&directory, "%sbgm", base) < 0) return false;
    menu->tracks = SDL_GlobDirectory(directory, "*.ogg", SDL_GLOB_CASEINSENSITIVE, &menu->count);
    SDL_free(directory);
    if (!menu->tracks || menu->count == 0) {
        fprintf(stderr, "No OGG tracks found in bgm: %s\n", SDL_GetError());
        vx_track_menu_close(menu);
        return false;
    }
    qsort(menu->tracks, (size_t)menu->count, sizeof(*menu->tracks), compare_tracks);
    menu->lines = SDL_calloc((size_t)menu->count + 2, sizeof(*menu->lines));
    if (!menu->lines) { vx_track_menu_close(menu); return false; }
    return true;
}

void vx_track_menu_close(VxTrackMenu *menu) {
    SDL_free(menu->tracks);
    SDL_free(menu->lines);
    *menu = (VxTrackMenu){0};
}

static float row_spacing(const VxTrackMenu *menu, VxVec2 canvas) {
    return fminf(52, canvas.y * 0.45f / (float)menu->count);
}

size_t vx_track_menu_lines(VxTrackMenu *menu, VxVec2 canvas) {
    if (menu->count == 0) return 0;
    const float spacing = row_spacing(menu, canvas);
    const float first = canvas.y * 0.5f - (float)(menu->count - 1) * spacing * 0.5f;
    menu->lines[0] = (VxTextLine){"Which track would you like to play?", first - 82, 3, 1};
    for (int i = 0; i < menu->count; ++i)
        menu->lines[i + 1] = (VxTextLine){menu->tracks[i], first + (float)i * spacing,
            fminf(3, spacing / 16), i == menu->selected ? 1.0f : 0.55f};
    menu->lines[menu->count + 1] = (VxTextLine){"Click a track, or use Up/Down and Enter",
        first + (float)(menu->count - 1) * spacing + 82, 2, 0.65f};
    return (size_t)menu->count + 2;
}

bool vx_track_menu_event(VxTrackMenu *menu, const SDL_Event *event, SDL_Window *window, VxVec2 canvas) {
    if (menu->count == 0) return false;
    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        switch (event->key.scancode) {
        case SDL_SCANCODE_UP: case SDL_SCANCODE_W:
            menu->selected = (menu->selected + menu->count - 1) % menu->count; break;
        case SDL_SCANCODE_DOWN: case SDL_SCANCODE_S:
            menu->selected = (menu->selected + 1) % menu->count; break;
        case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: return true;
        default: break;
        }
    }
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT) {
        int width, height;
        if (!SDL_GetWindowSize(window, &width, &height) || width <= 0 || height <= 0) return false;
        const float scale = fminf((float)width / canvas.x, (float)height / canvas.y);
        const float x = (event->button.x - ((float)width - canvas.x * scale) * 0.5f) / scale;
        const float y = (event->button.y - ((float)height - canvas.y * scale) * 0.5f) / scale;
        vx_track_menu_lines(menu, canvas);
        for (int i = 0; i < menu->count; ++i) {
            const float half_width = fminf(canvas.x * 0.45f,
                (float)SDL_strlen(menu->tracks[i]) * menu->lines[i + 1].scale * 3.5f);
            if (fabsf(x - canvas.x * 0.5f) <= half_width &&
                fabsf(y - menu->lines[i + 1].y) <= row_spacing(menu, canvas) * 0.4f) {
                menu->selected = i;
                return true;
            }
        }
    }
    return false;
}

char *vx_track_menu_path(const VxTrackMenu *menu) {
    char *path = NULL;
    if (menu->count && SDL_asprintf(&path, "bgm/%s", menu->tracks[menu->selected]) >= 0) return path;
    return NULL;
}
