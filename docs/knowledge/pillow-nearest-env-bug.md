# Pillow nearest env bug

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-11-16 08:32 UTC - note by pm: Pillow 12.1.1 on Python 3.14.4 here fails only Image.NEAREST resize

Environment issue (2026-11-16), NOT a repo bug: `python3 tools/gen_assets.py --verify` crashes in Pillow with `SystemError: <method 'resize' of 'ImagingCore' objects> returned NULL without setting an exception`.

Isolated: `Image.new('RGBA',(8,8)).resize((160,160), Image.NEAREST)` fails the same way, while BILINEAR, BICUBIC, LANCZOS and BOX all succeed. So the fault is specifically Pillow's NEAREST path in this build (Pillow 12.1.1, Python 3.14.4, /usr/lib/python3/dist-packages).

Consequences: the `gen_assets.py --verify` gate step cannot run right now, for any agent. It is not asset corruption — `assets/` is byte-identical to the last green verify (`git diff 03b0e6e HEAD -- assets/` empty; 50 textures). Do not "fix" assets or the generator in response to this; it is the Python/Pillow install. Workaround options if a verify run is truly needed: use a working Pillow (another interpreter/venv) or a resample other than NEAREST — but do not change gen_assets.py's output path casually, since assets are byte-gated.

Flagged to the owner. Re-check before relying on the asset-verify step in the gate.

## 2026-11-18 addendum - bug did NOT reproduce

On 2026-11-18, `python3 tools/gen_assets.py --verify` ran clean on this same
machine and reported **50 textures** (both on the assets lane and after the PM
merged H5 into main). So the NEAREST failure is intermittent/environmental, not
a permanent breakage and not a repo bug. Re-run the verify rather than assuming
it is blocked; only if it fails repeatedly should it be recorded as ENV-BLOCKED
for that session.
