#include "maskaglia_ref.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>

#include "test_util.h"

static int
dl_once(uint8_t bits, unsigned t, int32_t want)
{
	uint8_t b[8] = { bits, 0, 0, 0, 0, 0, 0, 0 };
	byte_src s = { b, sizeof b, 0 };
	mg_ref_params p;
	mg_rng r;
	int32_t z;

	check(mg_ref_params_init(&p, 3, 2, 63, 8) == MG_OK);
	check(mg_rng_init(&r, byte_read, &s) == MG_OK);
	check(mg_ref_dl(&z, t, &p, &r) == MG_OK);
	check(z == want);
	return 0;
}

static int
test_dl(void)
{
	/* t=0: reject negative zero, then take -2 */
	check(dl_once(0x33, 0, -2) == 0);
	check(dl_once(0x01, 0, 0) == 0);

	check(dl_once(0x01, 1, 0) == 0);
	check(dl_once(0x03, 1, 1) == 0);
	check(dl_once(0x04, 1, -2) == 0);
	check(dl_once(0x0c, 1, 3) == 0);
	return 0;
}

static int
test_exp(void)
{
	mg_ref_params p;
	mg_rat e;
	int z;
	unsigned t;
	long double s = 1.5L;

	check(mg_ref_params_init(&p, 3, 2, 63, 64) == MG_OK);
	check(mg_ref_exp(&e, 0, 0, &p) == MG_OK);
	check(e.num == 9 && e.den == 16);
	check(mg_ref_exp(&e, 1, 0, &p) == MG_OK);
	check(e.num == 1 && e.den == 144);
	check(mg_ref_exp(&e, 0, 1, &p) == MG_OK);
	check(e.num == 25 && e.den == 144);

	/* kernel identity behind the rejection transform */
	for (t = 0; t <= 1; t ++) {
		for (z = -32; z <= 32; z ++) {
			int d2 = 2 * z - (int)t;
			long double c = (long double)t / 2.0L;
			long double d = fabsl((long double)z - c);
			long double a;
			long double b;
			check(mg_ref_exp(&e, z, t, &p) == MG_OK);
			if (d2 < 0) {
				d2 = -d2;
			}
			/* s=3/2 identity after multiplying both sides by 144 */
			check((UINT64_C(72) * (uint64_t)d2 * e.den
				+ UINT64_C(144) * e.num)
				== (UINT64_C(16) * (uint64_t)d2 * (uint64_t)d2
					+ UINT64_C(81)) * e.den);
			a = d + (long double)e.num / (long double)e.den;
			b = ((long double)z - c) * ((long double)z - c)
				/ (s * s) + s * s / 4.0L;
			check(fabsl(a - b) <= 64.0L * LDBL_EPSILON
				* (fabsl(a) + fabsl(b) + 1.0L));
		}
	}
	return 0;
}

static int
test_map(void)
{
	int z;
	unsigned t;

	for (t = 0; t <= 1; t ++) {
		for (z = -20; z <= 20; z ++) {
			int32_t x;
			check(mg_ref_hawk_map(&x, z, t) == MG_OK);
			check((x & 1) == (int)t);
			check(x == 2 * z - (int)t);
		}
	}
	check(mg_ref_hawk_map(NULL, 0, 0) == MG_EINVAL);
	{
		int32_t x;
		check(mg_ref_hawk_map(&x, 0, 2) == MG_EINVAL);
		check(mg_ref_hawk_map(&x, INT32_MAX, 0) == MG_EOVERFLOW);
		check(mg_ref_hawk_map(&x, INT32_MAX, 1) == MG_EOVERFLOW);
		check(mg_ref_hawk_map(&x, INT32_MIN, 0) == MG_EOVERFLOW);
		check(mg_ref_hawk_map(&x, INT32_MIN, 1) == MG_EOVERFLOW);
		check(mg_ref_hawk_map(&x, INT32_MIN / 2, 0) == MG_OK);
		check(x == INT32_MIN);
	}
	return 0;
}

static int
test_prob_one(void)
{
	/* t=1, k=0, sign=0 gives z=0 and e=0 for s=1 */
	static const uint8_t b[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };
	byte_src s = { b, sizeof b, 0 };
	mg_ref_params p;
	mg_rng r;
	int32_t z;

	check(mg_ref_params_init(&p, 1, 1, 63, 1) == MG_OK);
	check(mg_rng_init(&r, byte_read, &s) == MG_OK);
	check(mg_ref_sample_fp(&z, 1, &p, &r) == MG_OK);
	check(z == 0);
	check(mg_rng_used(&r) == 2);
	return 0;
}

static int
test_params(void)
{
	mg_ref_params p;
	mg_rat e;
	long double want = 1.2739827004320285L;

	check(mg_ref_params_init(&p, 3, 2, 63, 64) == MG_OK);
	check(fabsl(mg_ref_sigma(&p) - want) < 1.0e-15L);
	check(mg_ref_params_init(&p, 0, 2, 63, 64) == MG_EINVAL);
	check(mg_ref_params_init(&p, 3, 0, 63, 64) == MG_EINVAL);
	check(mg_ref_params_init(&p, 3, 2, 63, 0) == MG_EINVAL);
	check(mg_ref_params_init(&p, UINT32_MAX, UINT32_MAX, 63, 64)
		== MG_EOVERFLOW);
	p.sn = UINT32_MAX;
	p.sd = 1;
	p.gmax = 63;
	p.tries = 64;
	check(mg_ref_exp(&e, INT32_MAX, 0, &p) == MG_EOVERFLOW);
	p.sn = 1;
	p.sd = UINT32_MAX;
	check(mg_ref_exp(&e, 1, 0, &p) == MG_EOVERFLOW);
	return 0;
}

int
main(void)
{
	check(test_params() == 0);
	check(test_dl() == 0);
	check(test_exp() == 0);
	check(test_map() == 0);
	check(test_prob_one() == 0);
	puts("test_ref: ok");
	return 0;
}
