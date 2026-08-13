#include "maskaglia.h"

#include <stdint.h>

#include "test_util.h"

static int
test_bits(void)
{
	static const uint8_t b[8] = {
		0xa5, 0x5a, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06
	};
	byte_src s = { b, sizeof b, 0 };
	mg_rng r;
	uint32_t x;

	check(mg_rng_init(&r, byte_read, &s) == MG_OK);
	check(mg_rng_bits(&r, 4, &x) == MG_OK && x == 5);
	check(mg_rng_bits(&r, 4, &x) == MG_OK && x == 10);
	check(mg_rng_bits(&r, 8, &x) == MG_OK && x == 0x5a);
	check(mg_rng_bits(&r, 0, &x) == MG_OK && x == 0);
	check(mg_rng_used(&r) == 16);
	check(mg_rng_bits(&r, 33, &x) == MG_EINVAL);
	return 0;
}

static int
test_u64(void)
{
	static const uint8_t b[16] = {
		0, 1, 2, 3, 4, 5, 6, 7,
		15, 9, 10, 11, 12, 13, 14, 15
	};
	byte_src s = { b, 8, 0 };
	mg_rng r;
	uint64_t x;
	uint32_t q;

	check(mg_rng_init(&r, byte_read, &s) == MG_OK);
	check(mg_rng_u64(&r, &x) == MG_OK);
	check(x == UINT64_C(0x0706050403020100));
	check(mg_rng_used(&r) == 64);

	/* same check across a non-byte-aligned refill */
	s.n = sizeof b;
	s.off = 0;
	check(mg_rng_init(&r, byte_read, &s) == MG_OK);
	check(mg_rng_bits(&r, 3, &q) == MG_OK && q == 0);
	check(mg_rng_u64(&r, &x) == MG_OK);
	check(x == ((UINT64_C(0x0706050403020100) >> 3)
		| (UINT64_C(0x0f0e0d0c0b0a090f) << 61)));
	check(mg_rng_used(&r) == 67);
	return 0;
}

static int
test_geom(void)
{
	static const uint8_t a[8] = { 0x28, 0, 0, 0, 0, 0, 0, 0 };
	static const uint8_t b[24] = {
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		1, 0, 0, 0, 0, 0, 0, 0
	};
	byte_src sa = { a, sizeof a, 0 };
	byte_src sb = { b, sizeof b, 0 };
	mg_rng r;
	uint32_t x;

	check(mg_rng_init(&r, byte_read, &sa) == MG_OK);
	check(mg_rng_geom(&r, 8, &x) == MG_OK && x == 3);
	check(mg_rng_geom(&r, 8, &x) == MG_OK && x == 1);
	check(mg_rng_used(&r) == 6);

	check(mg_rng_init(&r, byte_read, &sb) == MG_OK);
	check(mg_rng_geom(&r, 128, &x) == MG_OK && x == 128);
	check(mg_rng_used(&r) == 129);

	sb.off = 0;
	check(mg_rng_init(&r, byte_read, &sb) == MG_OK);
	check(mg_rng_geom(&r, 63, &x) == MG_EBOUND);
	check(mg_rng_used(&r) == 64);
	check(mg_rng_bits(&r, 1, &x) == MG_EBOUND);
	check(mg_rng_used(&r) == 64);

	{
		static const uint8_t c[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };
		byte_src sc = { c, sizeof c, 0 };
		check(mg_rng_init(&r, byte_read, &sc) == MG_OK);
		check(mg_rng_geom(&r, 0, &x) == MG_OK && x == 0);
	}
	{
		static const uint8_t c[8] = { 2, 0, 0, 0, 0, 0, 0, 0 };
		byte_src sc = { c, sizeof c, 0 };
		check(mg_rng_init(&r, byte_read, &sc) == MG_OK);
		check(mg_rng_geom(&r, 0, &x) == MG_EBOUND);
		check(mg_rng_geom(&r, 8, &x) == MG_EBOUND);
	}
	return 0;
}

static int
test_fail(void)
{
	static const uint8_t b[1] = { 1 };
	byte_src s = { b, sizeof b, 0 };
	mg_rng r;
	uint32_t x = 7;

	check(mg_rng_init(NULL, byte_read, &s) == MG_EINVAL);
	check(mg_rng_init(&r, NULL, &s) == MG_EINVAL);
	check(mg_rng_init(&r, byte_read, &s) == MG_OK);
	check(mg_rng_bits(&r, 1, &x) == MG_ERNG && x == 0);
	check(mg_rng_bits(&r, 1, &x) == MG_ERNG && x == 0);
	check(mg_strerror(MG_ERNG) != NULL);

	{
		static const uint8_t c[8] = {
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
		};
		byte_src sc = { c, sizeof c, 0 };
		check(mg_rng_init(&r, byte_read, &sc) == MG_OK);
		check(mg_rng_bits(&r, 31, &x) == MG_OK);
		check(mg_rng_bits(&r, 32, &x) == MG_OK);
		check(mg_rng_bits(&r, 2, &x) == MG_ERNG && x == 0);
		check(mg_rng_used(&r) == 64);
	}
	return 0;
}

int
main(void)
{
	check(test_bits() == 0);
	check(test_u64() == 0);
	check(test_geom() == 0);
	check(test_fail() == 0);
	puts("test_rng: ok");
	return 0;
}
