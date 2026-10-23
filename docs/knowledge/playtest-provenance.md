# Playtest provenance

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-23 12:13 UTC - decision by pm: Playtest the tree that contains the fix; verify provenance before launching

**Rule:** before launching a playtest build, prove the fix commit is an ancestor of the tree being built, and launch from that tree's own `build/` (assets are resolved relative to cwd/build):

```sh
git -C <tree> merge-base --is-ancestor <fix-sha> HEAD && echo "fix present"
grep -q '<expected>' <tree>/<changed-file>   # or check the texture/asset exists
```

`main` only contains what has been merged into it. Lane branches (`feature/*` in `worktrees/*`) hold un-landed work. A lane being gate-green means its own tree builds and tests, **not** that `main` has it.

When a fix lives on a lane: either build/run that lane's `build/`, or land the lane into `main` first and run one merged build. Prefer the merged build once the owner is verifying several lanes at once, because a per-lane build silently omits the other lanes' fixes (e.g. the assets lane has no animation walk or net swords).

Incident: 2026-10-23 owner playtest of B43/B45/B44/AT9 was run from `main` (`38ee6e8`), which lacked the assets lane; the fixes were present all along on `cc5d5b9`. Owner reaction: "none of the stuff were fixed" — a false negative that nearly led to escalating to another tool. Corrected by building `worktrees/assets/build` and then landing assets into `main`.

**Why:** On 2026-10-23 the owner playtested a build of `main` to verify B43/B45/B44/AT9 and saw none of the fixes: those commits existed only on `feature/assets-maps`, which had never been landed. Green lane gates do not put code in `main`, and launching the wrong tree wastes the owner's verification pass and looks like the fix failed.
