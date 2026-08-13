#include "maskaglia_ref.h"

#include <limits.h>
#include <math.h>

#include "../internal/common.h"

/*
 * scalar maskaglia reference
 *
 * transform from abou haidar, espitau, hoffmann, tibouchi,
 * "maskaglia", ia.cr/2026/988, 2026.
 *
 * this file is unmasked, variable-time, and the exp2l path is approximate.
 */

static int
valid(const mg_ref_params *p, unsigned t)
{
	return p != NULL && p->sn != 0 && p->sd != 0
		&& p->tries != 0 && t <= 1;
}

int
mg_ref_dl(int32_t *z, unsigned t, const mg_ref_params *p, mg_rng *r)
{
	uint32_t i;

	if (z == NULL || r == NULL || !valid(p, t)) {
		return MG_EINVAL;
	}
	for (i = 0; i < p->tries; i ++) {
		uint32_t k;
		uint32_t b;
		int rc;

		rc = mg_rng_geom(r, p->gmax, &k);
		if (rc != MG_OK) {
			return rc;
		}
		if (k > (uint32_t)INT32_MAX) {
			return MG_EOVERFLOW;
		}
		rc = mg_rng_bits(r, 1, &b);
		if (rc != MG_OK) {
			return rc;
		}
		if (t == 0) {
			/* remove duplicate negative zero */
			if (k == 0 && b != 0) {
				continue;
			}
			*z = b != 0 ? -(int32_t)k : (int32_t)k;
		} else {
			if (b != 0 && k == (uint32_t)INT32_MAX) {
				return MG_EOVERFLOW;
			}
			*z = b != 0 ? (int32_t)k + 1 : -(int32_t)k;
		}
		return MG_OK;
	}
	return MG_EBOUND;
}

int
mg_ref_exp(mg_rat *e, int32_t z, unsigned t,
	const mg_ref_params *p)
{
	int64_t q;
	uint64_t d;
	uint64_t sn2;
	uint64_t sd2;
	uint64_t a;
	uint64_t v;
	uint64_t den;
	uint64_t g;

	if (e == NULL || !valid(p, t)) {
		return MG_EINVAL;
	}
	q = (int64_t)z * 2 - (int64_t)t;
	d = q < 0 ? (uint64_t)(-q) : (uint64_t)q;
	if (!mg_mul64(p->sn, p->sn, &sn2)
		|| !mg_mul64(p->sd, p->sd, &sd2)
		|| !mg_mul64(d, sd2, &a)) {
		return MG_EOVERFLOW;
	}
	v = a > sn2 ? a - sn2 : sn2 - a;
	if (!mg_mul64(v, v, &e->num)
		|| !mg_mul64(sn2, sd2, &den)
		|| !mg_mul64(den, 4, &e->den)) {
		return MG_EOVERFLOW;
	}
	g = mg_gcd64(e->num, e->den);
	if (g != 0) {
		e->num /= g;
		e->den /= g;
	}
	return MG_OK;
}

int
mg_ref_sample_fp(int32_t *z, unsigned t, const mg_ref_params *p,
	mg_rng *r)
{
	uint32_t i;

	if (z == NULL || r == NULL || !valid(p, t)) {
		return MG_EINVAL;
	}
	for (i = 0; i < p->tries; i ++) {
		mg_rat e;
		int32_t y;
		uint64_t w;
		long double q;
		long double u;
		int rc;

		rc = mg_ref_dl(&y, t, p, r);
		if (rc != MG_OK) {
			return rc;
		}
		rc = mg_ref_exp(&e, y, t, p);
		if (rc != MG_OK) {
			return rc;
		}
		/* exact probability one, no coin needed */
		if (e.num == 0) {
			*z = y;
			return MG_OK;
		}
		rc = mg_rng_u64(r, &w);
		if (rc != MG_OK) {
			return rc;
		}
		q = (long double)e.num / (long double)e.den;
		/* diagnostic only, no portable error bound */
		u = ldexpl((long double)w + 0.5L, -64);
		if (u < exp2l(-q)) {
			*z = y;
			return MG_OK;
		}
	}
	return MG_EBOUND;
}

int
mg_ref_fill_fp(int32_t *z, size_t n, unsigned t,
	const mg_ref_params *p, mg_rng *r)
{
	size_t i;

	if (z == NULL && n != 0) {
		return MG_EINVAL;
	}
	for (i = 0; i < n; i ++) {
		int rc = mg_ref_sample_fp(&z[i], t, p, r);
		if (rc != MG_OK) {
			return rc;
		}
	}
	return MG_OK;
}

int
mg_ref_hawk_map(int32_t *x, int32_t z, unsigned t)
{
	int64_t y;

	if (x == NULL || t > 1) {
		return MG_EINVAL;
	}
	y = (int64_t)z * 2 - (int64_t)t;
	if (y < INT32_MIN || y > INT32_MAX) {
		return MG_EOVERFLOW;
	}
	*x = (int32_t)y;
	return MG_OK;
}
