#include "maskaglia_bitslice.h"

#include <stddef.h>

/* hacker's delight style 32x32 transpose */
static void
tr32(uint32_t a[32])
{
	unsigned j = 16;
	uint32_t m = UINT32_C(0x0000ffff);

	while (j != 0) {
		unsigned k = 0;
		while (k < 32) {
			uint32_t t = (a[k] ^ (a[k + j] >> j)) & m;
			a[k] ^= t;
			a[k + j] ^= t << j;
			k = (k + j + 1) & ~j;
		}
		j >>= 1;
		if (j != 0) {
			m ^= m << j;
		}
	}
}

void
mg_pack16(uint32_t out[MG_PLANES16], const uint16_t in[MG_LANES])
{
	uint32_t a[32];
	size_t i;

	for (i = 0; i < 32; i ++) {
		a[31 - i] = in[i];
	}
	tr32(a);
	for (i = 0; i < 16; i ++) {
		out[i] = a[31 - i];
	}
}

void
mg_unpack16(uint16_t out[MG_LANES], const uint32_t in[MG_PLANES16])
{
	uint32_t a[32] = { 0 };
	size_t i;

	for (i = 0; i < 16; i ++) {
		a[31 - i] = in[i];
	}
	tr32(a);
	for (i = 0; i < 32; i ++) {
		out[i] = (uint16_t)a[31 - i];
	}
}
