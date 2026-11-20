#ifndef ZW_CORE_RNG_H
#define ZW_CORE_RNG_H

#include <stdint.h>

/* The simulation's random number generator (B63).
 *
 * WHY THIS FILE EXISTS: libc `rand()` is implementation-defined. MSVC and
 * glibc, given the same seed, produce different sequences and different
 * RAND_MAX values (32767 vs 2147483647), so two builds of this game diverged
 * at the first zombie spawn of wave 1 and no cross-platform byte comparison
 * could ever succeed. No #ifdef fixes that; the only fix is to stop asking the
 * platform for the numbers. See docs/knowledge/project-owned-prng.md.
 *
 * The algorithm is PCG32 (O'Neill 2014), chosen because it is specified
 * entirely in uint64_t arithmetic, which C defines to wrap modulo 2^64 on
 * every conforming implementation. There is no latitude left for a platform to
 * differ -- which is the entire requirement here, ahead of speed or quality.
 *
 * THE SEQUENCE IS A CONTRACT. Two peers on the same seed must agree, and
 * tests.c pins the first draws against literal constants. Changing the
 * algorithm, the multiplier, the increment or the seeding procedure is a
 * NET_WORLD_GEN_VERSION bump plus a deliberate gate re-baseline.
 *
 * RULE: the simulation calls these functions and never `rand()`. Cosmetic
 * randomness (floating text spread, particle twinkle) must keep its own
 * private state so it cannot shift the simulation's draw order --
 * src/ui/text_popup.c is the model to copy. */

/* Largest value zw_rand() can return. Deliberately the same as glibc's
 * RAND_MAX so that `zw_rand() % n` keeps the range behaviour the call sites
 * were written against. */
#define ZW_RAND_MAX 0x7fffffff

/* Seed the stream. Equivalent in role to srand(); same seed, same sequence,
 * on every platform and every build. */
void zw_srand(uint32_t seed);

/* Raw 32-bit draw, uniform over the whole range. */
uint32_t zw_rand_u32(void);

/* Non-negative draw in [0, ZW_RAND_MAX]. The drop-in for rand(). */
int zw_rand(void);

/* Uniform in [0, n). Returns 0 for n <= 0 rather than dividing by zero.
 * Rejection-sampled, so unlike `zw_rand() % n` it carries no modulo bias --
 * use this for new code; the existing `% n` call sites are left alone because
 * changing them would move the baseline for no gameplay gain. */
int zw_rand_below(int n);

/* Uniform float in [0, 1). */
float zw_randf(void);

/* The generator's current state. For tests and diagnostics only -- nothing in
 * the simulation may branch on this. */
uint64_t zw_rng_state(void);

#endif /* ZW_CORE_RNG_H */
