#include "maskaglia.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static int randomcoins(void *context, uint8_t *output, size_t output_length)
{
	(void)context;
	(void)output;
	(void)output_length;

	return 0;
}

int main(void)
{
	int16_t sample;

	if (maskaglia_sample_ref(&sample, 0, 0, randomcoins, NULL) != 0)
	{
		puts("sampler not implemented");
	}

	return 0;
}
