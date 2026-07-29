#ifndef MASKAGLIA_H
#define MASKAGLIA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*maskaglia_rng)(
	void *ctx,
	uint8_t *dst,
	size_t len
);

enum {
	MASKAGLIA_OK = 0,
	MASKAGLIA_ERR_NULL = -1,
	MASKAGLIA_ERR_PARAMETER_SET = -2,
	MASKAGLIA_ERR_COSET = -3,
	MASKAGLIA_ERR_RNG = -4
};

enum {
	MASKAGLIA_HAWK_256 = 256,
	MASKAGLIA_HAWK_512 = 512,
	MASKAGLIA_HAWK_1024 = 1024
};

/*
 * Generate one unmasked sample from the requested HAWK coset.
 *
 * parameter_set must be MASKAGLIA_HAWK_256, MASKAGLIA_HAWK_512, or
 * MASKAGLIA_HAWK_1024. coset must be 0 or 1. On success, *out belongs
 * to 2*Z + coset.
 *
 * This research reference is variable-time and uses floating-point
 * arithmetic. It is not a masked or production-ready implementation.
 */
int maskaglia_sample_ref(
	maskaglia_rng rng,
	void *rng_ctx,
	unsigned parameter_set,
	unsigned coset,
	int32_t *out
);

#ifdef __cplusplus
}
#endif

#endif
