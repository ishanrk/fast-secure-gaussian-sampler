#include <stddef.h>
#include <stdint.h>

/*
 * Deterministic SplitMix64 stream for tests and benchmarks only.
 * This is not a cryptographic RNG.
 */
static uint64_t
splitmix64_next(uint64_t *state)
{
	uint64_t z;

	*state += UINT64_C(0x9E3779B97F4A7C15);
	z = *state;
	z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
	z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
	return z ^ (z >> 31);
}

void
maskaglia_test_rng(void *ctx, uint8_t *dst, size_t len)
{
	uint64_t *state = (uint64_t *)ctx;

	while (len > 0) {
		uint64_t word = splitmix64_next(state);
		size_t take = len < 8u ? len : 8u;
		size_t i;

		for (i = 0; i < take; i++) {
			dst[i] = (uint8_t)(word >> (8u * i));
		}
		dst += take;
		len -= take;
	}
}
