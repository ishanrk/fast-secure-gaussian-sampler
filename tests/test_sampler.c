#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "pqsamp.h"
#include "test.h"

static int pqsamp_test_vectors(void)
{
  static const int16_t expected[2][2][16] = {
      {{-1, 0, 0, -1, -1, 0, 0, -1, 2, 0, 0, 0, -1, 0, -2, 3},
       {2, 1, -1, 0, 1, 2, 0, 1, 3, -1, 3, 2, 0, -1, -1, 1}},
      {{1, 0, 1, 0, -1, 2, 1, -1, 0, 0, -2, -1, 0, -1, 0, 0},
       {-1, 1, 3, 0, 1, 1, 1, -1, 1, 1, 1, 2, 0, 0, -1, 1}}};
  unsigned profile;

  for (profile = 0; profile < 2U; profile++)
  {
    unsigned center;

    for (center = 0; center < 2U; center++)
    {
      pqsamp_test_rng source = pqsamp_test_rng_make(
          UINT64_C(0x123456789abcdef0) + profile * 17U + center);
      pqsamp_rng rng;
      int16_t sample[16];
      unsigned i;

      PQSAMP_CHECK(pqsamp_rng_init(&rng, pqsamp_test_randombytes, &source) ==
                   PQSAMP_OK);
      PQSAMP_CHECK(pqsamp_generate(sample, 16,
                                   pqsamp_params_get((pqsamp_profile)profile,
                                                     (pqsamp_center)center),
                                   &rng, NULL) == PQSAMP_OK);
      for (i = 0; i < 16U; i++)
      {
        PQSAMP_CHECK(sample[i] == expected[profile][center][i]);
      }
    }
  }
  return 0;
}

static int pqsamp_test_plain_profile(pqsamp_profile profile,
                                     pqsamp_center center, double mean,
                                     double variance)
{
  enum
  {
    SAMPLE_COUNT = 20000
  };
  const pqsamp_params *params = pqsamp_params_get(profile, center);
  pqsamp_test_rng source = pqsamp_test_rng_make(UINT64_C(0x8a5cd789635d2dff) +
                                                profile * 7U + center);
  pqsamp_rng rng;
  int16_t sample[SAMPLE_COUNT];
  double sum = 0.0;
  double squares = 0.0;
  unsigned i;

  PQSAMP_CHECK(params != NULL);
  PQSAMP_CHECK(pqsamp_params_check(params) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_rng_init(&rng, pqsamp_test_randombytes, &source) ==
               PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_generate(sample, SAMPLE_COUNT, params, &rng, NULL) ==
               PQSAMP_OK);
  for (i = 0; i < SAMPLE_COUNT; i++)
  {
    double value = sample[i];

    PQSAMP_CHECK(sample[i] >= -13 && sample[i] <= 13);
    sum += value;
    squares += value * value;
  }
  sum /= SAMPLE_COUNT;
  squares = squares / SAMPLE_COUNT - sum * sum;
  PQSAMP_CHECK(fabs(sum - mean) < 0.05);
  PQSAMP_CHECK(fabs(squares - variance) < 0.10);
  return 0;
}

static int pqsamp_test_masked(unsigned shares)
{
  const pqsamp_params *params =
      pqsamp_params_get(PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_HALF);
  pqsamp_test_rng coin_source =
      pqsamp_test_rng_make(UINT64_C(0xd1b54a32d192ed03) + shares);
  pqsamp_test_rng mask_source =
      pqsamp_test_rng_make(UINT64_C(0x94d049bb133111eb) + shares);
  pqsamp_rng coins;
  pqsamp_rng masks;
  pqsamp_stats stats;
  pqsamp_masked_i16 sample[64];
  unsigned i;

  PQSAMP_CHECK(pqsamp_rng_init(&coins, pqsamp_test_randombytes, &coin_source) ==
               PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_rng_init(&masks, pqsamp_test_randombytes, &mask_source) ==
               PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_generate_masked(sample, 64, params, shares, &coins,
                                      &masks, &stats) == PQSAMP_OK);
  PQSAMP_CHECK(stats.candidates == 256U);
  PQSAMP_CHECK(stats.candidate_batches == 8U);
  PQSAMP_CHECK(stats.sec_and_calls != 0U);
  PQSAMP_CHECK(stats.mask_bits != 0U);
  for (i = 0; i < 64U; i++)
  {
    int16_t value = pqsamp_reconstruct(&sample[i], shares);

    PQSAMP_CHECK(value >= -13 && value <= 13);
  }
  return 0;
}

static int pqsamp_test_masked_differential(void)
{
  enum
  {
    SAMPLE_COUNT = 33
  };
  unsigned profile;

  for (profile = 0; profile < 2U; profile++)
  {
    unsigned center;

    for (center = 0; center < 2U; center++)
    {
      const pqsamp_params *params =
          pqsamp_params_get((pqsamp_profile)profile, (pqsamp_center)center);
      uint64_t coin_seed =
          UINT64_C(0x243f6a8885a308d3) + profile * 11U + center;
      pqsamp_test_rng reference_source = pqsamp_test_rng_make(coin_seed);
      pqsamp_rng reference_coins;
      pqsamp_masked_i16 reference[SAMPLE_COUNT];
      unsigned shares;

      PQSAMP_CHECK(pqsamp_rng_init(&reference_coins, pqsamp_test_randombytes,
                                   &reference_source) == PQSAMP_OK);
      PQSAMP_CHECK(pqsamp_generate_masked(reference, SAMPLE_COUNT, params, 1U,
                                          &reference_coins, NULL,
                                          NULL) == PQSAMP_OK);
      for (shares = 2; shares <= PQSAMP_MAX_SHARES; shares++)
      {
        pqsamp_test_rng coin_source = pqsamp_test_rng_make(coin_seed);
        pqsamp_test_rng mask_source =
            pqsamp_test_rng_make(UINT64_C(0x13198a2e03707344) + profile * 31U +
                                 center * 7U + shares);
        pqsamp_rng coins;
        pqsamp_rng masks;
        pqsamp_masked_i16 sample[SAMPLE_COUNT];
        unsigned i;

        PQSAMP_CHECK(pqsamp_rng_init(&coins, pqsamp_test_randombytes,
                                     &coin_source) == PQSAMP_OK);
        PQSAMP_CHECK(pqsamp_rng_init(&masks, pqsamp_test_randombytes,
                                     &mask_source) == PQSAMP_OK);
        PQSAMP_CHECK(pqsamp_generate_masked(sample, SAMPLE_COUNT, params,
                                            shares, &coins, &masks,
                                            NULL) == PQSAMP_OK);
        PQSAMP_CHECK(pqsamp_rng_bits_used(&coins) ==
                     pqsamp_rng_bits_used(&reference_coins));
        for (i = 0; i < SAMPLE_COUNT; i++)
        {
          PQSAMP_CHECK(pqsamp_reconstruct(&sample[i], shares) ==
                       pqsamp_reconstruct(&reference[i], 1U));
        }
      }
    }
  }
  return 0;
}

static int pqsamp_test_rng_failure(void)
{
  const pqsamp_params *params =
      pqsamp_params_get(PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_ZERO);
  pqsamp_test_rng source = pqsamp_test_rng_make(7U);
  pqsamp_rng rng;
  int16_t out[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  unsigned i;

  source.fail_after = 0;
  PQSAMP_CHECK(pqsamp_rng_init(&rng, pqsamp_test_randombytes, &source) ==
               PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_generate(out, 8, params, &rng, NULL) ==
               PQSAMP_ERR_RANDOM);
  for (i = 0; i < 8U; i++)
  {
    PQSAMP_CHECK(out[i] == 0);
  }
  return 0;
}

static int pqsamp_test_masked_rng_failure(void)
{
  enum
  {
    BLOCK_COUNT = 32,
    SAMPLE_COUNT = 33,
    SHARES = 3
  };
  const pqsamp_params *params =
      pqsamp_params_get(PQSAMP_PROFILE_S1521_1000, PQSAMP_CENTER_HALF);
  pqsamp_test_rng probe_coin_source =
      pqsamp_test_rng_make(UINT64_C(0xa4093822299f31d0));
  pqsamp_test_rng probe_mask_source =
      pqsamp_test_rng_make(UINT64_C(0x082efa98ec4e6c89));
  pqsamp_rng probe_coins;
  pqsamp_rng probe_masks;
  pqsamp_masked_i16 probe[BLOCK_COUNT];
  unsigned fail_masks;

  PQSAMP_CHECK(pqsamp_rng_init(&probe_coins, pqsamp_test_randombytes,
                               &probe_coin_source) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_rng_init(&probe_masks, pqsamp_test_randombytes,
                               &probe_mask_source) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_generate_masked(probe, BLOCK_COUNT, params, SHARES,
                                      &probe_coins, &probe_masks,
                                      NULL) == PQSAMP_OK);

  for (fail_masks = 0; fail_masks < 2U; fail_masks++)
  {
    pqsamp_test_rng coin_source =
        pqsamp_test_rng_make(UINT64_C(0xa4093822299f31d0));
    pqsamp_test_rng mask_source =
        pqsamp_test_rng_make(UINT64_C(0x082efa98ec4e6c89));
    pqsamp_rng coins;
    pqsamp_rng masks;
    pqsamp_masked_i16 out[SAMPLE_COUNT];
    unsigned i;

    if (fail_masks != 0U)
    {
      mask_source.fail_after = probe_mask_source.calls;
    }
    else
    {
      coin_source.fail_after = probe_coin_source.calls;
    }
    for (i = 0; i < SAMPLE_COUNT; i++)
    {
      unsigned share;

      for (share = 0; share < PQSAMP_MAX_SHARES; share++)
      {
        out[i].share[share] =
            (uint16_t)(UINT16_C(0x1234) + (uint16_t)i + (uint16_t)share);
      }
    }
    PQSAMP_CHECK(pqsamp_rng_init(&coins, pqsamp_test_randombytes,
                                 &coin_source) == PQSAMP_OK);
    PQSAMP_CHECK(pqsamp_rng_init(&masks, pqsamp_test_randombytes,
                                 &mask_source) == PQSAMP_OK);
    PQSAMP_CHECK(pqsamp_generate_masked(out, SAMPLE_COUNT, params, SHARES,
                                        &coins, &masks,
                                        NULL) == PQSAMP_ERR_RANDOM);
    for (i = 0; i < SAMPLE_COUNT; i++)
    {
      unsigned share;

      for (share = 0; share < PQSAMP_MAX_SHARES; share++)
      {
        PQSAMP_CHECK(out[i].share[share] == 0U);
      }
    }
  }
  return 0;
}

static int pqsamp_test_custom_params(void)
{
  enum
  {
    GEOM_COUNT = 40,
    SAMPLE_COUNT = 33
  };
  const pqsamp_params *base =
      pqsamp_params_get(PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_ZERO);
  pqsamp_candidate side0[GEOM_COUNT] = {{0}};
  pqsamp_candidate side1[GEOM_COUNT] = {{0}};
  pqsamp_params params = *base;
  pqsamp_test_rng plain_source =
      pqsamp_test_rng_make(UINT64_C(0x452821e638d01377));
  pqsamp_test_rng coin_source =
      pqsamp_test_rng_make(UINT64_C(0xbe5466cf34e90c6c));
  pqsamp_test_rng mask_source =
      pqsamp_test_rng_make(UINT64_C(0xc0ac29b7c97c50dd));
  pqsamp_rng plain_rng;
  pqsamp_rng coins;
  pqsamp_rng masks;
  int16_t plain[8];
  pqsamp_masked_i16 masked[SAMPLE_COUNT];
  unsigned i;

  for (i = 0; i < base->geom_count; i++)
  {
    side0[i] = base->side[0][i];
    side1[i] = base->side[1][i];
  }
  params.geom_count = GEOM_COUNT;
  params.side_num = 1;
  params.side_den = 4;
  params.side[0] = side0;
  params.side[1] = side1;
  PQSAMP_CHECK(pqsamp_params_check(&params) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_rng_init(&plain_rng, pqsamp_test_randombytes,
                               &plain_source) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_generate(plain, 8, &params, &plain_rng, NULL) ==
               PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_rng_init(&coins, pqsamp_test_randombytes, &coin_source) ==
               PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_rng_init(&masks, pqsamp_test_randombytes, &mask_source) ==
               PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_generate_masked(masked, SAMPLE_COUNT, &params, 2U, &coins,
                                      &masks, NULL) == PQSAMP_OK);
  return 0;
}

int main(void)
{
  unsigned shares;

  PQSAMP_CHECK(pqsamp_test_vectors() == 0);
  PQSAMP_CHECK(pqsamp_test_plain_profile(PQSAMP_PROFILE_S3_2,
                                         PQSAMP_CENTER_ZERO, 0.0, 1.622) == 0);
  PQSAMP_CHECK(pqsamp_test_plain_profile(PQSAMP_PROFILE_S3_2,
                                         PQSAMP_CENTER_HALF, 0.5, 1.622) == 0);
  PQSAMP_CHECK(pqsamp_test_plain_profile(PQSAMP_PROFILE_S1521_1000,
                                         PQSAMP_CENTER_ZERO, 0.0, 1.669) == 0);
  PQSAMP_CHECK(pqsamp_test_plain_profile(PQSAMP_PROFILE_S1521_1000,
                                         PQSAMP_CENTER_HALF, 0.5, 1.669) == 0);
  for (shares = 2; shares <= PQSAMP_MAX_SHARES; shares++)
  {
    PQSAMP_CHECK(pqsamp_test_masked(shares) == 0);
  }
  PQSAMP_CHECK(pqsamp_test_masked_differential() == 0);
  PQSAMP_CHECK(pqsamp_test_rng_failure() == 0);
  PQSAMP_CHECK(pqsamp_test_masked_rng_failure() == 0);
  PQSAMP_CHECK(pqsamp_test_custom_params() == 0);
  puts("sampler: ok");
  return 0;
}
