#!/usr/bin/env python3
"""Generate placeholder sound effects for the game.

Every sound is synthesised here from a fixed seed: no samples are downloaded,
no files are vendored, and nothing depends on the host's Python version.
Output is 48 kHz mono 16-bit PCM, byte-identical on every run, so regenerating
leaves `git status` clean.

The slot names match AudioSfx in src/audio/audio.h. A slot with no file is
silent, never an error, so adding a sound is a row in SOUNDS plus a path in
SFX_PATHS - not an engine change.

Usage:  python3 tools/gen_audio.py [--verify]
"""

import argparse
import array
import math
import os
import sys
import wave

RATE = 48000
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "audio")


# ------------------------------------------------------------------ synthesis

class Noise:
    """xorshift32: our own PRNG so regenerated files never depend on the
    standard library's random implementation."""

    def __init__(self, seed):
        self.s = seed & 0xFFFFFFFF or 1

    def next(self):
        x = self.s
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= x >> 17
        x ^= (x << 5) & 0xFFFFFFFF
        self.s = x
        return x / 0xFFFFFFFF * 2.0 - 1.0


def noise(seed, n):
    r = Noise(seed)
    return [r.next() for _ in range(n)]


def lowpass(sig, cutoff):
    a = 1.0 - math.exp(-2.0 * math.pi * cutoff / RATE)
    y = 0.0
    out = []
    for v in sig:
        y += a * (v - y)
        out.append(y)
    return out


def highpass(sig, cutoff):
    lp = lowpass(sig, cutoff)
    return [v - l for v, l in zip(sig, lp)]


def sweep_lowpass(sig, f0, f1):
    y = 0.0
    out = []
    n = len(sig)
    for i, v in enumerate(sig):
        f = f0 + (f1 - f0) * i / max(1, n - 1)
        a = 1.0 - math.exp(-2.0 * math.pi * f / RATE)
        y += a * (v - y)
        out.append(y)
    return out


def glide(seconds, f0, f1, vib_hz=0.0, vib_depth=0.0):
    n = int(seconds * RATE)
    out = []
    ph = 0.0
    for i in range(n):
        f = f0 + (f1 - f0) * i / max(1, n - 1)
        if vib_hz:
            f += vib_depth * math.sin(2.0 * math.pi * vib_hz * i / RATE)
        ph += 2.0 * math.pi * f / RATE
        out.append(math.sin(ph))
    return out


def tone(seconds, freq, harmonics=(1.0,)):
    n = int(seconds * RATE)
    out = [0.0] * n
    for k, amp in enumerate(harmonics, start=1):
        if amp == 0.0:
            continue
        for i in range(n):
            out[i] += amp * math.sin(2.0 * math.pi * freq * k * i / RATE)
    return out


def decay(seconds, attack=0.001, tau=0.03):
    """Attack, then exponential decay with time constant tau."""
    n = int(seconds * RATE)
    out = []
    for i in range(n):
        t = i / RATE
        out.append(t / attack if t < attack else math.exp(-(t - attack) / tau))
    return out


def shaped(sig, env):
    return [v * e for v, e in zip(sig, env)]


def mix(*layers):
    n = max(len(layer) for layer in layers)
    out = [0.0] * n
    for layer in layers:
        for i, v in enumerate(layer):
            out[i] += v
    return out


def gain(sig, g):
    return [v * g for v in sig]


# ---------------------------------------------------------------------- sounds
# Each builder returns a float signal; the table below sets the peak it is
# normalised to, so an explosion is louder than a pickup on purpose.

def sfx_shot_bullet():
    return mix(
        shaped(highpass(noise(1001, int(0.14 * RATE)), 900.0), decay(0.14, 0.0008, 0.018)),
        gain(shaped(glide(0.06, 150.0, 90.0), decay(0.06, 0.001, 0.02)), 0.8),
    )


def sfx_shot_grenade():
    return mix(
        shaped(lowpass(noise(1002, int(0.22 * RATE)), 900.0), decay(0.22, 0.002, 0.05)),
        gain(shaped(glide(0.12, 120.0, 70.0), decay(0.12, 0.001, 0.04)), 0.9),
    )


def sfx_shot_rocket():
    n = int(0.35 * RATE)
    whoosh = sweep_lowpass(noise(1003, n), 500.0, 2400.0)
    whoosh = sweep_lowpass(whoosh, 2400.0, 600.0)
    env = [math.sin(math.pi * i / (n - 1)) ** 1.5 for i in range(n)]
    return mix(
        shaped(whoosh, env),
        gain(shaped(glide(0.18, 80.0, 55.0), decay(0.18, 0.002, 0.06)), 0.7),
    )


def sfx_hit_zombie():
    body = highpass(lowpass(noise(1004, int(0.12 * RATE)), 1200.0), 200.0)
    return mix(
        shaped(body, decay(0.12, 0.0008, 0.025)),
        gain(shaped(glide(0.08, 170.0, 120.0), decay(0.08, 0.001, 0.022)), 0.9),
    )


def sfx_hit_player():
    return mix(
        gain(shaped(glide(0.09, 110.0, 75.0), decay(0.09, 0.001, 0.028)), 1.0),
        shaped(lowpass(noise(1005, int(0.16 * RATE)), 450.0), decay(0.16, 0.001, 0.05)),
        gain(shaped(tone(0.02, 200.0), decay(0.02, 0.0005, 0.006)), 0.25),
    )


def sfx_explosion():
    n = int(0.9 * RATE)
    rumble = sweep_lowpass(noise(1006, n), 900.0, 220.0)
    return mix(
        gain(shaped(glide(0.5, 65.0, 44.0), decay(0.5, 0.003, 0.18)), 1.0),
        shaped(lowpass(noise(1007, n), 1800.0), decay(0.9, 0.002, 0.16)),
        gain(shaped(rumble, decay(0.9, 0.01, 0.35)), 0.8),
    )


def sfx_death_zombie():
    return mix(
        shaped(glide(0.5, 230.0, 70.0, vib_hz=6.0, vib_depth=8.0), decay(0.5, 0.01, 0.22)),
        gain(shaped(lowpass(noise(1008, int(0.35 * RATE)), 700.0), decay(0.35, 0.01, 0.1)), 0.35),
    )


def sfx_death_player():
    return mix(
        gain(shaped(glide(0.7, 300.0, 105.0, vib_hz=5.0, vib_depth=6.0), decay(0.7, 0.02, 0.3)), 1.0),
        gain(shaped(glide(0.7, 150.0, 58.0), decay(0.7, 0.02, 0.3)), 0.6),
        gain(shaped(lowpass(noise(1009, int(0.5 * RATE)), 600.0), decay(0.5, 0.02, 0.16)), 0.3),
    )


def sfx_wave_start():
    horn = (1.0, 0.5, 0.28, 0.12)
    first = shaped(tone(0.3, 330.0, horn), decay(0.3, 0.02, 0.22))
    second = shaped(tone(0.46, 440.0, horn), decay(0.46, 0.02, 0.3))
    gap = [0.0] * int(0.06 * RATE)
    return first + gap + gain(second, 1.1)


def sfx_wave_end():
    horn = (1.0, 0.4, 0.2)
    first = shaped(tone(0.34, 440.0, horn), decay(0.34, 0.02, 0.28))
    second = shaped(tone(0.55, 294.0, horn), decay(0.55, 0.02, 0.36))
    gap = [0.0] * int(0.06 * RATE)
    return first + gap + gain(second, 1.05)


def sfx_player_hurt():
    return mix(
        shaped(glide(0.3, 175.0, 108.0, vib_hz=7.0, vib_depth=5.0), decay(0.3, 0.006, 0.12)),
        gain(shaped(lowpass(noise(1010, int(0.2 * RATE)), 500.0), decay(0.2, 0.006, 0.07)), 0.35),
    )


def sfx_pickup():
    first = shaped(tone(0.1, 880.0, (1.0, 0.3)), decay(0.1, 0.002, 0.05))
    gap = [0.0] * int(0.05 * RATE)
    second = shaped(tone(0.18, 1320.0, (1.0, 0.25)), decay(0.18, 0.002, 0.08))
    return first + gap + second


def sfx_points():
    return mix(
        shaped(tone(0.25, 1568.0, (1.0, 0.4)), decay(0.25, 0.001, 0.05)),
        gain(shaped(tone(0.25, 2093.0, (1.0,)), decay(0.25, 0.001, 0.03)), 0.5),
        [0.0] * int(0.02 * RATE) + shaped(tone(0.23, 2637.0), decay(0.23, 0.001, 0.025)),
    )


# name -> (builder, peak). The name is the AudioSfx slot and the file stem.
SOUNDS = [
    ("shot_bullet", sfx_shot_bullet, 0.55),
    ("shot_grenade", sfx_shot_grenade, 0.55),
    ("shot_rocket", sfx_shot_rocket, 0.6),
    ("hit_zombie", sfx_hit_zombie, 0.5),
    ("hit_player", sfx_hit_player, 0.55),
    ("explosion", sfx_explosion, 0.75),
    ("death_zombie", sfx_death_zombie, 0.5),
    ("death_player", sfx_death_player, 0.55),
    ("wave_start", sfx_wave_start, 0.5),
    ("wave_end", sfx_wave_end, 0.5),
    ("player_hurt", sfx_player_hurt, 0.45),
    ("pickup", sfx_pickup, 0.4),
    ("points", sfx_points, 0.35),
]


# ----------------------------------------------------------------------- write

def finish(sig, peak):
    """Fade the edges (a click at the end is audible), normalise to the sound's
    own peak, clamp, and return 16-bit samples."""
    n = len(sig)
    fade_in = min(int(0.0005 * RATE), n)
    fade_out = min(int(0.003 * RATE), n)
    for i in range(fade_in):
        sig[i] *= i / fade_in
    for i in range(fade_out):
        sig[n - 1 - i] *= i / fade_out

    top = max(abs(v) for v in sig) or 1.0
    scale = peak / top
    return [max(-1.0, min(1.0, v * scale)) for v in sig]


def encode(sig):
    samples = array.array("h", (int(round(v * 32767.0)) for v in sig))
    if sys.byteorder != "little":
        samples.byteswap()
    return samples.tobytes()


def build(builder, peak):
    return encode(finish(builder(), peak))


def write_wav(path, payload):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(payload)


def verify(path, payload):
    with wave.open(path, "rb") as w:
        assert w.getnchannels() == 1, path
        assert w.getsampwidth() == 2, path
        assert w.getframerate() == RATE, path
        assert w.getnframes() == len(payload) // 2, path
        frames = w.readframes(w.getnframes())
    if frames != payload:
        raise SystemExit(f"{path}: regenerated samples differ - generator is not deterministic")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--verify", action="store_true",
                    help="re-synthesise and compare against the committed files")
    args = ap.parse_args()

    os.makedirs(OUT_DIR, exist_ok=True)

    for name, builder, peak in SOUNDS:
        path = os.path.join(OUT_DIR, f"{name}.wav")
        payload = build(builder, peak)
        if args.verify:
            verify(path, payload)
        else:
            write_wav(path, payload)

    verb = "verified" if args.verify else "wrote"
    print(f"{verb} {len(SOUNDS)} sounds in {os.path.normpath(OUT_DIR)}")


if __name__ == "__main__":
    main()
