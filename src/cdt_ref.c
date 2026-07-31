#include "maskaglia.h"

#include <stddef.h>
#include <stdint.h>

struct row
{
	uint64_t lo;
	uint16_t hi;
};

/* From HAWK's src/hawk_sign.c: https://github.com/hawk-sign/dev */
static const struct row table256[] = {
	{ UINT64_C(0x71FBD58485D45050), UINT16_C(0x4D70) }, { UINT64_C(0x1408A4B181C718B1), UINT16_C(0x268B) },
	{ UINT64_C(0x54114F1DC2FA7AC9), UINT16_C(0x0F80) }, { UINT64_C(0x614569CC54722DC9), UINT16_C(0x04FA) },
	{ UINT64_C(0x42F74ADDA0B5AE61), UINT16_C(0x0144) }, { UINT64_C(0x151C5CDCBAFF49A3), UINT16_C(0x0041) },
	{ UINT64_C(0x252E2152AB5D758B), UINT16_C(0x000A) }, { UINT64_C(0x23460C30AC398322), UINT16_C(0x0001) },
	{ UINT64_C(0x0FDE62196C1718FC), UINT16_C(0x0000) }, { UINT64_C(0x01355A8330C44097), UINT16_C(0x0000) },
	{ UINT64_C(0x00127325DDF8CEBA), UINT16_C(0x0000) }, { UINT64_C(0x0000DC8DE401FD12), UINT16_C(0x0000) },
	{ UINT64_C(0x000008100822C548), UINT16_C(0x0000) }, { UINT64_C(0x0000003B0FFB28F0), UINT16_C(0x0000) },
	{ UINT64_C(0x0000000152A6E9AE), UINT16_C(0x0000) }, { UINT64_C(0x0000000005EFCD99), UINT16_C(0x0000) },
	{ UINT64_C(0x000000000014DA4A), UINT16_C(0x0000) }, { UINT64_C(0x0000000000003953), UINT16_C(0x0000) },
	{ UINT64_C(0x000000000000007B), UINT16_C(0x0000) }, { UINT64_C(0x0000000000000000), UINT16_C(0x0000) }
};

static const struct row table512[] = {
	{ UINT64_C(0x0C27920A04F8F267), UINT16_C(0x580B) }, { UINT64_C(0x3C689D9213449DC9), UINT16_C(0x35F9) },
	{ UINT64_C(0x1C4FF17C204AA058), UINT16_C(0x1D34) }, { UINT64_C(0x7B908C81FCE3524F), UINT16_C(0x0DD7) },
	{ UINT64_C(0x5E63263BE0098FFD), UINT16_C(0x05B7) }, { UINT64_C(0x4EBEFD8FF4F07378), UINT16_C(0x020C) },
	{ UINT64_C(0x56AEDFB0876A3BD8), UINT16_C(0x00A2) }, { UINT64_C(0x4628BC6B23887196), UINT16_C(0x002B) },
	{ UINT64_C(0x061E21D588CC61CC), UINT16_C(0x000A) }, { UINT64_C(0x7F769211F07B326F), UINT16_C(0x0001) },
	{ UINT64_C(0x2BA568D92EEC18E7), UINT16_C(0x0000) }, { UINT64_C(0x0668F461693DFF8F), UINT16_C(0x0000) },
	{ UINT64_C(0x00CF0F8687D3B009), UINT16_C(0x0000) }, { UINT64_C(0x001670DB65964485), UINT16_C(0x0000) },
	{ UINT64_C(0x000216A0C344EB45), UINT16_C(0x0000) }, { UINT64_C(0x00002AB6E11C2552), UINT16_C(0x0000) },
	{ UINT64_C(0x000002EDF0B98A84), UINT16_C(0x0000) }, { UINT64_C(0x0000002C253C7E81), UINT16_C(0x0000) },
	{ UINT64_C(0x000000023AF3B2E7), UINT16_C(0x0000) }, { UINT64_C(0x0000000018C14ABF), UINT16_C(0x0000) },
	{ UINT64_C(0x0000000000EBCC6A), UINT16_C(0x0000) }, { UINT64_C(0x000000000007876E), UINT16_C(0x0000) },
	{ UINT64_C(0x00000000000034CF), UINT16_C(0x0000) }, { UINT64_C(0x000000000000013D), UINT16_C(0x0000) },
	{ UINT64_C(0x0000000000000006), UINT16_C(0x0000) }, { UINT64_C(0x0000000000000000), UINT16_C(0x0000) }
};

static const struct row table1024[] = {
	{ UINT64_C(0x3AAA2EB76504E560), UINT16_C(0x58B0) }, { UINT64_C(0x01AE2B17728DF2DE), UINT16_C(0x36FE) },
	{ UINT64_C(0x70E1C03E49BB683E), UINT16_C(0x1E3A) }, { UINT64_C(0x6A00B82C69624C93), UINT16_C(0x0EA0) },
	{ UINT64_C(0x55CDA662EF2D1C48), UINT16_C(0x0632) }, { UINT64_C(0x2685DB30348656A4), UINT16_C(0x024A) },
	{ UINT64_C(0x31E874B355421BB7), UINT16_C(0x00BC) }, { UINT64_C(0x430192770E205503), UINT16_C(0x0034) },
	{ UINT64_C(0x57C0676C029895A7), UINT16_C(0x000C) }, { UINT64_C(0x5353BD4091AA96DB), UINT16_C(0x0002) },
	{ UINT64_C(0x3D4D67696E51F820), UINT16_C(0x0000) }, { UINT64_C(0x09915A53D8667BEE), UINT16_C(0x0000) },
	{ UINT64_C(0x014A1A8A93F20738), UINT16_C(0x0000) }, { UINT64_C(0x0026670030160D5F), UINT16_C(0x0000) },
	{ UINT64_C(0x0003DAF47E8DFB21), UINT16_C(0x0000) }, { UINT64_C(0x0000557CD1C5F797), UINT16_C(0x0000) },
	{ UINT64_C(0x000006634617B3FF), UINT16_C(0x0000) }, { UINT64_C(0x0000006965E15B13), UINT16_C(0x0000) },
	{ UINT64_C(0x00000005DBEFB646), UINT16_C(0x0000) }, { UINT64_C(0x0000000047E9AB38), UINT16_C(0x0000) },
	{ UINT64_C(0x0000000002F93038), UINT16_C(0x0000) }, { UINT64_C(0x00000000001B2445), UINT16_C(0x0000) },
	{ UINT64_C(0x000000000000D5A7), UINT16_C(0x0000) }, { UINT64_C(0x00000000000005AA), UINT16_C(0x0000) },
	{ UINT64_C(0x0000000000000021), UINT16_C(0x0000) }, { UINT64_C(0x0000000000000000), UINT16_C(0x0000) }
};

static uint64_t read64(const uint8_t *buf)
{
	uint64_t value = 0;
	unsigned i;

	for (i = 0; i < 8; i++)
	{
		value |= (uint64_t)buf[i] << (8u * i);
	}

	return value;
}

static uint16_t read16(const uint8_t *buf)
{
	return (uint16_t)((uint16_t)buf[0] | (uint16_t)((uint16_t)buf[1] << 8));
}

int cdt_sample_ref(int16_t *sample, unsigned parameter_set, unsigned coset, maskaglia_randombytes_fn randombytes, void *random_context)
{
	const struct row *table;
	uint8_t coins[10];
	uint64_t lo;
	uint32_t hi, carry, count, sign;
	int32_t value;
	size_t i, length;

	if (sample == NULL || randombytes == NULL || coset > 1)
	{
		return -1;
	}

	switch (parameter_set)
	{
		case 256:
			table = table256;
			length = sizeof table256 / sizeof table256[0];
			break;
		case 512:
			table = table512;
			length = sizeof table512 / sizeof table512[0];
			break;
		case 1024:
			table = table1024;
			length = sizeof table1024 / sizeof table1024[0];
			break;
		default:
			return -1;
	}

	if (randombytes(random_context, coins, sizeof coins) != 0)
	{
		return -1;
	}

	lo = read64(coins);
	hi = (uint32_t)(read16(coins + 8) & UINT16_C(0x7FFF));
	sign = (uint32_t)(lo >> 63);
	lo &= UINT64_C(0x7FFFFFFFFFFFFFFF);
	count = 0;

	for (i = coset; i < length; i += 2)
	{
		carry = (uint32_t)((lo - table[i].lo) >> 63);
		count += (hi - (uint32_t)table[i].hi - carry) >> 31;
	}

	value = (int32_t)((count << 1) + coset);
	value *= 1 - 2 * (int32_t)sign;
	*sample = (int16_t)value;

	return 0;
}
