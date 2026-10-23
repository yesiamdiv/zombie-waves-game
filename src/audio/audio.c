#include "audio/audio.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "assets/asset_manager.h"
#include "audio/audio_mixer.h"
#include "core/log.h"

/* One backend, one rate: sounds are converted to mono float at this rate when
 * loaded, so the audio thread only ever copies samples. 48 kHz matches the
 * device format on this machine; SDL converts if a device wants something
 * else, which is a load-free operation inside the stream. */
#define AUDIO_RATE 48000
#define AUDIO_CHANNELS 2

/* Largest block the mixer renders per callback. The device may ask for more;
 * supplying less is legal and it will ask again. */
#define AUDIO_SCRATCH_FRAMES 2048

/* World units at which a sound has faded to silence. Tuned by ear in Phase E;
 * the value only changes loudness, never the simulation. */
#define AUDIO_FALLOFF_RADIUS 24.0f

/* Slot -> file under assets/, filled by Phase C (tools/gen_audio.py). Every
 * NULL is a slot that is deliberately silent for now: no file is opened and
 * no warning is logged, so the foundations phase runs clean before any asset
 * exists. */
static const char *const SFX_PATHS[AUDIO_SFX_COUNT] = {
    [AUDIO_SFX_NONE] = NULL,
    [AUDIO_SFX_SHOT_BULLET] = NULL,
    [AUDIO_SFX_SHOT_GRENADE] = NULL,
    [AUDIO_SFX_SHOT_ROCKET] = NULL,
    [AUDIO_SFX_HIT_ZOMBIE] = NULL,
    [AUDIO_SFX_HIT_PLAYER] = NULL,
    [AUDIO_SFX_EXPLOSION] = NULL,
    [AUDIO_SFX_DEATH_ZOMBIE] = NULL,
    [AUDIO_SFX_DEATH_PLAYER] = NULL,
    [AUDIO_SFX_WAVE_START] = NULL,
    [AUDIO_SFX_WAVE_END] = NULL,
    [AUDIO_SFX_PLAYER_HURT] = NULL,
    [AUDIO_SFX_PICKUP] = NULL,
    [AUDIO_SFX_POINTS] = NULL,
};

static const char *const SFX_NAMES[AUDIO_SFX_COUNT] = {
    [AUDIO_SFX_NONE] = "none",
    [AUDIO_SFX_SHOT_BULLET] = "shot_bullet",
    [AUDIO_SFX_SHOT_GRENADE] = "shot_grenade",
    [AUDIO_SFX_SHOT_ROCKET] = "shot_rocket",
    [AUDIO_SFX_HIT_ZOMBIE] = "hit_zombie",
    [AUDIO_SFX_HIT_PLAYER] = "hit_player",
    [AUDIO_SFX_EXPLOSION] = "explosion",
    [AUDIO_SFX_DEATH_ZOMBIE] = "death_zombie",
    [AUDIO_SFX_DEATH_PLAYER] = "death_player",
    [AUDIO_SFX_WAVE_START] = "wave_start",
    [AUDIO_SFX_WAVE_END] = "wave_end",
    [AUDIO_SFX_PLAYER_HURT] = "player_hurt",
    [AUDIO_SFX_PICKUP] = "pickup",
    [AUDIO_SFX_POINTS] = "points",
};

typedef struct {
    AudioClip clip;
    float *data; /* owned, mono @ AUDIO_RATE */
} AudioSlot;

typedef struct {
    SDL_AudioDeviceID dev;
    SDL_AudioStream *stream;
    SDL_Mutex *lock; /* guards the mixer (game thread <-> audio thread) */
    AudioMixer mixer;
    float scratch[AUDIO_SCRATCH_FRAMES * AUDIO_CHANNELS];
    AudioSlot slots[AUDIO_SFX_COUNT];
    float *test_data;
    AudioClip test_clip;
    Uint64 last_sid;
    bool device_open;
} AudioSystem;

static AudioSystem *g_audio = NULL;

const char *audio_sfx_name(AudioSfx sfx) {
    if (sfx < 0 || sfx >= AUDIO_SFX_COUNT) return "?";
    return SFX_NAMES[sfx];
}

/* ---------------------------------------------------------------- decoding */

static float wav_mono(const SDL_AudioSpec *spec, const Uint8 *buf, int frame) {
    int bytes = SDL_AUDIO_BYTESIZE(spec->format);
    float sum = 0.0f;
    for (int c = 0; c < spec->channels; c++) {
        const Uint8 *p = buf + ((size_t)frame * (size_t)spec->channels +
                                (size_t)c) * (size_t)bytes;
        switch (spec->format) {
        case SDL_AUDIO_U8:
            sum += ((float)p[0] - 128.0f) / 128.0f;
            break;
        case SDL_AUDIO_S16: {
            Sint16 v = 0;
            memcpy(&v, p, sizeof(v));
            sum += (float)v / 32768.0f;
            break;
        }
        case SDL_AUDIO_S32: {
            Sint32 v = 0;
            memcpy(&v, p, sizeof(v));
            sum += (float)((double)v / 2147483648.0);
            break;
        }
        case SDL_AUDIO_F32: {
            float v = 0.0f;
            memcpy(&v, p, sizeof(v));
            sum += v;
            break;
        }
        default:
            break;
        }
    }
    return sum / (float)spec->channels;
}

/* Load one slot: decode to mono float at AUDIO_RATE once, at init, so the
 * audio thread never converts. Missing or unreadable files are silence. */
static bool audio_load_clip(AudioSystem *sys, AudioSfx sfx, const char *rel) {
    SDL_AudioSpec spec;
    Uint8 *wav = NULL;
    Uint32 len = 0;
    const char *path = asset_path(rel);
    if (!SDL_LoadWAV(path, &spec, &wav, &len)) {
        LOG_WARN("Audio: '%s' not loaded (%s) - slot stays silent", rel,
                 SDL_GetError());
        return false;
    }

    int bytes = SDL_AUDIO_BYTESIZE(spec.format);
    int src_frames = (bytes > 0 && spec.channels > 0)
                         ? (int)(len / (Uint32)(bytes * spec.channels))
                         : 0;
    if (src_frames <= 0 || spec.freq <= 0) {
        SDL_free(wav);
        LOG_WARN("Audio: '%s' has no usable samples - slot stays silent", rel);
        return false;
    }

    int dst_frames = (int)(((long long)src_frames * AUDIO_RATE) / spec.freq);
    if (dst_frames <= 0) dst_frames = 1;

    float *data = (float *)SDL_malloc((size_t)dst_frames * sizeof(float));
    if (!data) {
        SDL_free(wav);
        return false;
    }

    for (int i = 0; i < dst_frames; i++) {
        double src_pos = (double)i * (double)spec.freq / (double)AUDIO_RATE;
        int i0 = (int)src_pos;
        float frac = (float)(src_pos - (double)i0);
        if (i0 >= src_frames) i0 = src_frames - 1;
        int i1 = (i0 + 1 < src_frames) ? i0 + 1 : i0;
        float a = wav_mono(&spec, wav, i0);
        float b = wav_mono(&spec, wav, i1);
        data[i] = a + (b - a) * frac;
    }

    SDL_free(wav);
    sys->slots[sfx].data = data;
    sys->slots[sfx].clip = (AudioClip){data, dst_frames};
    LOG_INFO("Audio: '%s' loaded (%d frames @ %d Hz)", rel, dst_frames,
             AUDIO_RATE);
    return true;
}

static void audio_load_slots(AudioSystem *sys) {
    for (int i = 1; i < AUDIO_SFX_COUNT; i++) {
        if (!SFX_PATHS[i]) continue;
        (void)audio_load_clip(sys, (AudioSfx)i, SFX_PATHS[i]);
    }
}

/* ------------------------------------------------------------- device path */

static void audio_queue(AudioSystem *sys, const AudioClip *clip, float gain,
                        float pan) {
    if (!sys || !clip || !clip->samples || !sys->device_open) return;
    if (!sys->lock) return;
    SDL_LockMutex(sys->lock);
    bool queued = audio_mixer_queue(&sys->mixer, clip, gain, pan);
    SDL_UnlockMutex(sys->lock);
    if (!queued) LOG_DEBUG("Audio: trigger ring full - dropped a sound");
}

/* SDL asks for more input data while the device drains the stream. Mix here,
 * on the audio thread: game hitches cannot starve playback because the voices
 * keep reading their samples. */
static void SDLCALL audio_get_cb(void *userdata, SDL_AudioStream *stream,
                                 int additional_amount, int total_amount) {
    (void)total_amount;
    AudioSystem *sys = (AudioSystem *)userdata;
    if (!sys || !sys->lock) return;

    int bytes_per_frame = (int)(sizeof(float) * AUDIO_CHANNELS);
    int frames = additional_amount / bytes_per_frame;
    if (frames <= 0) return;
    if (frames > AUDIO_SCRATCH_FRAMES) frames = AUDIO_SCRATCH_FRAMES;

    SDL_LockMutex(sys->lock);
    audio_mixer_render(&sys->mixer, sys->scratch, frames);
    SDL_UnlockMutex(sys->lock);

    SDL_PutAudioStreamData(stream, sys->scratch, frames * bytes_per_frame);
}

static bool audio_open_device(AudioSystem *sys) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;

    SDL_AudioSpec want;
    SDL_zero(want);
    want.format = SDL_AUDIO_F32;
    want.channels = AUDIO_CHANNELS;
    want.freq = AUDIO_RATE;

    sys->dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want);
    if (!sys->dev) return false;

    SDL_AudioSpec got;
    SDL_zero(got);
    if (!SDL_GetAudioDeviceFormat(sys->dev, &got, NULL)) return false;

    sys->stream = SDL_CreateAudioStream(&want, &got);
    if (!sys->stream) return false;
    if (!SDL_SetAudioStreamGetCallback(sys->stream, audio_get_cb, sys)) {
        return false;
    }
    if (!SDL_BindAudioStream(sys->dev, sys->stream)) return false;
    if (!SDL_ResumeAudioStreamDevice(sys->stream)) return false;

    LOG_INFO("Audio: device open (driver=%s, %s %d ch %d Hz)",
             SDL_GetCurrentAudioDriver(), SDL_GetAudioFormatName(got.format),
             got.channels, got.freq);
    sys->device_open = true;
    return true;
}

static void audio_close_device(AudioSystem *sys) {
    if (!sys) return;
    /* Destroying the stream first stops the callback, so nothing can touch the
     * mixer while the rest is torn down. */
    if (sys->stream) {
        SDL_DestroyAudioStream(sys->stream);
        sys->stream = NULL;
    }
    if (sys->dev) {
        SDL_CloseAudioDevice(sys->dev);
        sys->dev = 0;
    }
    if (SDL_WasInit(SDL_INIT_AUDIO)) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    sys->device_open = false;
}

/* A known tone through the whole path, for the on-machine check that proves
 * the device works before judging a mix (--audio-test). */
static void audio_play_test_cue(AudioSystem *sys) {
    const int ms = 150;
    const int fade = AUDIO_RATE * 5 / 1000;
    int frames = AUDIO_RATE * ms / 1000;
    float *data = (float *)SDL_malloc((size_t)frames * sizeof(float));
    if (!data) return;

    for (int i = 0; i < frames; i++) {
        float env = 1.0f;
        if (i < fade) env = (float)i / (float)fade;
        else if (i >= frames - fade) env = (float)(frames - 1 - i) / (float)fade;
        data[i] = 0.3f * env * SDL_sinf(6.2831853f * 1000.0f * (float)i /
                                        (float)AUDIO_RATE);
    }

    sys->test_data = data;
    sys->test_clip = (AudioClip){data, frames};
    audio_queue(sys, &sys->test_clip, 1.0f, 0.0f);
    LOG_INFO("Audio: test cue queued (1 kHz, %d ms)", ms);
}

/* ----------------------------------------------------------------- lifecycle */

bool audio_init(bool headless, bool test_cue) {
    if (g_audio) return true;

    /* Headless is the gate's normal condition: no device, no allocation, no
     * per-frame work. Returning false here is what makes every later call a
     * no-op. */
    if (headless) {
        LOG_DEBUG("Audio: headless - device never opened");
        return false;
    }

    AudioSystem *sys = (AudioSystem *)SDL_calloc(1, sizeof(*sys));
    if (!sys) {
        LOG_WARN("Audio: disabled (out of memory)");
        return false;
    }
    audio_mixer_init(&sys->mixer, AUDIO_RATE, AUDIO_CHANNELS);

    sys->lock = SDL_CreateMutex();
    if (!sys->lock) {
        LOG_WARN("Audio: disabled (%s)", SDL_GetError());
        SDL_free(sys);
        return false;
    }

    if (!audio_open_device(sys)) {
        LOG_WARN("Audio: disabled (%s) - the game continues silently",
                 SDL_GetError());
        audio_close_device(sys);
        SDL_DestroyMutex(sys->lock);
        SDL_free(sys);
        return false;
    }

    g_audio = sys;
    audio_load_slots(sys);
    if (test_cue) audio_play_test_cue(sys);
    return true;
}

void audio_shutdown(void) {
    AudioSystem *sys = g_audio;
    if (!sys) return;
    g_audio = NULL;

    audio_close_device(sys);
    if (sys->lock) SDL_DestroyMutex(sys->lock);
    for (int i = 0; i < AUDIO_SFX_COUNT; i++) SDL_free(sys->slots[i].data);
    SDL_free(sys->test_data);
    SDL_free(sys);
    LOG_INFO("Audio: shut down");
}

bool audio_available(void) {
    return g_audio != NULL && g_audio->device_open;
}

/* --------------------------------------------------------------- triggers */

/* Phase D fills this from the relayed bus (audio-plan §7): the slot per event
 * is the trigger audit's table. Phase B ships the plumbing and the mixer with
 * no mapping, so every event maps to silence. */
static AudioSfx audio_sfx_for_event(const GameEvent *ev) {
    (void)ev;
    return AUDIO_SFX_NONE;
}

void audio_tick(EventBus *events, float listener_x, float listener_y) {
    AudioSystem *sys = g_audio;
    if (!sys || !sys->device_open || !events) return;

    /* Same watermark walk as relay_net_events(): the ring is write-only during
     * gameplay and this runs before event_bus_flush() drains it. */
    int count = events->count;
    for (int i = 0; i < count; i++) {
        const GameEvent *ev = &events->ring[(events->head + i) % EV_MAX_EVENTS];
        if (ev->sid <= sys->last_sid) continue;
        sys->last_sid = ev->sid;

        AudioSfx sfx = audio_sfx_for_event(ev);
        if (sfx == AUDIO_SFX_NONE) continue;

        const AudioClip *clip = &sys->slots[sfx].clip;
        if (!clip->samples) continue;

        float gain = 1.0f;
        float pan = 0.0f;
        audio_mix_spatialise(ev->x, ev->y, listener_x, listener_y,
                             AUDIO_FALLOFF_RADIUS, &gain, &pan);
        audio_queue(sys, clip, gain, pan);
    }
}
