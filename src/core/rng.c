#include "maskaglia.h"

#include <limits.h>

#include "../internal/common.h"

static void
add_used(mg_rng *r, unsigned n)
{
	if (UINT64_MAX - r->used < (uint64_t)n) {
		r->used = UINT64_MAX;
	} else {
		r->used += (uint64_t)n;
	}
}

static int
refill(mg_rng *r)
{
	uint8_t b[8];

	if (r->err != MG_OK) {
		return r->err;
	}
	if (r->read(r->ctx, b, sizeof b) != 0) {
		r->buf = 0;
		r->left = 0;
		r->err = MG_ERNG;
		return r->err;
	}
	r->buf = mg_load64le(b);
	r->left = 64;
	return MG_OK;
}

int
mg_rng_init(mg_rng *r, mg_read_fn read, void *ctx)
{
	if (r == NULL || read == NULL) {
		return MG_EINVAL;
	}
	r->read = read;
	r->ctx = ctx;
	r->buf = 0;
	r->used = 0;
	r->left = 0;
	r->err = MG_OK;
	return MG_OK;
}

int
mg_rng_bits(mg_rng *r, unsigned n, uint32_t *x)
{
	uint64_t v = 0;
	unsigned off = 0;

	if (r == NULL || x == NULL || n > 32) {
		return MG_EINVAL;
	}
	if (r->err != MG_OK) {
		*x = 0;
		return r->err;
	}
	while (off < n) {
		unsigned take;
		uint64_t mask;

		if (r->left == 0 && refill(r) != MG_OK) {
			*x = 0;
			return r->err;
		}
		take = n - off;
		if (take > r->left) {
			take = r->left;
		}
		mask = take == 64 ? UINT64_MAX : (((uint64_t)1 << take) - 1);
		v |= (r->buf & mask) << off;
		r->buf >>= take;
		r->left -= take;
		add_used(r, take);
		off += take;
	}
	*x = (uint32_t)v;
	return MG_OK;
}

int
mg_rng_u64(mg_rng *r, uint64_t *x)
{
	uint32_t lo;
	uint32_t hi;
	int rc;

	if (r == NULL || x == NULL) {
		return MG_EINVAL;
	}
	rc = mg_rng_bits(r, 32, &lo);
	if (rc != MG_OK) {
		*x = 0;
		return rc;
	}
	rc = mg_rng_bits(r, 32, &hi);
	if (rc != MG_OK) {
		*x = 0;
		return rc;
	}
	*x = (uint64_t)lo | ((uint64_t)hi << 32);
	return MG_OK;
}

static unsigned
ctz64(uint64_t x)
{
#if defined(__GNUC__) || defined(__clang__)
	return (unsigned)__builtin_ctzll((unsigned long long)x);
#else
	unsigned n = 0;
	while ((x & 1) == 0) {
		x >>= 1;
		n ++;
	}
	return n;
#endif
}

int
mg_rng_geom(mg_rng *r, uint32_t max, uint32_t *x)
{
	uint64_t n = 0;

	if (r == NULL || x == NULL) {
		return MG_EINVAL;
	}
	if (r->err != MG_OK) {
		*x = 0;
		return r->err;
	}
	for (;;) {
		unsigned z;
		unsigned take;

		if (r->left == 0 && refill(r) != MG_OK) {
			*x = 0;
			return r->err;
		}
		if (r->buf == 0) {
			n += r->left;
			add_used(r, r->left);
			r->left = 0;
			if (n > max) {
				*x = 0;
				r->err = MG_EBOUND;
				return r->err;
			}
			continue;
		}
		z = ctz64(r->buf);
		take = z + 1;
		n += z;
		if (take == 64) {
			r->buf = 0;
		} else {
			r->buf >>= take;
		}
		r->left -= take;
		add_used(r, take);
		if (n > max) {
			*x = 0;
			r->err = MG_EBOUND;
			return r->err;
		}
		*x = (uint32_t)n;
		return MG_OK;
	}
}

uint64_t
mg_rng_used(const mg_rng *r)
{
	return r == NULL ? 0 : r->used;
}

const char *
mg_strerror(int err)
{
	switch (err) {
	case MG_OK:
		return "ok";
	case MG_EINVAL:
		return "invalid argument";
	case MG_ERNG:
		return "random source failed";
	case MG_EBOUND:
		return "public sampling bound exceeded";
	case MG_EOVERFLOW:
		return "integer overflow";
	case MG_ENOTSUP:
		return "not supported";
	default:
		return "unknown error";
	}
}
