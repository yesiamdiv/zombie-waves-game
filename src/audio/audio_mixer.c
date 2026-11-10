#include "audio/audio_mixer.h"

#include <math.h>
#include <string.h>

/* Pan law: linear, equal in both channels at the centre. Cheap and
 * predictable, which is all a 2D top-down game needs; a constant-power law
 * would only matter if the listener rotated. */
static void voice_apply_pan(AudioVoice *v, float pan) {
    if (pan < -1.0f) pan = -1.0f;
    if (pan > 1.0f) pan = 1.0f;
    if (pan <= 0.0f) {
        v->pan_l = v->gain;
        v->pan_r = v->gain * (1.0f + pan);
    } else {
        v->pan_l = v->gain * (1.0f - pan);
        v->pan_r = v->gain;
    }
}

void audio_mixer_init(AudioMixer *m, int rate, int channels) {
    if (!m) return;
    memset(m, 0, sizeof(*m));
    m->rate = rate > 0 ? rate : 48000;
    m->channels = channels == 1 ? 1 : 2;
}

static AudioVoice *find_voice(AudioMixer *m) {
    for (int i = 0; i < AUDIO_MIX_MAX_VOICES; i++) {
        if (!m->voices[i].clip) return &m->voices[i];
    }
    /* Pool exhausted: replace the voice closest to finishing, so a new sound
     * never cuts one that just started. */
    AudioVoice *oldest = &m->voices[0];
    for (int i = 1; i < AUDIO_MIX_MAX_VOICES; i++) {
        if (m->voices[i].remaining < oldest->remaining) oldest = &m->voices[i];
    }
    return oldest;
}

static void start_voice(AudioMixer *m, const AudioClip *clip, float gain,
                        float pan) {
    AudioVoice *v = find_voice(m);
    v->clip = clip;
    v->cursor = clip->samples;
    v->gain = gain;
    v->remaining = clip->frames;
    voice_apply_pan(v, pan);
}

bool audio_mixer_queue(AudioMixer *m, const AudioClip *clip, float gain,
                       float pan) {
    if (!m || !clip || !clip->samples || clip->frames <= 0) return false;
    if (m->pending_count >= AUDIO_MIX_MAX_PENDING) return false;

    int slot = (m->pending_head + m->pending_count) % AUDIO_MIX_MAX_PENDING;
    m->pending[slot] = (AudioRequest){clip, gain, pan};
    m->pending_count++;
    return true;
}

int audio_mixer_active(const AudioMixer *m) {
    if (!m) return 0;
    int n = 0;
    for (int i = 0; i < AUDIO_MIX_MAX_VOICES; i++) {
        if (m->voices[i].clip) n++;
    }
    return n;
}

void audio_mixer_render(AudioMixer *m, float *out, int frames) {
    if (!m || !out || frames <= 0) return;

    int channels = m->channels;
    memset(out, 0, (size_t)frames * (size_t)channels * sizeof(float));

    while (m->pending_count > 0) {
        AudioRequest *r = &m->pending[m->pending_head];
        start_voice(m, r->clip, r->gain, r->pan);
        m->pending_head = (m->pending_head + 1) % AUDIO_MIX_MAX_PENDING;
        m->pending_count--;
    }

    for (int v = 0; v < AUDIO_MIX_MAX_VOICES; v++) {
        AudioVoice *voice = &m->voices[v];
        if (!voice->clip) continue;

        int n = voice->remaining < frames ? voice->remaining : frames;
        for (int i = 0; i < n; i++) {
            float s = *voice->cursor++;
            if (channels == 1) {
                out[i] += s * voice->pan_l;
            } else {
                out[i * 2] += s * voice->pan_l;
                out[i * 2 + 1] += s * voice->pan_r;
            }
        }
        voice->remaining -= n;
        if (voice->remaining <= 0) {
            voice->clip = NULL;
            voice->cursor = NULL;
        }
    }

    /* Overlapping shots can sum past full scale; clamp instead of wrapping,
     * which would turn a loud moment into a nasty click. */
    int total = frames * channels;
    for (int i = 0; i < total; i++) {
        if (out[i] > 1.0f) out[i] = 1.0f;
        else if (out[i] < -1.0f) out[i] = -1.0f;
    }
}

void audio_mix_spatialise(float sx, float sy, float lx, float ly, float radius,
                          float *gain, float *pan) {
    float dx = sx - lx;
    float dy = sy - ly;
    if (radius <= 0.0f) radius = 1.0f;

    /* Both falloff and pan use the horizontal offset: the world is top-down
     * and the camera does not rotate, so "right of the listener" is +x. */
    float g = 1.0f - sqrtf(dx * dx + dy * dy) / radius;
    if (g < 0.0f) g = 0.0f;
    if (g > 1.0f) g = 1.0f;

    float p = dx / radius;
    if (p < -1.0f) p = -1.0f;
    if (p > 1.0f) p = 1.0f;

    if (gain) *gain = g;
    if (pan) *pan = p;
}
