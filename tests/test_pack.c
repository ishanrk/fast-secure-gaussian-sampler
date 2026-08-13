#include "maskaglia_bitslice.h"

#include <stdint.h>

#include "test_util.h"

int
main(void)
{
	uint16_t x[MG_LANES];
	uint16_t y[MG_LANES];
	uint32_t p[MG_PLANES16];
	uint32_t q[MG_PLANES16] = { 0 };
	unsigned i;
	unsigned j;
	unsigned k;

	for (i = 0; i < MG_LANES; i ++) {
		x[i] = (uint16_t)((i * 17777U) ^ 0xacedU);
	}
	mg_pack16(p, x);
	for (j = 0; j < MG_PLANES16; j ++) {
		for (i = 0; i < MG_LANES; i ++) {
			q[j] |= (((uint32_t)x[i] >> j) & UINT32_C(1)) << i;
		}
		check(p[j] == q[j]);
	}
	mg_unpack16(y, p);
	for (i = 0; i < MG_LANES; i ++) {
		check(y[i] == x[i]);
	}

	for (i = 0; i < MG_LANES; i ++) {
		x[i] = (uint16_t)i;
	}
	mg_pack16(p, x);
	check(p[0] == UINT32_C(0xaaaaaaaa));
	check(p[1] == UINT32_C(0xcccccccc));
	check(p[2] == UINT32_C(0xf0f0f0f0));
	check(p[3] == UINT32_C(0xff00ff00));
	check(p[4] == UINT32_C(0xffff0000));

	/* complete one-hot basis for the 32 by 16 mapping */
	for (i = 0; i < MG_LANES; i ++) {
		for (j = 0; j < MG_PLANES16; j ++) {
			for (k = 0; k < MG_LANES; k ++) {
				x[k] = 0;
			}
			x[i] = (uint16_t)(UINT32_C(1) << j);
			mg_pack16(p, x);
			for (k = 0; k < MG_PLANES16; k ++) {
				uint32_t want = k == j ? UINT32_C(1) << i : 0;
				check(p[k] == want);
			}
			mg_unpack16(y, p);
			for (k = 0; k < MG_LANES; k ++) {
				check(y[k] == x[k]);
			}
		}
	}
	puts("test_pack: ok");
	return 0;
}
