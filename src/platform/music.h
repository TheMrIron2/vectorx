#ifndef VX_MUSIC_H
#define VX_MUSIC_H

#include <SDL3/SDL.h>

typedef struct {
    struct stb_vorbis *decoder;
    SDL_AudioStream *stream;
    int channels;
    bool playing, audio_initialized;
    float samples[4096 * 2];
} VxMusic;

/* Paths are relative to the executable's data directory, not the working directory. */
bool vx_music_open(VxMusic *music, const char *track, float volume);
void vx_music_set_playing(VxMusic *music, bool playing);
void vx_music_close(VxMusic *music);

#endif
