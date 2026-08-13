#ifndef MG_TEST_UTIL_H
#define MG_TEST_UTIL_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define check(x) do { \
	if (!(x)) { \
		fprintf(stderr, "%s:%d: check failed: %s\n", \
			__FILE__, __LINE__, #x); \
		return 1; \
	} \
} while (0)

typedef struct {
	const uint8_t *p;
	size_t n;
	size_t off;
} byte_src;

static inline int
byte_read(void *ctx, uint8_t *dst, size_t n)
{
	byte_src *s = (byte_src *)ctx;
	if (n > s->n - s->off) {
		return -1;
	}
	memcpy(dst, s->p + s->off, n);
	s->off += n;
	return 0;
}

typedef struct {
	uint64_t s;
} test_prng;

static inline uint64_t
splitmix(test_prng *r)
{
	uint64_t z;
	r->s += UINT64_C(0x9e3779b97f4a7c15);
	z = r->s;
	z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
	z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
	return z ^ (z >> 31);
}

/* tests only, never a production random source */
static inline int
test_read(void *ctx, uint8_t *dst, size_t n)
{
	test_prng *r = (test_prng *)ctx;
	size_t i = 0;
	while (i < n) {
		uint64_t x = splitmix(r);
		unsigned j;
		for (j = 0; j < 8 && i < n; j ++, i ++) {
			dst[i] = (uint8_t)(x >> (8 * j));
		}
	}
	return 0;
}

#endif
