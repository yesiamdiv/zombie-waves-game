#!/usr/bin/env python3
"""Which game sounds appear in a captured audio file (Phase E aid).

Usage:
    python3 tools/match_audio_capture.py <capture.f32|.wav> [sounds dir] [--min-score S]

Capture through the game's own device path first:

    SDL_AUDIODRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/tmp/game.f32 \\
        ./build/open_world_zombie_waves --ai=bot --auto-start --run-seconds=20

The capture is the mixer's output (2 ch F32LE 48 kHz for the disk driver). Each
sound is matched by waveform normalised cross-correlation with local-window
normalisation, so pan/gain and overlapping sounds do not hide it.

Read the scores honestly:
  * >= 0.95  a scaled copy of that clip starts there - proof it played.
  * 0.6-0.9  consistent with playing, but clips from the same family (two
             noise bursts) also reach this range when the real one is absent.
             Cross-check the time against the `--events` log.
  * < 0.6    not found at that offset.

Requires numpy. This is a verification aid, not part of the gate.
"""
import glob
import os
import sys
import wave

import numpy as np

RATE = 48000


def read_capture(path):
    if path.endswith(".wav"):
        with wave.open(path, "rb") as w:
            n, ch, sw = w.getnframes(), w.getnchannels(), w.getsampwidth()
            raw = w.readframes(n)
        if sw != 2:
            raise SystemExit("only 16-bit wav captures are supported")
        a = np.frombuffer(raw, dtype="<i2").astype(np.float32) / 32768.0
        rate = 48000
    else:
        a = np.fromfile(path, dtype="<f4")
        ch, rate = 2, RATE
    return a.reshape(-1, ch).mean(axis=1), rate


def read_clip(path):
    with wave.open(path, "rb") as w:
        n = w.getnframes()
        a = np.frombuffer(w.readframes(n), dtype="<i2").astype(np.float32) / 32768.0
    return a - a.mean()


def match(capture, clip):
    """Best sliding-window NCC: (score, start_index).

    score 1.0 means a scaled copy of `clip` starts at that sample."""
    n, m = len(capture), len(clip)
    if m == 0 or n < m:
        return 0.0, 0
    size = 1
    while size < n + m:
        size *= 2
    corr = np.fft.irfft(np.fft.rfft(capture, size) * np.fft.rfft(clip[::-1], size), size)
    energy = np.fft.irfft(np.fft.rfft(capture * capture, size) *
                          np.fft.rfft(np.ones(m), size), size)
    k = np.arange(m - 1, n)
    score = corr[k] / np.sqrt(np.maximum(energy[k], 1e-20) * float((clip * clip).sum()))
    score = np.clip(score, -1.0, 1.0)
    i = int(np.argmax(score))
    return float(score[i]), int(k[i]) - m + 1


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not args:
        raise SystemExit(__doc__)
    capture_path = args[0]
    sounds_dir = args[1] if len(args) > 1 else os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "audio")
    floor = float(sys.argv[sys.argv.index("--min-score") + 1]) if "--min-score" in sys.argv else 0.5

    capture, rate = read_capture(capture_path)
    capture = capture - capture.mean()
    print("capture=%s %.1fs sr=%d peak=%.3f" %
          (os.path.basename(capture_path), len(capture) / rate, rate, np.abs(capture).max()))

    rows = []
    for path in sorted(glob.glob(os.path.join(sounds_dir, "*.wav"))):
        clip = read_clip(path)
        if not np.any(clip):
            continue
        score, start = match(capture, clip)
        rows.append((score, start / rate, os.path.basename(path)))

    for score, t, name in sorted(rows, reverse=True):
        mark = "PLAYED" if score >= floor else "  -   "
        print("%s %-20s score=%.3f at t=%6.2fs" % (mark, name, score, t))


if __name__ == "__main__":
    main()
