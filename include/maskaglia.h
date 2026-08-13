#ifndef MASKAGLIA_H
#define MASKAGLIA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MG_VERSION_MAJOR 0
#define MG_VERSION_MINOR 1
#define MG_VERSION_PATCH 0

/* security-critical backends are off until their paper constants are pinned */
#define MG_HAVE_MASKED 0
#define MG_HAVE_HAWK_ADAPTER 0

#if defined(MG_ENABLE_MASKED)
#error "masked maskaglia backend not implemented yet"
#endif

typedef enum {
	MG_OK = 0,
	MG_EINVAL = -1,
	MG_ERNG = -2,
	MG_EBOUND = -3,
	MG_EOVERFLOW = -4,
	MG_ENOTSUP = -5
} mg_err;

/* must fill all n bytes or return nonzero */
typedef int (*mg_read_fn)(void *ctx, uint8_t *dst, size_t n);

/* public so callers can keep it on the stack, no heap needed */
typedef struct {
	mg_read_fn read;
	void *ctx;
	uint64_t buf;
	uint64_t used;
	unsigned left;
	int err;
} mg_rng;

int mg_rng_init(mg_rng *r, mg_read_fn read, void *ctx);
int mg_rng_bits(mg_rng *r, unsigned n, uint32_t *x);
int mg_rng_u64(mg_rng *r, uint64_t *x);
int mg_rng_geom(mg_rng *r, uint32_t max, uint32_t *x);
uint64_t mg_rng_used(const mg_rng *r);

/* rng source and geometric-bound failures stay sticky until mg_rng_init */

const char *mg_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif
