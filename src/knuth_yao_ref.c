#include "maskaglia.h"

#include <stddef.h>
#include <stdint.h>

/* Generated from HAWK's src/hawk_sign.c: https://github.com/hawk-sign/dev */
static const uint64_t table256_0[] = {
	UINT64_C(0x651E3810A9EDE8B8), UINT64_C(0x3DF03BD50CCD85B3),
	UINT64_C(0x0E3C223408804489), UINT64_C(0x013A3B925315EAB0),
	UINT64_C(0x000A2A9F7E727E8C), UINT64_C(0x00001F97DDE71C3C),
	UINT64_C(0x00000024D62BABAC), UINT64_C(0x00000000101D6AF7),
	UINT64_C(0x000000000002A524), UINT64_C(0x0000000000000029)
};

static const uint64_t table256_1[] = {
	UINT64_C(0x5974D7EEB69CFC77), UINT64_C(0x2190658675CA5AA9),
	UINT64_C(0x04B9985219DF32E5), UINT64_C(0x003FE3ACA1581D8B),
	UINT64_C(0x00014421635AF6EA), UINT64_C(0x00000268FBEA9984),
	UINT64_C(0x00000001B8A5A80D), UINT64_C(0x0000000000761416),
	UINT64_C(0x0000000000000BDF)
};

static const uint64_t table512_0[] = {
	UINT64_C(0x4FE9CF61B7D7EC2A), UINT64_C(0x3AD6DFAF411BC95C),
	UINT64_C(0x177C7BD996808082), UINT64_C(0x05150F688D16B13E),
	UINT64_C(0x0098A1217BB5FD3B), UINT64_C(0x0009B4F171F8B3C0),
	UINT64_C(0x000055ACB2A54E30), UINT64_C(0x00000199F1CB891D),
	UINT64_C(0x000000042765A516), UINT64_C(0x0000000005D76B8B),
	UINT64_C(0x000000000004740F), UINT64_C(0x00000000000001D7)
};

static const uint64_t table512_1[] = {
	UINT64_C(0x4A06872EC4DBD97B), UINT64_C(0x282181B022202CC2),
	UINT64_C(0x0BCB59A31DE40FE5), UINT64_C(0x01E1112C8249A2D0),
	UINT64_C(0x00298D6454B2661A), UINT64_C(0x0001F21B3B610E7A),
	UINT64_C(0x00000CA5070C074F), UINT64_C(0x0000002C8C4908F4),
	UINT64_C(0x00000000551577BF), UINT64_C(0x00000000005818F6),
	UINT64_C(0x0000000000003173), UINT64_C(0x000000000000000F)
};

static const uint64_t table1024_0[] = {
	UINT64_C(0x4E9F155745226BF8), UINT64_C(0x3A759390DCF23692),
	UINT64_C(0x1808362833B6B51C), UINT64_C(0x057647CA635F33D6),
	UINT64_C(0x00AFB4501A8EA553), UINT64_C(0x000C34E60005288D),
	UINT64_C(0x0000780699BDB4BF), UINT64_C(0x0000028C7F2C2AC8),
	UINT64_C(0x00000007A92270EC), UINT64_C(0x000000000CBAD44F),
	UINT64_C(0x00000000000BB1ED), UINT64_C(0x00000000000005F0),
	UINT64_C(0x0000000000000001)
};

static const uint64_t table1024_1[] = {
	UINT64_C(0x4901FCA3A9D11AE8), UINT64_C(0x285D2F5AE5D61257),
	UINT64_C(0x0C5686F5B9F869B7), UINT64_C(0x0215C70891724CCC),
	UINT64_C(0x0031DF5BAA6CF8EB), UINT64_C(0x00029384C5D97288),
	UINT64_C(0x000012D5E6A750A0), UINT64_C(0x0000004C2306BCA0),
	UINT64_C(0x00000000AA26D7C9), UINT64_C(0x0000000000D23BEF),
	UINT64_C(0x0000000000008F9D), UINT64_C(0x0000000000000036)
};

int knuth_yao_sample_ref(int16_t *sample, unsigned parameter_set, unsigned coset, maskaglia_randombytes_fn randombytes, void *random_context)
{
	const uint64_t *table;
	uint8_t coins[8];
	size_t i, length;
	unsigned bit, digit;
	int distance, value;

	if (sample == NULL || randombytes == NULL || coset > 1)
	{
		return -1;
	}

	switch (parameter_set)
	{
		case 256:
			table = coset == 0 ? table256_0 : table256_1;
			length = coset == 0 ? sizeof table256_0 / sizeof table256_0[0] : sizeof table256_1 / sizeof table256_1[0];
			break;
		case 512:
			table = coset == 0 ? table512_0 : table512_1;
			length = coset == 0 ? sizeof table512_0 / sizeof table512_0[0] : sizeof table512_1 / sizeof table512_1[0];
			break;
		case 1024:
			table = coset == 0 ? table1024_0 : table1024_1;
			length = coset == 0 ? sizeof table1024_0 / sizeof table1024_0[0] : sizeof table1024_1 / sizeof table1024_1[0];
			break;
		default:
			return -1;
	}

	if (randombytes(random_context, coins, sizeof coins) != 0)
	{
		return -1;
	}

	distance = 0;
	for (bit = 0; bit < 64; bit++)
	{
		distance = (distance << 1) + ((coins[bit >> 3] >> (bit & 7)) & 1);
		for (i = 0; i < length; i++)
		{
			digit = (unsigned)((table[i] >> (63u - bit)) & 1u);
			distance -= (int)digit;
			value = (int)(2 * i + coset);
			if (distance < 0)
			{
				*sample = (int16_t)value;
				return 0;
			}
			if (value != 0)
			{
				distance -= (int)digit;
				if (distance < 0)
				{
					*sample = (int16_t)-value;
					return 0;
				}
			}
		}
	}

	return -1;
}
