#ifndef GOODMAXTON_RNG_H
#define GOODMAXTON_RNG_H

#include <stdint.h>

/* Générateur pseudo-aléatoire déterministe (splitmix64) : une même graine
 * donne toujours la même suite, ce qui rend les parties reproductibles. */
typedef struct {
    uint64_t state;
} Rng;

static inline void rng_seed(Rng *r, uint64_t seed) { r->state = seed; }

static inline uint64_t rng_next(Rng *r)
{
    uint64_t z = (r->state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

/* Flottant uniforme dans [0, 1). */
static inline float rng_float(Rng *r) { return (float)(rng_next(r) >> 40) / 16777216.0f; }

/* Flottant uniforme dans [lo, hi). */
static inline float rng_range(Rng *r, float lo, float hi)
{
    return lo + (hi - lo) * rng_float(r);
}

#endif
