#include "maskaglia.h"

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define SAMPLES 1000000u
#define LIMIT 32

typedef int (*sample_fn)(int16_t *sample, unsigned parameter_set, unsigned coset, maskaglia_randombytes_fn randombytes, void *random_context);

static uint64_t randomword(uint64_t *state)
{
	uint64_t value;

	*state += UINT64_C(0x9E3779B97F4A7C15);
	value = *state;
	value = (value ^ (value >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
	value = (value ^ (value >> 27)) * UINT64_C(0x94D049BB133111EB);

	return value ^ (value >> 31);
}

static int randomcoins(void *context, uint8_t *output, size_t output_length)
{
	uint64_t *state = context;
	size_t i, take;

	while (output_length > 0)
	{
		uint64_t word = randomword(state);

		take = output_length < 8 ? output_length : 8;
		for (i = 0; i < take; i++)
		{
			output[i] = (uint8_t)(word >> (8u * i));
		}
		output += take;
		output_length -= take;
	}

	return 0;
}

static int badcoins(void *context, uint8_t *output, size_t output_length)
{
	(void)context;
	(void)output;
	(void)output_length;

	return -1;
}

static double sigma(unsigned parameter_set)
{
	switch (parameter_set)
	{
		case 256:
			return 1.010;
		case 512:
			return 1.278;
		case 1024:
			return 1.299;
		default:
			return 0.0;
	}
}

static int same_coset(int sample, unsigned coset)
{
	unsigned value = (unsigned)(sample < 0 ? -sample : sample);

	return (value & 1u) == coset;
}

static double weight(int sample, double width)
{
	double value = (double)sample;

	return exp(-(value * value) / (8.0 * width * width));
}

static void test_errors(void)
{
	uint64_t state = UINT64_C(1);
	int16_t sample = 7;

	assert(maskaglia_sample_ref(&sample, 0, 0, randomcoins, &state) != 0);
	assert(cdt_sample_ref(NULL, 256, 0, randomcoins, &state) != 0);
	assert(cdt_sample_ref(&sample, 0, 0, randomcoins, &state) != 0);
	assert(cdt_sample_ref(&sample, 256, 2, randomcoins, &state) != 0);
	assert(cdt_sample_ref(&sample, 256, 0, NULL, &state) != 0);
	assert(cdt_sample_ref(&sample, 256, 0, badcoins, &state) != 0);
	assert(knuth_yao_sample_ref(NULL, 256, 0, randomcoins, &state) != 0);
	assert(knuth_yao_sample_ref(&sample, 0, 0, randomcoins, &state) != 0);
	assert(knuth_yao_sample_ref(&sample, 256, 2, randomcoins, &state) != 0);
	assert(knuth_yao_sample_ref(&sample, 256, 0, NULL, &state) != 0);
	assert(knuth_yao_sample_ref(&sample, 256, 0, badcoins, &state) != 0);
	assert(sample == 7);
}

static void test_sampler(const char *name, sample_fn sampler_fn, unsigned parameter_set, unsigned coset)
{
	uint64_t state = UINT64_C(0x53414D504C455246) ^ ((uint64_t)parameter_set << 8) ^ coset ^ (uint8_t)name[0];
	uint64_t frequency[2 * LIMIT + 1] = { 0 };
	double total = 0.0, squares = 0.0, norm = 0.0, expected_variance = 0.0, max_diff = 0.0, mean, variance;
	double width = sigma(parameter_set);
	unsigned i;
	int result, sample, x;

	for (x = -LIMIT; x <= LIMIT; x++)
	{
		if (same_coset(x, coset))
		{
			double value = weight(x, width);

			norm += value;
			expected_variance += (double)(x * x) * value;
		}
	}
	expected_variance /= norm;

	for (i = 0; i < SAMPLES; i++)
	{
		int16_t value;

		result = sampler_fn(&value, parameter_set, coset, randomcoins, &state);
		assert(result == 0);
		assert(same_coset(value, coset));
		assert(value >= -LIMIT && value <= LIMIT);
		sample = value;
		frequency[sample + LIMIT]++;
		total += sample;
		squares += (double)sample * sample;
	}

	mean = total / SAMPLES;
	variance = squares / SAMPLES - mean * mean;

	for (x = -LIMIT; x <= LIMIT; x++)
	{
		double expected = same_coset(x, coset) ? weight(x, width) / norm : 0.0;
		double found = (double)frequency[x + LIMIT] / SAMPLES;
		double diff = fabs(found - expected);

		if (diff > max_diff)
		{
			max_diff = diff;
		}
	}

	printf("%s %u coset %u: mean %.4f variance %.4f max diff %.5f\n", name, parameter_set, coset, mean, variance, max_diff);
	assert(fabs(mean) < 0.01);
	assert(fabs(variance - expected_variance) < 0.03);
	assert(max_diff < 0.0025);
}

int main(void)
{
	static const unsigned parameter_sets[] = { 256, 512, 1024 };
	size_t i;
	unsigned coset;

	test_errors();
	for (i = 0; i < sizeof parameter_sets / sizeof parameter_sets[0]; i++)
	{
		for (coset = 0; coset < 2; coset++)
		{
			test_sampler("cdt", cdt_sample_ref, parameter_sets[i], coset);
			test_sampler("knuth-yao", knuth_yao_sample_ref, parameter_sets[i], coset);
		}
	}

	return 0;
}
