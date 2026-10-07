#define SDL_MAIN_HANDLED
#include "platform/music.h"
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
    VxMusic music;
    CHECK(vx_music_open(&music, "bgm/Overdriven Purpose.ogg", 0.5f));
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
    return EXIT_SUCCESS;
}
