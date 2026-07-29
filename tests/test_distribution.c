#include "maskaglia.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NUM_SAMPLES 1000000u
#define BIN_LIMIT 32
#define NORMALIZATION_LIMIT 64
#define MAX_ABS_DIFF 0.0025

void maskaglia_test_rng(void *ctx, uint8_t *dst, size_t len);

static double
sigma_sign(unsigned parameter_set)
{
	switch (parameter_set) {
	case MASKAGLIA_HAWK_256:
		return 1.010;
	case MASKAGLIA_HAWK_512:
		return 1.278;
	case MASKAGLIA_HAWK_1024:
		return 1.299;
	default:
		assert(0 && "unsupported HAWK parameter set");
		return 0.0;
	}
}

static int
has_coset(int x, unsigned coset)
{
	unsigned magnitude = (unsigned)(x < 0 ? -x : x);
	return magnitude % 2u == coset;
}

static double
rho(int x, double tau)
{
	double value = (double)x;
	return exp(-(value * value) / (2.0 * tau * tau));
}

static void
test_errors(void)
{
	uint64_t state = UINT64_C(1);
	int32_t out;

	assert(maskaglia_sample_ref(NULL, &state, MASKAGLIA_HAWK_256,
		0, &out) == MASKAGLIA_ERR_NULL);
	assert(maskaglia_sample_ref(maskaglia_test_rng, &state,
		MASKAGLIA_HAWK_256, 0, NULL) == MASKAGLIA_ERR_NULL);
	assert(maskaglia_sample_ref(maskaglia_test_rng, &state,
		999, 0, &out) == MASKAGLIA_ERR_PARAMETER_SET);
	assert(maskaglia_sample_ref(maskaglia_test_rng, &state,
		MASKAGLIA_HAWK_256, 2, &out) == MASKAGLIA_ERR_COSET);
}

static void
test_one_distribution(unsigned parameter_set, unsigned coset)
{
	uint64_t state = UINT64_C(0x4D41534B41474C49)
		^ ((uint64_t)parameter_set << 8) ^ (uint64_t)coset;
	uint64_t frequencies[2 * BIN_LIMIT + 1];
	uint64_t outside = 0;
	double tau = 2.0 * sigma_sign(parameter_set);
	double normalization = 0.0;
	double max_abs_diff = 0.0;
	unsigned i;
	int x;

	memset(frequencies, 0, sizeof frequencies);

	for (x = -NORMALIZATION_LIMIT; x <= NORMALIZATION_LIMIT; x++) {
		if (has_coset(x, coset)) {
			normalization += rho(x, tau);
		}
	}

	for (i = 0; i < NUM_SAMPLES; i++) {
		int32_t sample;
		int result = maskaglia_sample_ref(maskaglia_test_rng, &state,
			parameter_set, coset, &sample);

		assert(result == MASKAGLIA_OK);
		assert(has_coset(sample, coset));
		if (sample < -BIN_LIMIT || sample > BIN_LIMIT) {
			outside++;
		} else {
			frequencies[sample + BIN_LIMIT]++;
		}
	}

	for (x = -BIN_LIMIT; x <= BIN_LIMIT; x++) {
		double expected = has_coset(x, coset)
			? rho(x, tau) / normalization : 0.0;
		double observed = (double)frequencies[x + BIN_LIMIT]
			/ (double)NUM_SAMPLES;
		double difference = fabs(observed - expected);

		if (difference > max_abs_diff) {
			max_abs_diff = difference;
		}
	}

	printf("HAWK-%u coset %u: max abs.diff. %.6f, outside bins %llu\n",
		parameter_set, coset, max_abs_diff,
		(unsigned long long)outside);
	assert(outside == 0);
	assert(max_abs_diff < MAX_ABS_DIFF);
}

int
main(void)
{
	static const unsigned parameter_sets[] = {
		MASKAGLIA_HAWK_256,
		MASKAGLIA_HAWK_512,
		MASKAGLIA_HAWK_1024
	};
	size_t i;
	unsigned coset;

	test_errors();
	for (i = 0; i < sizeof parameter_sets / sizeof parameter_sets[0]; i++) {
		for (coset = 0; coset < 2; coset++) {
			test_one_distribution(parameter_sets[i], coset);
		}
	}
	return 0;
}
