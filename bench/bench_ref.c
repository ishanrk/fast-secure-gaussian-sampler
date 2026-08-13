#include "maskaglia_ref.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

typedef struct {
	uint64_t s;
} bench_rng;

static uint64_t
mix(bench_rng *r)
{
	uint64_t z;
	r->s += UINT64_C(0x9e3779b97f4a7c15);
	z = r->s;
	z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
	z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
	return z ^ (z >> 31);
}

static int
read_test(void *ctx, uint8_t *dst, size_t n)
{
	bench_rng *r = (bench_rng *)ctx;
	size_t i = 0;
	while (i < n) {
		uint64_t x = mix(r);
		unsigned j;
		for (j = 0; j < 8 && i < n; j ++, i ++) {
			dst[i] = (uint8_t)(x >> (8 * j));
		}
	}
	return 0;
}

int
main(void)
{
	enum { n = 250000 };
	bench_rng s = { UINT64_C(0x4d595df4d0f33173) };
	mg_ref_params p;
	mg_rng r;
	clock_t t0;
	clock_t t1;
	int64_t sum = 0;
	int i;

	if (mg_ref_params_init(&p, 3, 2, 63, 256) != MG_OK
		|| mg_rng_init(&r, read_test, &s) != MG_OK) {
		return 1;
	}
	t0 = clock();
	for (i = 0; i < n; i ++) {
		int32_t z;
		if (mg_ref_sample_fp(&z, 0, &p, &r) != MG_OK) {
			return 1;
		}
		sum += z;
	}
	t1 = clock();
	printf("approx ref: %.1f ns/sample, %.2f bits/sample, sum=%" PRId64 "\n",
		1.0e9 * (double)(t1 - t0) / CLOCKS_PER_SEC / n,
		(double)mg_rng_used(&r) / n, sum);
	return 0;
}
