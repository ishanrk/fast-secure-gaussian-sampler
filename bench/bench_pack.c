#include "maskaglia_bitslice.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

int
main(void)
{
	enum { n = 1000000 };
	uint16_t x[MG_LANES];
	uint16_t y[MG_LANES];
	uint32_t p[MG_PLANES16];
	uint32_t sum = 0;
	clock_t t0;
	clock_t t1;
	int i;

	for (i = 0; i < MG_LANES; i ++) {
		x[i] = (uint16_t)(i * 7919);
	}
	t0 = clock();
	for (i = 0; i < n; i ++) {
		x[i & 31] ^= (uint16_t)i;
		mg_pack16(p, x);
		mg_unpack16(y, p);
		sum ^= y[(i + 7) & 31];
	}
	t1 = clock();
	printf("pack+unpack: %.1f ns/32 lanes, sum=%u\n",
		1.0e9 * (double)(t1 - t0) / CLOCKS_PER_SEC / n, sum);
	return 0;
}
