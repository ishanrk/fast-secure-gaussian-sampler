#include "maskaglia_ref.h"

#include <math.h>
#include <stdint.h>

#include "test_util.h"

static int
run(unsigned t, uint64_t seed)
{
	enum { n = 50000 };
	test_prng s = { seed };
	mg_ref_params p;
	mg_rng r;
	long double sx = 0;
	long double sx2 = 0;
	long double sw = 0;
	long double tw = 0;
	long double tw2 = 0;
	long double c = (long double)t / 2.0L;
	long double ss = 1.5L;
	int i;
	int z;

	check(mg_ref_params_init(&p, 3, 2, 63, 256) == MG_OK);
	check(mg_rng_init(&r, test_read, &s) == MG_OK);
	for (i = 0; i < n; i ++) {
		int32_t x;
		check(mg_ref_sample_fp(&x, t, &p, &r) == MG_OK);
		sx += x;
		sx2 += (long double)x * x;
	}
	for (z = -24; z <= 24; z ++) {
		long double d = (long double)z - c;
		long double w = exp2l(-(d * d) / (ss * ss));
		sw += w;
		tw += w * z;
		tw2 += w * z * z;
	}
	sx /= n;
	sx2 /= n;
	tw /= sw;
	tw2 /= sw;
	check(fabsl(sx - tw) < 0.025L);
	check(fabsl(sx2 - tw2) < 0.04L);
	return 0;
}

int
main(void)
{
	check(run(0, UINT64_C(0x123456789abcdef0)) == 0);
	check(run(1, UINT64_C(0x0fedcba987654321)) == 0);
	puts("test_stats: ok (approximate oracle)");
	return 0;
}
