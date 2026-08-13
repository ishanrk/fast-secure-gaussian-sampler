#ifndef MG_COMMON_H
#define MG_COMMON_H

#include <stdint.h>

static inline uint64_t
mg_load64le(const uint8_t x[8])
{
	return (uint64_t)x[0]
		| ((uint64_t)x[1] << 8)
		| ((uint64_t)x[2] << 16)
		| ((uint64_t)x[3] << 24)
		| ((uint64_t)x[4] << 32)
		| ((uint64_t)x[5] << 40)
		| ((uint64_t)x[6] << 48)
		| ((uint64_t)x[7] << 56);
}

static inline int
mg_mul64(uint64_t a, uint64_t b, uint64_t *z)
{
	if (a != 0 && b > UINT64_MAX / a) {
		return 0;
	}
	*z = a * b;
	return 1;
}

static inline uint64_t
mg_gcd64(uint64_t a, uint64_t b)
{
	while (b != 0) {
		uint64_t t = a % b;
		a = b;
		b = t;
	}
	return a;
}

#endif
