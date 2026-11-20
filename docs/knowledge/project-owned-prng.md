# Project-owned PRNG — the B63 decision

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-11-20 - decision by owner, implemented by G: the simulation gets its own PRNG

**Decision: the simulation stops drawing from libc `rand()` and draws from a
project-owned PCG32 stream in `src/core/rng.c` instead.** The owner approved
this on 2026-11-20 in answer to B63, including the re-baseline it forces.

### Why `rand()` could never work

`rand()` is implementation-defined. Its sequence, its period and even
`RAND_MAX` differ per C library, and nothing in the standard promises
otherwise. Measured, same seed, same source, same commit:

| | first three draws | `RAND_MAX` |
|---|---|---|
| MSVC (Windows) | 175, 400, 17869 | 32767 |
| glibc (Linux) | 71876166, 708592740, ... | 2147483647 |

They diverge at the **first zombie spawn** of wave 1. No `#ifdef` can fix
this: the only fix is to stop asking the platform for the numbers. Note also
that `rand() % 360` on MSVC can only ever produce 32768 distinct states, so
the spawn ring was measurably coarser on Windows even ignoring determinism.

### What this costs, deliberately

Both single-player byte/line references move. That is a **deliberate
re-baseline** under the AGENTS.md rule, not a drift: the spawn angles, ring
radii, zombie variants and sizes, item types and bot strafe directions are all
different numbers now, so the events log differs from the first wave onward.
The new figures are declared in [gate-baseline] and nowhere else.

What does **not** change: the gate's shape. Two same-seed runs must still be
byte-identical, and now they must be byte-identical *across platforms too*,
which is a strictly stronger guarantee than the project has ever had.

### Why PCG32 and not xorshift or an LCG

- It is specified in exact `uint64_t` arithmetic, which C defines to wrap
  mod 2^64 on every conforming implementation. There is no latitude for a
  platform to differ — that is the whole requirement.
- It passes TestU01 BigCrush, which `rand()` and a bare LCG do not. Spawn
  placement visibly benefits: an LCG's low bits cycle short, and
  `rand() % 2` on a bad LCG alternates.
- 64 bits of state, 32 bits of output, no allocation, no dependency.

### Rules that follow

1. **The simulation calls `zw_rand*()`, never `rand()`.** `tests.c` pins the
   stream against literal constants, so a `rand()` call sneaking back into
   the sim turns the cross-platform promise red instead of silent.
2. **Cosmetic randomness keeps its own state** and does not touch the sim
   stream. `src/ui/text_popup.c` already does exactly this and is the model;
   see its header comment. That rule is unchanged by this decision — it is
   now enforced against `zw_rand()` rather than `rand()`.
3. **`zw_rand()`'s output sequence is a wire-visible contract** in the sense
   that two peers running the same seed must agree. Changing the algorithm,
   the multiplier, the increment or the seeding procedure is a
   `NET_WORLD_GEN_VERSION` bump (it changes spawn rules) *and* a re-baseline.
   Do not "improve" it casually.

Related: [gate-baseline], `docs/BUGS.md` B63, `docs/VERSIONING.md` §2.
