#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "internal.h"
#include "pqsamp_hawk.h"
#include "test.h"

static int adapter(unsigned shares)
{
  enum
  {
    SAMPLE_COUNT = 35
  };
  test_rng coin_source = test_rng_make(UINT64_C(0x6a09e667f3bcc909) + shares);
  test_rng mask_source = test_rng_make(UINT64_C(0xbb67ae8584caa73b) + shares);
  pqsamp_rng coins;
  pqsamp_rng masks;
  pqsamp_masked_bit center[SAMPLE_COUNT];
  pqsamp_masked_i16 out[SAMPLE_COUNT];
  pqsamp_masked_i16 zero[SAMPLE_COUNT];
  pqsamp_masked_i16 half[SAMPLE_COUNT];
  test_rng reference_coin_source;
  test_rng reference_mask_source;
  pqsamp_rng reference_coins;
  pqsamp_rng reference_masks;
  unsigned i;

  PQSAMP_CHECK(pqsamp_rng_init(&coins, test_randombytes, &coin_source) ==
               PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_rng_init(&masks, test_randombytes, &mask_source) ==
               PQSAMP_OK);
  for (i = 0; i < SAMPLE_COUNT; i++)
  {
    unsigned share;
    uint8_t value = (uint8_t)(i & 1U);

    center[i].share[0] = value;
    for (share = 1; share < shares; share++)
    {
      uint32_t mask;

      PQSAMP_CHECK(pqsamp_rng_bits(&masks, 1U, &mask) == PQSAMP_OK);
      center[i].share[share] = (uint8_t)mask;
      center[i].share[0] ^= (uint8_t)mask;
    }
    for (; share < PQSAMP_MAX_SHARES; share++)
    {
      center[i].share[share] = 0;
    }
  }

  reference_coin_source = coin_source;
  reference_mask_source = mask_source;
  reference_coins = coins;
  reference_masks = masks;
  reference_coins.context = &reference_coin_source;
  reference_masks.context = &reference_mask_source;

  PQSAMP_CHECK(pqsamp_hawk_sample_masked(out, SAMPLE_COUNT, PQSAMP_PROFILE_S3_2,
                                         center, shares, &coins,
                                         &masks) == PQSAMP_OK);
  for (i = 0; i < SAMPLE_COUNT;)
  {
    size_t block = SAMPLE_COUNT - i;

    if (block > PQSAMP_LANES)
    {
      block = PQSAMP_LANES;
    }
    PQSAMP_CHECK(pqsamp_sample_masked(&zero[i], block, PQSAMP_PROFILE_S3_2,
                                      PQSAMP_CENTER_ZERO, shares,
                                      &reference_coins, &reference_masks,
                                      NULL) == PQSAMP_OK);
    PQSAMP_CHECK(pqsamp_sample_masked(&half[i], block, PQSAMP_PROFILE_S3_2,
                                      PQSAMP_CENTER_HALF, shares,
                                      &reference_coins, &reference_masks,
                                      NULL) == PQSAMP_OK);
    i += (unsigned)block;
  }
  PQSAMP_CHECK(coins.bits_used == reference_coins.bits_used);
  for (i = 0; i < SAMPLE_COUNT; i++)
  {
    int16_t value = test_reconstruct(&out[i], shares);
    int16_t selected = (i & 1U) == 0U ? test_reconstruct(&zero[i], shares)
                                      : test_reconstruct(&half[i], shares);
    int16_t expected = (int16_t)(2 * selected - (int16_t)(i & 1U));

    PQSAMP_CHECK(value == expected);
    PQSAMP_CHECK(value >= -27 && value <= 26);
    PQSAMP_CHECK(((uint16_t)value & 1U) == (i & 1U));
  }
  return 0;
}

int main(void)
{
  unsigned shares;

  for (shares = 2; shares <= PQSAMP_MAX_SHARES; shares++)
  {
    PQSAMP_CHECK(adapter(shares) == 0);
  }
  puts("adapter: ok");
  return 0;
}
