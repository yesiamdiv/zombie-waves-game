#ifndef AUDIO_MIXER_H
#define AUDIO_MIXER_H

#include <stdbool.h>

/* Pure PCM mixing: no SDL, no device, no allocation, no RNG.
 *
 * The game thread queues one-shot requests; the audio thread (SDL's callback)
 * drains them into a fixed voice pool and mixes. Both sides only touch fixed
 * arrays, so a frame can never allocate and a game hitch cannot cause an
 * underrun - the voices keep playing until their samples run out.
 *
 * This is also the part a gate cannot hear, so it is the part with unit tests.
 */

#define AUDIO_MIX_MAX_VOICES  32
#define AUDIO_MIX_MAX_PENDING 64

typedef struct {
    const float *samples; /* mono, at the mixer's rate, owned by the caller */
    int frames;
} AudioClip;

typedef struct {
    const AudioClip *clip;
    const float *cursor; /* next source frame to read */
    float gain;
    float pan_l;
    float pan_r;
    int remaining;
} AudioVoice;

typedef struct {
    const AudioClip *clip;
    float gain;
    float pan;
} AudioRequest;

typedef struct {
    AudioVoice voices[AUDIO_MIX_MAX_VOICES];
    AudioRequest pending[AUDIO_MIX_MAX_PENDING];
    int pending_head;
    int pending_count;
    int rate;
    int channels;
} AudioMixer;

void audio_mixer_init(AudioMixer *m, int rate, int channels);

/* Queue a one-shot. Returns false when the pending ring is full; the caller
 * drops the sound (never blocks, never allocates). */
bool audio_mixer_queue(AudioMixer *m, const AudioClip *clip, float gain,
                       float pan);

/* Mix `frames` interleaved output frames into `out`, writing silence before
 * and after the active voices. Advances/retires voices and drains the pending
 * ring. The output is clamped to [-1, 1]. */
void audio_mixer_render(AudioMixer *m, float *out, int frames);

int audio_mixer_active(const AudioMixer *m);

/* Distance falloff and stereo pan for a world position relative to the
 * listener. Gain is 1 at the listener and 0 at `radius` (linear); pan is the
 * x-offset scaled by the same radius, so a sound at the listener is centred
 * and a sound `radius` to a side is fully panned. Pure and stateless: the same
 * input always gives the same output on every machine. */
void audio_mix_spatialise(float sx, float sy, float lx, float ly, float radius,
                          float *gain, float *pan);

#endif
