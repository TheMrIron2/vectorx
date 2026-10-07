#include "platform/music.h"
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#include <stdio.h>

static void SDLCALL fill_music(void *userdata, SDL_AudioStream *stream, int additional, int total) {
    VxMusic *music = userdata;
    (void)total;
    const int frame_bytes = music->channels * (int)sizeof(float);
    bool rewound = false;
    while (additional > 0) {
        const int wanted = SDL_min(4096, (additional + frame_bytes - 1) / frame_bytes);
        const int frames = stb_vorbis_get_samples_float_interleaved(music->decoder, music->channels,
            music->samples, wanted * music->channels);
        if (frames == 0) {
            if (rewound || stb_vorbis_get_error(music->decoder) != VORBIS__no_error ||
                !stb_vorbis_seek_start(music->decoder)) {
                SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Unable to continue music playback");
                return;
            }
            rewound = true;
            continue;
        }
        rewound = false;
        const int bytes = frames * frame_bytes;
        if (!SDL_PutAudioStreamData(stream, music->samples, bytes)) return;
        additional -= bytes;
    }
}

bool vx_music_open(VxMusic *music, const char *track, float volume) {
    *music = (VxMusic){0};
    const char *base = SDL_GetBasePath();
    char *path = NULL;
    if (!base || SDL_asprintf(&path, "%s%s", base, track) < 0) {
        fprintf(stderr, "Music path unavailable: %s\n", SDL_GetError());
        return false;
    }
    int error = 0;
    music->decoder = stb_vorbis_open_filename(path, &error, NULL);
    SDL_free(path);
    if (!music->decoder) {
        fprintf(stderr, "Cannot decode music %s (Vorbis error %d)\n", track, error);
        return false;
    }
    const stb_vorbis_info info = stb_vorbis_get_info(music->decoder);
    if (info.channels < 1 || info.channels > 2 || info.sample_rate == 0 ||
        stb_vorbis_stream_length_in_samples(music->decoder) == 0) {
        fprintf(stderr, "Music must be a non-empty mono or stereo Vorbis track: %s\n", track);
        vx_music_close(music);
        return false;
    }
    const float duration = stb_vorbis_stream_length_in_seconds(music->decoder);
    music->channels = info.channels;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        fprintf(stderr, "Audio initialization failed: %s\n", SDL_GetError());
        vx_music_close(music);
        return false;
    }
    music->audio_initialized = true;
    const SDL_AudioSpec spec = {.format = SDL_AUDIO_F32, .channels = info.channels, .freq = (int)info.sample_rate};
    music->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, fill_music, music);
    if (!music->stream || !SDL_SetAudioStreamGain(music->stream, SDL_clamp(volume, 0.0f, 1.0f))) {
        fprintf(stderr, "Music playback unavailable: %s\n", SDL_GetError());
        vx_music_close(music);
        return false;
    }
    printf("Music: %s | %u Hz | %d channels | %.1f seconds\n", track, info.sample_rate, info.channels, duration);
    return true;
}

void vx_music_set_playing(VxMusic *music, bool playing) {
    if (!music->stream || playing == music->playing) return;
    const bool okay = playing ? SDL_ResumeAudioStreamDevice(music->stream) : SDL_PauseAudioStreamDevice(music->stream);
    if (okay) music->playing = playing;
    else fprintf(stderr, "Music pause/resume failed: %s\n", SDL_GetError());
}

void vx_music_close(VxMusic *music) {
    /* Stop and join the audio callback before releasing its decoder/buffer. */
    if (music->stream) SDL_DestroyAudioStream(music->stream);
    if (music->decoder) stb_vorbis_close(music->decoder);
    if (music->audio_initialized) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    *music = (VxMusic){0};
}
