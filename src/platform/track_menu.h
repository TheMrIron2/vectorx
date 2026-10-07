#ifndef VX_TRACK_MENU_H
#define VX_TRACK_MENU_H

#include "platform/gl_renderer.h"

typedef struct {
    char **tracks;
    VxTextLine *lines;
    int count, selected;
} VxTrackMenu;

bool vx_track_menu_open(VxTrackMenu *menu);
void vx_track_menu_close(VxTrackMenu *menu);
bool vx_track_menu_event(VxTrackMenu *menu, const SDL_Event *event, SDL_Window *window, VxVec2 canvas);
size_t vx_track_menu_lines(VxTrackMenu *menu, VxVec2 canvas);
char *vx_track_menu_path(const VxTrackMenu *menu);

#endif
