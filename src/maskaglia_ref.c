#include "maskaglia.h"

#include <math.h>
#include <stdint.h>

#define MASKAGLIA_LN2 0.69314718055994530942
#define MASKAGLIA_INV_2_POW_53 1.11022302462515654042e-16
#define MASKAGLIA_MAX_ATTEMPTS 1048576u
#define MASKAGLIA_MAX_GEOMETRIC 1024u

/*
 * HAWK's signature sampler uses sigma_sign below, while its integer output
 * is scaled by two. Thus the output distribution has standard deviation
 * tau = 2*sigma_sign and is restricted to the requested parity coset.
 */
static int
hawk_tau_squared(unsigned parameter_set, double *tau_squared)
{
	double sigma;

	switch (parameter_set) {
	case MASKAGLIA_HAWK_256:
		sigma = 1.010;
		break;
	case MASKAGLIA_HAWK_512:
		sigma = 1.278;
		break;
	case MASKAGLIA_HAWK_1024:
		sigma = 1.299;
		break;
	default:
		return MASKAGLIA_ERR_PARAMETER_SET;
	}

	*tau_squared = 4.0 * sigma * sigma;
	return MASKAGLIA_OK;
}

static uint64_t
random_u64(maskaglia_rng rng, void *rng_ctx)
{
	uint8_t bytes[8];
	uint64_t value = 0;
	unsigned i;

	rng(rng_ctx, bytes, sizeof bytes);
	for (i = 0; i < 8; i++) {
		value |= (uint64_t)bytes[i] << (8u * i);
	}
	return value;
}

/*
 * Sample k with Pr[k = j] = 2^(-j-1) by counting trailing one bits.
 * The cap makes a broken RNG fail instead of looping forever. Reaching it
 * with a uniform RNG has probability at most 2^-1025.
 */
static int
sample_geometric_half(maskaglia_rng rng, void *rng_ctx, unsigned *k)
{
	unsigned value = 0;

	for (;;) {
		uint64_t bits = random_u64(rng, rng_ctx);
		unsigned run = 0;

		while ((bits & UINT64_C(1)) != 0) {
			run++;
			bits >>= 1;
		}
		if (value > MASKAGLIA_MAX_GEOMETRIC - run) {
			return MASKAGLIA_ERR_RNG;
		}
		value += run;
		if (run != 64u) {
			*k = value;
			return MASKAGLIA_OK;
		}
	}
}

static double
log_target_over_proposal(unsigned k, unsigned coset, double tau_squared)
{
	double magnitude = 2.0 * (double)k + (double)coset;

	return -(magnitude * magnitude) / (2.0 * tau_squared)
		+ (double)k * MASKAGLIA_LN2;
}

/*
 * Compute log(M), where
 *
 *   M = max_k rho(2*k + coset) / 2^-k.
 *
 * The logarithm is a concave quadratic in k, so checking the two integers
 * around its continuous maximum is sufficient.
 */
static double
log_envelope(unsigned coset, double tau_squared)
{
	double vertex = (MASKAGLIA_LN2 * tau_squared
		- 2.0 * (double)coset) / 4.0;
	unsigned k0;
	double best;
	double candidate;

	if (vertex <= 0.0) {
		k0 = 0;
	} else {
		k0 = (unsigned)floor(vertex);
	}

	best = log_target_over_proposal(k0, coset, tau_squared);
	candidate = log_target_over_proposal(k0 + 1u, coset, tau_squared);
	if (candidate > best) {
		best = candidate;
	}
	return best;
}

int
maskaglia_sample_ref(
	maskaglia_rng rng,
	void *rng_ctx,
	unsigned parameter_set,
	unsigned coset,
	int32_t *out)
{
	double tau_squared;
	double envelope;
	unsigned attempt;
	int result;

	if (rng == NULL || out == NULL) {
		return MASKAGLIA_ERR_NULL;
	}
	if (coset > 1u) {
		return MASKAGLIA_ERR_COSET;
	}
	result = hawk_tau_squared(parameter_set, &tau_squared);
	if (result != MASKAGLIA_OK) {
		return result;
	}
	envelope = log_envelope(coset, tau_squared);

	for (attempt = 0; attempt < MASKAGLIA_MAX_ATTEMPTS; attempt++) {
		unsigned k;
		unsigned magnitude;
		uint64_t random_bits;
		unsigned negative;
		double uniform;
		double log_acceptance;
		double acceptance;

		result = sample_geometric_half(rng, rng_ctx, &k);
		if (result != MASKAGLIA_OK) {
			return result;
		}

		magnitude = 2u * k + coset;
		random_bits = random_u64(rng, rng_ctx);
		negative = (unsigned)(random_bits & UINT64_C(1));

		/*
		 * Zero has only one representation. Reject its negative-sign
		 * representation so every integer in the coset starts with the
		 * same proposal mass.
		 */
		if (magnitude == 0u && negative != 0u) {
			continue;
		}

		uniform = (double)(random_bits >> 11)
			* MASKAGLIA_INV_2_POW_53;
		log_acceptance = log_target_over_proposal(
			k, coset, tau_squared) - envelope;
		acceptance = log_acceptance >= 0.0
			? 1.0 : exp(log_acceptance);

		if (uniform < acceptance) {
			int32_t sample = (int32_t)magnitude;
			*out = negative != 0u ? -sample : sample;
			return MASKAGLIA_OK;
		}
	}

	return MASKAGLIA_ERR_RNG;
}
