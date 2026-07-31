#include "maskaglia.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static int randomcoins(void *context, uint8_t *output, size_t output_length)
{
	(void)context;
	(void)output;
	(void)output_length;

	return 0;
}

int main(void)
{
	int16_t sample = 7;

	assert(maskaglia_sample_ref(&sample, 0, 0, randomcoins, NULL) != 0);
	assert(sample == 7);

	return 0;
}
