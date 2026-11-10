# Merge hygiene — conflict markers in docs

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-23 13:02 UTC - note by audio: A clean-looking merge can still land an orphan conflict marker in docs; grep every marker form before committing

On 2026-10-23 the main-into-`feature/audio` merge `534c438` landed a bare `>>>>>>>` line in `docs/knowledge/gate-baseline.md` (found by net, removed in `c710f13`). Cause: the resolution script deleted `>>>>>>> main` (the marker with its branch label) but two of the base entries in this file already ended with a bare `>>>>>>>` left over from an earlier merge, and the pre-commit check ran before the cleanup pass.

Rule when resolving a docs conflict: after stitching the two sides, throw away everything from the marker line onward in the taken side, then run

    grep -rn '^<<<<<<<\|^>>>>>>>\|^=======$' docs

and require **no output** before `git add`. Check every marker form (`<<<<<<< HEAD`, bare `>>>>>>>`, and a `>>>>>>> <branch>`), not just the labelled one; `git diff --check` does **not** flag these.

Also: if a fix lands on `main` for something your lane also carries, fast-forward the lane to it immediately (audio did: `feature/audio` -> `6ac699f`). Otherwise the marker returns on the next main-into-lane merge.
