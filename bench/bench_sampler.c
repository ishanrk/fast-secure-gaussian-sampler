#include "maskaglia.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define BENCH_SAMPLES 1000000u

void maskaglia_test_rng(void *ctx, uint8_t *dst, size_t len);

static void
bench_one(unsigned parameter_set, unsigned coset)
{
	uint64_t state = UINT64_C(0x42454E43484D4153)
		^ ((uint64_t)parameter_set << 8) ^ (uint64_t)coset;
	int64_t checksum = 0;
	clock_t start = clock();
	clock_t end;
	unsigned i;

	for (i = 0; i < BENCH_SAMPLES; i++) {
		int32_t sample;
		int result = maskaglia_sample_ref(maskaglia_test_rng, &state,
			parameter_set, coset, &sample);

		assert(result == MASKAGLIA_OK);
		checksum += sample;
	}
	end = clock();

	{
		double seconds = (double)(end - start) / (double)CLOCKS_PER_SEC;
		double nanoseconds = seconds * 1.0e9 / (double)BENCH_SAMPLES;

		printf("HAWK-%-4u coset %u: %8.2f ns/sample"
			" (%lld checksum)\n",
			parameter_set, coset, nanoseconds,
			(long long)checksum);
	}
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

	for (i = 0; i < sizeof parameter_sets / sizeof parameter_sets[0]; i++) {
		for (coset = 0; coset < 2; coset++) {
			bench_one(parameter_sets[i], coset);
		}
	}
	return 0;
}
