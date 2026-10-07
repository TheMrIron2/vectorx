#define SDL_MAIN_HANDLED
#include "platform/music.h"
#include "platform/track_menu.h"
#include <SDL3/SDL_main.h>
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Music check failed at line %d: %s (%s)\n", __LINE__, #condition, SDL_GetError()); \
    exit(EXIT_FAILURE); } } while (0)

static int sample_offset(VxMusic *music) {
    SDL_LockAudioStream(music->stream);
    const int offset = stb_vorbis_get_sample_offset(music->decoder);
    SDL_UnlockAudioStream(music->stream);
    return offset;
}

int main(void) {
    SDL_SetMainReady();
    CHECK(SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy"));
    VxTrackMenu menu;
    CHECK(vx_track_menu_open(&menu) && menu.count >= 2);
    const VxVec2 canvas = vx_view_size(VX_VIEW_WIDE);
    CHECK(vx_track_menu_lines(&menu, canvas) == (size_t)menu.count + 2);
    CHECK(SDL_strcmp(menu.lines[1].text, menu.tracks[0]) == 0);
    SDL_Event event = {0};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_UP;
    CHECK(!vx_track_menu_event(&menu, &event, NULL, canvas) && menu.selected == menu.count - 1);
    event.key.scancode = SDL_SCANCODE_DOWN;
    CHECK(!vx_track_menu_event(&menu, &event, NULL, canvas) && menu.selected == 0);
    event.key.repeat = true;
    CHECK(!vx_track_menu_event(&menu, &event, NULL, canvas) && menu.selected == 0);
    event.key.repeat = false;
    event.key.scancode = SDL_SCANCODE_RETURN;
    CHECK(vx_track_menu_event(&menu, &event, NULL, canvas));
    CHECK(SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy"));
    CHECK(SDL_InitSubSystem(SDL_INIT_VIDEO));
    SDL_Window *window = SDL_CreateWindow("Track menu tests", 1280, 720, SDL_WINDOW_HIDDEN);
    CHECK(window);
    event = (SDL_Event){0};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 640;
    event.button.y = menu.lines[2].y * 720 / canvas.y;
    CHECK(vx_track_menu_event(&menu, &event, window, canvas) && menu.selected == 1);
    event.button.x = 5; event.button.y = 5;
    CHECK(!vx_track_menu_event(&menu, &event, window, canvas));
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    for (int track = 0; track < menu.count; ++track) {
        menu.selected = track;
        char *track_path = vx_track_menu_path(&menu);
        CHECK(track_path);
        VxMusic music;
        CHECK(vx_music_open(&music, track_path, 0.5f));
        SDL_free(track_path);
        CHECK(!music.playing && music.stream && music.decoder);
        CHECK(fabsf(SDL_GetAudioStreamGain(music.stream) - 0.5f) < 0.001f);
        const unsigned int expected = stb_vorbis_stream_length_in_samples(music.decoder);
        unsigned long long decoded = 0;
        double energy = 0;
        float samples[8192];
        int frames;
        while ((frames = stb_vorbis_get_samples_float_interleaved(music.decoder, music.channels, samples, 8192)) > 0) {
            decoded += (unsigned int)frames;
            for (int i = 0; i < frames * music.channels; ++i) {
                CHECK(isfinite(samples[i]));
                energy += (double)samples[i] * samples[i];
            }
        }
        CHECK(decoded == expected && energy > 1);
        CHECK(stb_vorbis_get_error(music.decoder) == VORBIS__no_error);
        CHECK(expected > 64 && stb_vorbis_seek(music.decoder, expected - 64));
        CHECK(SDL_GetAudioStreamData(music.stream, samples, sizeof(samples)) > 0);
        CHECK(sample_offset(&music) < (int)expected - 64);
        vx_music_set_playing(&music, true);
        CHECK(music.playing);
        SDL_Delay(80);
        vx_music_set_playing(&music, false);
        CHECK(!music.playing);
        const int paused_offset = sample_offset(&music);
        SDL_Delay(50);
        CHECK(sample_offset(&music) == paused_offset);
        vx_music_set_playing(&music, true);
        SDL_Delay(80);
        CHECK(sample_offset(&music) != paused_offset);
        vx_music_close(&music);
        CHECK(!music.stream && !music.decoder && !music.audio_initialized);
        vx_music_close(&music);
        printf("Music checks passed: %llu decoded frames, looping, gain, pause/resume and cleanup.\n", decoded);
    }
    vx_track_menu_close(&menu);
    CHECK(!menu.tracks && !menu.lines && menu.count == 0);
    SDL_Quit();
    return EXIT_SUCCESS;
}
