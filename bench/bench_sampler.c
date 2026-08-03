#include "maskaglia.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define SAMPLES 1000000u

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

static void bench_one(const char *name, sample_fn sampler_fn, unsigned parameter_set, unsigned coset)
{
	uint64_t state = UINT64_C(0x42454E434853414D) ^ ((uint64_t)parameter_set << 8) ^ coset ^ (uint8_t)name[0];
	int64_t total = 0;
	clock_t start, end;
	unsigned i;

	start = clock();
	for (i = 0; i < SAMPLES; i++)
	{
		int16_t sample;

		if (sampler_fn(&sample, parameter_set, coset, randomcoins, &state) != 0)
		{
			puts("sampler failed");
			return;
		}
		total += sample;
	}
	end = clock();

	printf("%s %u coset %u: %.2f ns/sample (%lld)\n", name, parameter_set, coset, (double)(end - start) * 1.0e9 / CLOCKS_PER_SEC / SAMPLES, (long long)total);
}

int main(void)
{
	static const unsigned parameter_sets[] = { 256, 512, 1024 };
	size_t i;
	unsigned coset;

	for (i = 0; i < sizeof parameter_sets / sizeof parameter_sets[0]; i++)
	{
		for (coset = 0; coset < 2; coset++)
		{
			bench_one("cdt", cdt_sample_ref, parameter_sets[i], coset);
			bench_one("knuth-yao", knuth_yao_sample_ref, parameter_sets[i], coset);
		}
	}

	return 0;
}
