#include "core/rng.h"

/* PCG32: state advances by an LCG mod 2^64, output is an xorshift of the high
 * bits followed by a data-dependent rotation. The permutation is what makes it
 * pass BigCrush where the bare LCG underneath it fails badly -- and in
 * particular it is why the low bits are usable, so `% 2` and `% 3` at the call
 * sites do not alternate or cycle short the way they do on a raw LCG. */
#define ZW_PCG_MULT 6364136223846793005ULL
#define ZW_PCG_INC  1442695040888963407ULL  /* must be odd */

/* Default state = zw_srand(0). Spelling it out means a forgotten seed call
 * still gives a defined, reproducible stream rather than whatever was in
 * memory. */
static uint64_t g_state = 0ULL;

uint32_t zw_rand_u32(void) {
    uint64_t old = g_state;
    g_state = old * ZW_PCG_MULT + ZW_PCG_INC;

    uint32_t xorshifted = (uint32_t)(((old >> 18) ^ old) >> 27);
    uint32_t rot = (uint32_t)(old >> 59);
    /* Rotate right by `rot`. The `& 31` keeps the left shift defined when
     * rot == 0 -- shifting a 32-bit value by 32 is undefined behaviour, and
     * this is the one line of PCG that implementations get wrong. */
    return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
}

void zw_srand(uint32_t seed) {
    /* The canonical PCG seeding dance: zero the state, step, add the seed,
     * step again. Stepping twice decorrelates nearby seeds, so seed 42 and
     * seed 43 do not produce near-identical first draws (a bare `state = seed`
     * LCG does exactly that, and the project's gate uses seed 42). */
    g_state = 0ULL;
    (void)zw_rand_u32();
    g_state += (uint64_t)seed;
    (void)zw_rand_u32();
}

int zw_rand(void) {
    /* Drop the top bit rather than masking the bottom: on PCG both halves are
     * equally good, and a shift keeps the result non-negative without the
     * signed-overflow question a cast of the full 32 bits would raise. */
    return (int)(zw_rand_u32() >> 1);
}

int zw_rand_below(int n) {
    if (n <= 0) return 0;

    /* Rejection sampling: discard the partial bucket at the top of the 32-bit
     * range so every output is equally likely. Expected cost is under two
     * draws for any n the game uses. */
    uint32_t bound = (uint32_t)n;
    uint32_t threshold = (uint32_t)(-(int32_t)bound) % bound; /* 2^32 % bound */
    for (;;) {
        uint32_t r = zw_rand_u32();
        if (r >= threshold) return (int)(r % bound);
    }
}

float zw_randf(void) {
    /* 24 bits -- the mantissa of a float. Taking all 32 would round to exactly
     * 1.0f for the largest draws and break the half-open range. */
    return (float)(zw_rand_u32() >> 8) / (float)0x1000000u;
}

uint64_t zw_rng_state(void) {
    return g_state;
}
