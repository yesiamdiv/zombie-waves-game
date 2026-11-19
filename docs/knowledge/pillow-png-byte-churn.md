# Pillow rewrites every asset PNG byte-for-byte with identical pixels

<!-- Project knowledge. Agents append entries; edit freely. -->

## 2026-11-20 - note by art: on this Windows box, `gen_assets.py` dirties all 50 PNGs with zero pixel change

Running `python tools/gen_assets.py` in a clean worktree on the Windows box
(Python 3.10.0, Pillow 12.0.0) leaves **all 50 texture files** showing as
modified in `git status`, while **every pixel is identical**. Measured, not
assumed: re-decoding `git show HEAD:<path>` and the working copy and comparing
`Image.tobytes()` gives 0 differing files out of 50.

So it is a PNG *encoder* difference (chunk layout / compression parameters)
between Pillow 12.0.0 here and whatever Pillow produced the committed files -
not asset corruption, not a generator bug, and not something to "fix".

Consequences for anyone working in this repo on Windows:

- `assets/` is **byte-gated in git but only pixel-gated in meaning.** A diff of
  50 files after a generator run proves nothing on its own.
- When committing an art change, commit only the files whose **pixels** moved
  and `git checkout --` the rest, or the real change is invisible inside 50
  files of encoder churn.
- The reverse mistake is worse: do not conclude "the generator changed nothing"
  because `git status` is noisy. Compare decoded pixels.

A short way to tell them apart, from the repo root:

```sh
python - <<'PY'
import subprocess, io
from PIL import Image
files = subprocess.run(["git","diff","--name-only","--","assets/"],
                       capture_output=True, text=True).stdout.split()
changed = []
for p in files:
    old = subprocess.run(["git","show","HEAD:"+p], capture_output=True).stdout
    if (Image.open(io.BytesIO(old)).convert("RGBA").tobytes()
            != Image.open(p).convert("RGBA").tobytes()):
        changed.append(p)
print(len(files), "byte-changed,", len(changed), "pixel-changed:", changed)
PY
```

Related but distinct: `docs/knowledge/pillow-nearest-env-bug.md` is an
*intermittent crash* in Pillow's NEAREST resize on the Linux box. It did not
reproduce here - NEAREST resize works fine on Pillow 12.0.0 / Windows, and
`--verify` reports a clean 50 textures.
