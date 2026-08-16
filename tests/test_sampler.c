#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "internal.h"
#include "test.h"

typedef struct
{
  uint32_t word[256];
  size_t count;
  size_t offset;
} word_source;

static void push_word(word_source *source, uint32_t word)
{
  source->word[source->count++] = word;
}

static void push_value(word_source *source, uint64_t value, unsigned bits)
{
  unsigned bit;

  for (bit = 0; bit < bits; bit++)
  {
    push_word(source, ((value >> bit) & 1U) != 0U ? UINT32_MAX : 0U);
  }
}

static int word_randombytes(void *context, uint8_t *out, size_t n)
{
  word_source *source = context;
  size_t i;

  if (n != 8U || source->offset + 2U > source->count)
  {
    return -1;
  }
  for (i = 0; i < n; i++)
  {
    uint32_t word = source->word[source->offset + i / 4U];

    out[i] = (uint8_t)(word >> (8U * (i & 3U)));
  }
  source->offset += 2U;
  return 0;
}

static void push_side(word_source *source, const pqsamp_params *params,
                      unsigned side)
{
  unsigned bits = pqsamp_bit_width_u32(params->side_den - 1U);
  uint32_t value;

  if (params->side_num == 1U && params->side_den == 2U)
  {
    value = side;
    push_value(source, value, bits);
  }
  else
  {
    push_word(source, side != 0U ? UINT32_MAX : 0U);
    push_word(source, 0U);
  }
}

static uint64_t lane_value(const pqsamp_word *word, unsigned bits,
                           unsigned lane, unsigned shares)
{
  uint64_t value = 0;
  unsigned bit;

  for (bit = 0; bit < bits; bit++)
  {
    unsigned share;
    uint32_t plane = 0;

    for (share = 0; share < shares; share++)
    {
      plane ^= word[bit].share[share];
    }
    value |= (uint64_t)((plane >> lane) & 1U) << bit;
  }
  return value;
}

typedef struct
{
  pqsamp_half_batch batch;
  int16_t y;
  uint32_t live;
  uint32_t accept;
  uint64_t pre_ands;
  uint64_t finish_ands;
  uint64_t total_ands;
} half_result;

static int half_case(const pqsamp_params *params, unsigned side,
                     unsigned geometric, unsigned k, uint64_t u,
                     half_result *result)
{
  word_source source = {{0}, 0, 0};
  pqsamp_rng coins;
  pqsamp_state state;
  pqsamp_stats stats;
  pqsamp_word y[PQSAMP_VALUE_BITS];
  uint16_t bits;
  unsigned i;
  int rc;

  push_side(&source, params, side);
  for (i = 0; i < params->geom_count; i++)
  {
    push_word(&source, i == geometric ? UINT32_MAX : 0U);
  }
  for (i = 0; i < params->k_sat; i++)
  {
    push_word(&source, i == k ? UINT32_MAX : 0U);
  }
  push_value(&source, u, params->threshold_bits);
  if ((source.count & 1U) != 0U)
  {
    push_word(&source, 0U);
  }
  stats_clear(&stats);
  rc = pqsamp_rng_init(&coins, word_randombytes, &source);
  if (rc != PQSAMP_OK)
  {
    return rc;
  }
  state.shares = 1U;
  state.coins = &coins;
  state.masks = NULL;
  state.stats = &stats;
  rc = pqsamp_sample_half_pre(&state, &result->batch, &result->live, params);
  if (rc != PQSAMP_OK)
  {
    return rc;
  }
  result->pre_ands = stats.sec_and_calls;
  rc = pqsamp_sample_half_finish(&state, &result->batch, result->live,
                                 &result->accept, params);
  if (rc != PQSAMP_OK)
  {
    return rc;
  }
  result->finish_ands = stats.sec_and_calls - result->pre_ands;
  rc = pqsamp_half_reconstruct(&state, y, &result->batch.value);
  if (rc != PQSAMP_OK)
  {
    return rc;
  }
  result->total_ands = stats.sec_and_calls;
  bits = (uint16_t)lane_value(y, PQSAMP_VALUE_BITS, 0U, 1U);
  result->y = bits > (uint16_t)INT16_MAX
                  ? (int16_t)((int32_t)bits - INT32_C(65536))
                  : (int16_t)bits;
  return PQSAMP_OK;
}

static int masked_case(const pqsamp_params *params, unsigned side,
                       unsigned geometric, unsigned k, uint64_t u, int16_t *y,
                       int *accept)
{
  word_source source = {{0}, 0, 0};
  pqsamp_rng coins;
  pqsamp_state state;
  pqsamp_stats stats;
  pqsamp_trace trace = {0, 0, 0};
  pqsamp_batch batch;
  pqsamp_zero_side_pool side_pool;
  uint32_t live;
  uint32_t lanes;
  uint16_t bits = 0;
  unsigned i;
  int rc;

  if (params->center_num == 1 && params->center_den == 2U)
  {
    half_result result;

    rc = half_case(params, side, geometric, k, u, &result);
    if (rc != PQSAMP_OK)
    {
      return rc;
    }
    *y = result.y;
    *accept = (result.accept & 1U) != 0U;
    return PQSAMP_OK;
  }

  push_side(&source, params, side);
  for (i = 0; i < params->geom_count; i++)
  {
    push_word(&source, i == geometric ? UINT32_MAX : 0U);
  }
  for (i = 0; i < params->k_sat; i++)
  {
    push_word(&source, i == k ? UINT32_MAX : 0U);
  }
  push_value(&source, u, params->threshold_bits);
  if ((source.count & 1U) != 0U)
  {
    push_word(&source, 0U);
  }

  rc = pqsamp_rng_init(&coins, word_randombytes, &source);
  if (rc != PQSAMP_OK)
  {
    return rc;
  }
  state.shares = 1U;
  state.coins = &coins;
  state.masks = NULL;
  stats_clear(&stats);
  state.stats = &stats;
  pqsamp_zero_side_pool_init(&side_pool);
  rc = pqsamp_sample_pre(&state, &batch, &live, params, &side_pool, &trace);
  if (rc != PQSAMP_OK)
  {
    return rc;
  }
  PQSAMP_CHECK(stats.sec_and_calls ==
               trace.raw_side_batches + 2U * params->geom_count - 1U +
                   params->k_sat - 1U + pqsamp_bit_width_u32(params->k_sat));
  rc = pqsamp_sample_finish(&state, &batch, live, &lanes, params);
  if (rc != PQSAMP_OK)
  {
    return rc;
  }
  PQSAMP_CHECK(stats.sec_and_calls ==
               trace.raw_side_batches + 2U * params->geom_count - 1U +
                   params->k_sat - 1U +
                   2U * pqsamp_bit_width_u32(params->k_sat) +
                   params->threshold_bits + 1U);
  for (i = 0; i < PQSAMP_VALUE_BITS; i++)
  {
    bits |= (uint16_t)((batch.y[i].share[0] & 1U) << i);
  }
  {
    pqsamp_masked_i16 value = {{bits, 0, 0, 0}};

    *y = test_reconstruct(&value, 1U);
  }
  *accept = (lanes & 1U) != 0U;
  return PQSAMP_OK;
}

static int semantic_case(const pqsamp_params *params, unsigned side,
                         unsigned geometric, unsigned k, uint64_t u)
{
  const pqsamp_candidate *entry = NULL;
  int16_t masked_y;
  int masked_accept;
  int scalar_accept = 0;

  if (geometric < params->geom_count)
  {
    entry = &params->side[side][geometric];
    scalar_accept = candidate_accept(entry, k, u);
  }
  PQSAMP_CHECK(masked_case(params, side, geometric, k, u, &masked_y,
                           &masked_accept) == PQSAMP_OK);
  PQSAMP_CHECK(masked_accept == scalar_accept);
  if (scalar_accept != 0)
  {
    PQSAMP_CHECK(masked_y == entry->y);
  }
  return 0;
}

static int proposal_positions(void)
{
  const pqsamp_params *params =
      pqsamp_profile_get(PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_ZERO);
  unsigned geometric;

  PQSAMP_CHECK(params != NULL);
  for (geometric = 0; geometric <= params->geom_count; geometric++)
  {
    word_source source = {{0}, 2, 0};
    pqsamp_rng rng;
    unsigned actual;

    if (geometric < params->geom_count)
    {
      source.word[geometric / 32U] = UINT32_C(1) << (geometric & 31U);
    }
    PQSAMP_CHECK(pqsamp_rng_init(&rng, word_randombytes, &source) == PQSAMP_OK);
    PQSAMP_CHECK(pqsamp_scalar_geom(&rng, params->geom_count, &actual) ==
                 PQSAMP_OK);
    PQSAMP_CHECK(actual == geometric);
    PQSAMP_CHECK(rng.bits_used == params->geom_count);
  }
  return 0;
}

static int scalar_masked_semantics(void)
{
  unsigned profile;

  for (profile = 0; profile < 2U; profile++)
  {
    unsigned center;

    for (center = 0; center < 2U; center++)
    {
      const pqsamp_params *params =
          pqsamp_profile_get((pqsamp_profile)profile, (pqsamp_center)center);
      uint64_t limit;
      unsigned side;

      PQSAMP_CHECK(params != NULL);
      limit = UINT64_C(1) << params->threshold_bits;
      for (side = 0; side < 2U; side++)
      {
        unsigned geometric;

        for (geometric = 0; geometric < params->geom_count; geometric++)
        {
          const pqsamp_candidate *entry = &params->side[side][geometric];
          unsigned q = entry->quotient;

          if (q > 0U)
          {
            PQSAMP_CHECK(semantic_case(params, side, geometric, q - 1U, 0U) ==
                         0);
          }
          PQSAMP_CHECK(semantic_case(params, side, geometric, q + 1U, 0U) == 0);
          if (entry->boundary_count > 0U)
          {
            PQSAMP_CHECK(semantic_case(params, side, geometric, q,
                                       entry->boundary_count - 1U) == 0);
          }
          if (entry->boundary_count < limit)
          {
            PQSAMP_CHECK(semantic_case(params, side, geometric, q,
                                       entry->boundary_count) == 0);
          }
        }
        PQSAMP_CHECK(semantic_case(params, side, params->geom_count, 0U, 0U) ==
                     0);
      }

      {
        pqsamp_candidate side0[PQSAMP_GEOM_BITS];
        pqsamp_candidate side1[PQSAMP_GEOM_BITS];
        pqsamp_params custom = *params;
        unsigned i;

        for (i = 0; i < params->geom_count; i++)
        {
          side0[i] = params->side[0][i];
          side1[i] = params->side[1][i];
        }
        custom.side[0] = side0;
        custom.side[1] = side1;
        side0[0].boundary_count = 0;
        PQSAMP_CHECK(semantic_case(&custom, 0U, 0U, side0[0].quotient, 0U) ==
                     0);
        side0[0].boundary_count = limit;
        PQSAMP_CHECK(
            semantic_case(&custom, 0U, 0U, side0[0].quotient, limit - 1U) == 0);
      }
    }
  }
  return 0;
}

static int half_profile_cases(void)
{
  unsigned profile;

  for (profile = 0; profile < 2U; profile++)
  {
    const pqsamp_params *params =
        pqsamp_profile_get((pqsamp_profile)profile, PQSAMP_CENTER_HALF);
    unsigned geometric;

    PQSAMP_CHECK(params != NULL);
    for (geometric = 0; geometric < params->geom_count; geometric++)
    {
      const pqsamp_candidate *negative = &params->side[0][geometric];
      const pqsamp_candidate *positive = &params->side[1][geometric];
      unsigned side;

      PQSAMP_CHECK(negative->y == -(int16_t)geometric);
      PQSAMP_CHECK(positive->y == (int16_t)(geometric + 1U));
      PQSAMP_CHECK(negative->quotient == positive->quotient);
      PQSAMP_CHECK(negative->boundary_count == positive->boundary_count);
      PQSAMP_CHECK(negative->valid == 1U);
      PQSAMP_CHECK(positive->valid == (geometric == 13U ? 0U : 1U));
      for (side = 0; side < 2U; side++)
      {
        const pqsamp_candidate *entry = &params->side[side][geometric];
        half_result result;
        unsigned expected_valid = geometric != 13U || side == 0U;

        PQSAMP_CHECK(half_case(params, side, geometric, params->k_sat, 0U,
                               &result) == PQSAMP_OK);
        PQSAMP_CHECK(lane_value(result.batch.value.g, PQSAMP_HALF_GEOM_BITS, 0U,
                                1U) == geometric);
        PQSAMP_CHECK(lane_value(&result.batch.value.side, 1U, 0U, 1U) == side);
        PQSAMP_CHECK(lane_value(result.batch.quotient, PQSAMP_K_BITS, 0U, 1U) ==
                     entry->quotient);
        PQSAMP_CHECK(lane_value(result.batch.boundary, PQSAMP_BOUNDARY_BITS, 0U,
                                1U) == entry->boundary_count);
        PQSAMP_CHECK(result.y == entry->y);
        PQSAMP_CHECK((result.live & 1U) == expected_valid);
        PQSAMP_CHECK((result.accept & 1U) == expected_valid);
        PQSAMP_CHECK(result.pre_ands == params->k_sat + 20U);
        PQSAMP_CHECK(result.finish_ands == 51U);
        PQSAMP_CHECK(result.total_ands == params->k_sat + 74U);
      }
    }
  }
  return 0;
}

static int vectors(void)
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
      test_rng source =
          test_rng_make(UINT64_C(0x123456789abcdef0) + profile * 17U + center);
      pqsamp_rng rng;
      int16_t sample[16];
      unsigned i;

      PQSAMP_CHECK(pqsamp_rng_init(&rng, test_randombytes, &source) ==
                   PQSAMP_OK);
      PQSAMP_CHECK(pqsamp_sample(sample, 16, (pqsamp_profile)profile,
                                 (pqsamp_center)center, &rng,
                                 NULL) == PQSAMP_OK);
      for (i = 0; i < 16U; i++)
      {
        PQSAMP_CHECK(sample[i] == expected[profile][center][i]);
      }
    }
  }
  return 0;
}

static int plain_profile(pqsamp_profile profile, pqsamp_center center,
                         double mean, double variance)
{
  enum
  {
    SAMPLE_COUNT = 20000
  };
  test_rng source =
      test_rng_make(UINT64_C(0x8a5cd789635d2dff) + profile * 7U + center);
  pqsamp_rng rng;
  int16_t sample[SAMPLE_COUNT];
  double sum = 0.0;
  double squares = 0.0;
  unsigned i;

  PQSAMP_CHECK(pqsamp_rng_init(&rng, test_randombytes, &source) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_sample(sample, SAMPLE_COUNT, profile, center, &rng,
                             NULL) == PQSAMP_OK);
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

static uint32_t word_value(const pqsamp_word *word, unsigned shares)
{
  uint32_t value = 0;
  unsigned share;

  for (share = 0; share < shares; share++)
  {
    value ^= word->share[share];
  }
  return value;
}

static void share_plane(pqsamp_word *word, uint32_t value, unsigned shares,
                        uint32_t salt)
{
  unsigned share;

  pqsamp_word_zero(word);
  word->share[0] = value;
  for (share = 1; share < shares; share++)
  {
    uint32_t mask = salt * (UINT32_C(0x9e3779b9) ^ share);

    word->share[share] = mask;
    word->share[0] ^= mask;
  }
}

static unsigned test_popcount32(uint32_t value)
{
  unsigned count = 0;

  while (value != 0U)
  {
    value &= value - 1U;
    count++;
  }
  return count;
}

static void push_zero_raw(word_source *source, uint32_t u0, uint32_t u1)
{
  push_word(source, u0);
  push_word(source, u1);
}

static void reference_side_append(uint8_t fifo[64], unsigned *fill, uint32_t u0,
                                  uint32_t u1)
{
  unsigned lane;

  for (lane = 0; lane < PQSAMP_LANES; lane++)
  {
    unsigned a = (u0 >> lane) & 1U;
    unsigned b = (u1 >> lane) & 1U;

    if ((a & b) == 0U)
    {
      fifo[*fill] = (uint8_t)a;
      (*fill)++;
    }
  }
}

static uint32_t reference_side_take(uint8_t fifo[64], unsigned *fill)
{
  uint32_t out = 0;
  unsigned i;

  for (i = 0; i < PQSAMP_LANES; i++)
  {
    out |= (uint32_t)fifo[i] << i;
  }
  for (i = PQSAMP_LANES; i < *fill; i++)
  {
    fifo[i - PQSAMP_LANES] = fifo[i];
  }
  *fill -= PQSAMP_LANES;
  return out;
}

static int zero_side_raw_pairs(void)
{
  unsigned shares;

  for (shares = 1U; shares <= PQSAMP_MAX_SHARES; shares++)
  {
    unsigned rotation;

    for (rotation = 0; rotation < 4U; rotation++)
    {
      word_source source = {{0}, 0, 0};
      test_rng mask_source =
          test_rng_make(UINT64_C(0x510e527fade682d1) + shares * 7U + rotation);
      pqsamp_rng coins;
      pqsamp_rng masks;
      pqsamp_state state;
      pqsamp_stats stats;
      pqsamp_trace trace = {0, 0, 0};
      pqsamp_zero_side_pool pool;
      pqsamp_word out;
      uint8_t fifo[64] = {0};
      unsigned fill = 0;
      uint32_t u0 = 0;
      uint32_t u1 = 0;
      uint32_t second = UINT32_C(0xa53cc35a);
      uint32_t expected;
      unsigned lane;

      for (lane = 0; lane < PQSAMP_LANES; lane++)
      {
        unsigned pair = (lane + rotation) & 3U;

        u0 |= (uint32_t)(pair >> 1) << lane;
        u1 |= (uint32_t)(pair & 1U) << lane;
      }
      push_zero_raw(&source, u0, u1);
      push_zero_raw(&source, second, 0U);
      reference_side_append(fifo, &fill, u0, u1);
      reference_side_append(fifo, &fill, second, 0U);
      expected = reference_side_take(fifo, &fill);

      PQSAMP_CHECK(pqsamp_rng_init(&coins, word_randombytes, &source) ==
                   PQSAMP_OK);
      PQSAMP_CHECK(pqsamp_rng_init(&masks, test_randombytes, &mask_source) ==
                   PQSAMP_OK);
      stats_clear(&stats);
      state.shares = shares;
      state.coins = &coins;
      state.masks = shares == 1U ? NULL : &masks;
      state.stats = &stats;
      pqsamp_zero_side_pool_init(&pool);
      PQSAMP_CHECK(pqsamp_zero_side(&state, &pool, &out, &trace) == PQSAMP_OK);
      PQSAMP_CHECK(word_value(&out, shares) == expected);
      PQSAMP_CHECK(pool.fill == fill);
      PQSAMP_CHECK(word_value(&pool.word[0], shares) ==
                   (second >> (PQSAMP_LANES - fill)));
      PQSAMP_CHECK(source.offset == 4U);
      PQSAMP_CHECK(trace.raw_side_batches == 2U);
      PQSAMP_CHECK(stats.sec_and_calls == trace.raw_side_batches);
      PQSAMP_CHECK(masks.bits_used == (uint64_t)trace.raw_side_batches * 32U *
                                          (shares - 1U) * (shares + 2U));
    }
  }
  return 0;
}

static int zero_side_compaction(void)
{
  static const uint32_t valid_masks[] = {0U, 1U, UINT32_C(0xaaaaaaaa), 3U,
                                         UINT32_MAX};
  unsigned mask_index;

  for (mask_index = 0;
       mask_index < sizeof(valid_masks) / sizeof(valid_masks[0]); mask_index++)
  {
    word_source source = {{0}, 0, 0};
    pqsamp_rng coins;
    pqsamp_state state;
    pqsamp_stats stats;
    pqsamp_trace trace = {0, 0, 0};
    pqsamp_zero_side_pool pool;
    pqsamp_word out;
    uint8_t fifo[64] = {0};
    unsigned fill = 0;
    uint32_t valid = valid_masks[mask_index];
    uint32_t value = UINT32_C(0xa53cc35a) & valid;
    uint32_t invalid = ~valid;
    uint32_t u0 = invalid | value;
    uint32_t u1 = invalid;
    uint32_t expected;

    push_zero_raw(&source, u0, u1);
    reference_side_append(fifo, &fill, u0, u1);
    if (fill < PQSAMP_LANES)
    {
      push_zero_raw(&source, UINT32_C(0x3cc3a55a), 0U);
      reference_side_append(fifo, &fill, UINT32_C(0x3cc3a55a), 0U);
    }
    expected = reference_side_take(fifo, &fill);
    PQSAMP_CHECK(pqsamp_rng_init(&coins, word_randombytes, &source) ==
                 PQSAMP_OK);
    stats_clear(&stats);
    state.shares = 1U;
    state.coins = &coins;
    state.masks = NULL;
    state.stats = &stats;
    pqsamp_zero_side_pool_init(&pool);
    PQSAMP_CHECK(pqsamp_zero_side(&state, &pool, &out, &trace) == PQSAMP_OK);
    PQSAMP_CHECK(out.share[0] == expected);
    PQSAMP_CHECK(pool.fill == fill);
    PQSAMP_CHECK(trace.raw_side_batches ==
                 (test_popcount32(valid) == PQSAMP_LANES ? 1U : 2U));
    PQSAMP_CHECK(stats.sec_and_calls == trace.raw_side_batches);
  }
  return 0;
}

static int zero_side_fills(void)
{
  static const unsigned fills[] = {0U, 1U, 31U, 32U, 33U, 63U};
  unsigned fill_index;

  for (fill_index = 0; fill_index < sizeof(fills) / sizeof(fills[0]);
       fill_index++)
  {
    word_source source = {{0}, 0, 0};
    test_rng mask_source =
        test_rng_make(UINT64_C(0x1f83d9abfb41bd6b) + fill_index);
    pqsamp_rng coins;
    pqsamp_rng masks;
    pqsamp_state state;
    pqsamp_trace trace = {0, 0, 0};
    pqsamp_zero_side_pool pool;
    pqsamp_word out;
    uint8_t fifo[64] = {0};
    unsigned fill = fills[fill_index];
    uint32_t low = 0;
    uint32_t high = 0;
    uint32_t raw = UINT32_C(0xc33ca55a);
    uint32_t expected;
    uint32_t remaining = 0;
    unsigned i;

    pqsamp_zero_side_pool_init(&pool);
    for (i = 0; i < fill; i++)
    {
      unsigned bit = (i * 5U + 1U) & 1U;

      fifo[i] = (uint8_t)bit;
      if (i < PQSAMP_LANES)
      {
        low |= (uint32_t)bit << i;
      }
      else
      {
        high |= (uint32_t)bit << (i - PQSAMP_LANES);
      }
    }
    share_plane(&pool.word[0], low, 3U, 71U);
    share_plane(&pool.word[1], high, 3U, 73U);
    pool.fill = fill;
    if (fill < PQSAMP_LANES)
    {
      push_zero_raw(&source, raw, 0U);
      reference_side_append(fifo, &fill, raw, 0U);
    }
    expected = reference_side_take(fifo, &fill);
    for (i = 0; i < fill; i++)
    {
      remaining |= (uint32_t)fifo[i] << i;
    }
    PQSAMP_CHECK(pqsamp_rng_init(&coins, word_randombytes, &source) ==
                 PQSAMP_OK);
    PQSAMP_CHECK(pqsamp_rng_init(&masks, test_randombytes, &mask_source) ==
                 PQSAMP_OK);
    state.shares = 3U;
    state.coins = &coins;
    state.masks = &masks;
    state.stats = NULL;
    PQSAMP_CHECK(pqsamp_zero_side(&state, &pool, &out, &trace) == PQSAMP_OK);
    PQSAMP_CHECK(word_value(&out, 3U) == expected);
    PQSAMP_CHECK(pool.fill == fill);
    PQSAMP_CHECK(word_value(&pool.word[0], 3U) == remaining);
    PQSAMP_CHECK(source.offset == (fills[fill_index] < PQSAMP_LANES ? 2U : 0U));
  }
  return 0;
}

static int zero_side_script(void)
{
  static const uint32_t valid[6] = {
      UINT32_C(0xaaaaaaaa), UINT32_MAX, UINT32_C(0x77777777),
      UINT32_C(0x7fffffff), 1U,         UINT32_MAX};
  static const uint32_t value[6] = {UINT32_C(0x88228822),
                                    UINT32_C(0x12345678),
                                    UINT32_C(0x51050105),
                                    UINT32_C(0x55aa33cc),
                                    1U,
                                    UINT32_C(0xc33ca55a)};
  word_source source = {{0}, 0, 0};
  pqsamp_rng coins;
  pqsamp_state state;
  pqsamp_trace trace = {0, 0, 0};
  pqsamp_zero_side_pool pool;
  uint8_t fifo[64] = {0};
  unsigned fill = 0;
  unsigned raw = 0;
  unsigned request;

  for (request = 0; request < 6U; request++)
  {
    uint32_t invalid = ~valid[request];

    push_zero_raw(&source, invalid | (value[request] & valid[request]),
                  invalid);
  }
  PQSAMP_CHECK(pqsamp_rng_init(&coins, word_randombytes, &source) == PQSAMP_OK);
  state.shares = 1U;
  state.coins = &coins;
  state.masks = NULL;
  state.stats = NULL;
  pqsamp_zero_side_pool_init(&pool);
  for (request = 0; request < 4U; request++)
  {
    pqsamp_word out;
    uint32_t expected;

    while (fill < PQSAMP_LANES)
    {
      uint32_t invalid = ~valid[raw];
      uint32_t u0 = invalid | (value[raw] & valid[raw]);

      reference_side_append(fifo, &fill, u0, invalid);
      raw++;
    }
    expected = reference_side_take(fifo, &fill);
    PQSAMP_CHECK(pqsamp_zero_side(&state, &pool, &out, &trace) == PQSAMP_OK);
    PQSAMP_CHECK(out.share[0] == expected);
    PQSAMP_CHECK(pool.fill == fill);
    PQSAMP_CHECK(source.offset / 2U == raw);
    PQSAMP_CHECK(trace.raw_side_batches == raw);
  }
  return 0;
}

static int zero_side_law(void)
{
  unsigned pair;
  unsigned valid = 0;
  unsigned ones = 0;
  unsigned retry;

  for (pair = 0; pair < 4U; pair++)
  {
    unsigned u0 = pair >> 1;
    unsigned u1 = pair & 1U;

    if ((u0 & u1) == 0U)
    {
      valid++;
      ones += u0;
    }
  }
  PQSAMP_CHECK(valid == 3U);
  PQSAMP_CHECK(ones == 1U);
  for (retry = 1U; retry <= 4U; retry++)
  {
    unsigned sequences = 1U << (2U * retry);
    unsigned sequence;
    unsigned trace_valid = 0;
    unsigned trace_ones = 0;

    for (sequence = 0; sequence < sequences; sequence++)
    {
      unsigned position;

      for (position = 0; position < retry; position++)
      {
        unsigned current = (sequence >> (2U * position)) & 3U;

        if (current != 3U)
        {
          if (position + 1U == retry)
          {
            trace_valid++;
            trace_ones += current >> 1;
          }
          break;
        }
      }
    }
    PQSAMP_CHECK(trace_valid == 3U);
    PQSAMP_CHECK(trace_ones == 1U);
  }
  {
    unsigned first;
    unsigned second;
    unsigned joint[2][2] = {{0, 0}, {0, 0}};

    for (first = 0; first < 3U; first++)
    {
      for (second = 0; second < 3U; second++)
      {
        joint[first >> 1][second >> 1]++;
      }
    }
    PQSAMP_CHECK(joint[0][0] == 4U);
    PQSAMP_CHECK(joint[0][1] == 2U);
    PQSAMP_CHECK(joint[1][0] == 2U);
    PQSAMP_CHECK(joint[1][1] == 1U);
  }
  return 0;
}

static int half_reconstruction(void)
{
  static const unsigned fills[] = {0U, 1U, 15U, 31U, 32U};
  unsigned shares;

  for (shares = 1U; shares <= PQSAMP_MAX_SHARES; shares++)
  {
    unsigned fill_index;

    for (fill_index = 0; fill_index < sizeof(fills) / sizeof(fills[0]);
         fill_index++)
    {
      test_rng mask_source = test_rng_make(UINT64_C(0x6a09e667f3bcc909) +
                                           shares * 17U + fill_index);
      pqsamp_rng masks;
      pqsamp_state state;
      pqsamp_stats stats;
      pqsamp_half_value value;
      pqsamp_word y[PQSAMP_VALUE_BITS];
      uint32_t g_plane[PQSAMP_HALF_GEOM_BITS] = {0};
      uint32_t side_plane = 0;
      unsigned lane;
      unsigned bit;

      for (lane = 0; lane < fills[fill_index]; lane++)
      {
        unsigned g = (lane * 5U + 3U) % 14U;
        unsigned side = (lane * 7U + 1U) & 1U;

        side_plane |= (uint32_t)side << lane;
        for (bit = 0; bit < PQSAMP_HALF_GEOM_BITS; bit++)
        {
          g_plane[bit] |= (uint32_t)((g >> bit) & 1U) << lane;
        }
      }
      for (bit = 0; bit < PQSAMP_HALF_GEOM_BITS; bit++)
      {
        share_plane(&value.g[bit], g_plane[bit], shares, bit + 31U);
      }
      share_plane(&value.side, side_plane, shares, 47U);
      PQSAMP_CHECK(pqsamp_rng_init(&masks, test_randombytes, &mask_source) ==
                   PQSAMP_OK);
      stats_clear(&stats);
      state.shares = shares;
      state.coins = NULL;
      state.masks = shares == 1U ? NULL : &masks;
      state.stats = &stats;
      PQSAMP_CHECK(pqsamp_half_reconstruct(&state, y, &value) == PQSAMP_OK);
      PQSAMP_CHECK(stats.sec_and_calls == 3U);
      for (lane = 0; lane < PQSAMP_LANES; lane++)
      {
        unsigned g = (lane * 5U + 3U) % 14U;
        unsigned side = (lane * 7U + 1U) & 1U;
        int16_t expected = lane < fills[fill_index]
                               ? (side != 0U ? (int16_t)(g + 1U) : -(int16_t)g)
                               : 0;
        uint16_t bits =
            (uint16_t)lane_value(y, PQSAMP_VALUE_BITS, lane, shares);
        int16_t actual = bits > (uint16_t)INT16_MAX
                             ? (int16_t)((int32_t)bits - INT32_C(65536))
                             : (int16_t)bits;

        PQSAMP_CHECK(actual == expected);
        PQSAMP_CHECK(lane_value(y, PQSAMP_HALF_VALUE_BITS, lane, shares) ==
                     ((uint16_t)expected & UINT16_C(31)));
        for (bit = PQSAMP_HALF_VALUE_BITS; bit < PQSAMP_VALUE_BITS; bit++)
        {
          PQSAMP_CHECK(lane_value(&y[bit], 1U, lane, shares) ==
                       (expected < 0 ? 1U : 0U));
        }
      }
    }
  }
  return 0;
}

static int batch_matches(const pqsamp_batch *actual,
                         const pqsamp_batch *expected, unsigned shares)
{
  unsigned bit;

  for (bit = 0; bit < PQSAMP_VALUE_BITS; bit++)
  {
    unsigned share;

    PQSAMP_CHECK(word_value(&actual->y[bit], shares) ==
                 expected->y[bit].share[0]);
    for (share = shares; share < PQSAMP_MAX_SHARES; share++)
    {
      PQSAMP_CHECK(actual->y[bit].share[share] == 0U);
    }
  }
  for (bit = 0; bit < PQSAMP_K_BITS; bit++)
  {
    unsigned share;

    PQSAMP_CHECK(word_value(&actual->quotient[bit], shares) ==
                 expected->quotient[bit].share[0]);
    PQSAMP_CHECK(word_value(&actual->k[bit], shares) ==
                 expected->k[bit].share[0]);
    for (share = shares; share < PQSAMP_MAX_SHARES; share++)
    {
      PQSAMP_CHECK(actual->quotient[bit].share[share] == 0U);
      PQSAMP_CHECK(actual->k[bit].share[share] == 0U);
    }
  }
  for (bit = 0; bit < PQSAMP_BOUNDARY_BITS; bit++)
  {
    unsigned share;

    PQSAMP_CHECK(word_value(&actual->boundary[bit], shares) ==
                 expected->boundary[bit].share[0]);
    for (share = shares; share < PQSAMP_MAX_SHARES; share++)
    {
      PQSAMP_CHECK(actual->boundary[bit].share[share] == 0U);
    }
  }
  return 0;
}

static int half_batch_matches(const pqsamp_half_batch *actual,
                              const pqsamp_half_batch *expected,
                              unsigned shares)
{
  unsigned bit;

  for (bit = 0; bit < PQSAMP_HALF_GEOM_BITS; bit++)
  {
    PQSAMP_CHECK(word_value(&actual->value.g[bit], shares) ==
                 expected->value.g[bit].share[0]);
  }
  PQSAMP_CHECK(word_value(&actual->value.side, shares) ==
               expected->value.side.share[0]);
  for (bit = 0; bit < PQSAMP_K_BITS; bit++)
  {
    PQSAMP_CHECK(word_value(&actual->quotient[bit], shares) ==
                 expected->quotient[bit].share[0]);
    PQSAMP_CHECK(word_value(&actual->k[bit], shares) ==
                 expected->k[bit].share[0]);
  }
  for (bit = 0; bit < PQSAMP_BOUNDARY_BITS; bit++)
  {
    PQSAMP_CHECK(word_value(&actual->boundary[bit], shares) ==
                 expected->boundary[bit].share[0]);
  }
  return 0;
}

static int staged_share_differential(void)
{
  unsigned profile;

  for (profile = 0; profile < 2U; profile++)
  {
    unsigned center;

    for (center = 0; center < 1U; center++)
    {
      const pqsamp_params *params =
          pqsamp_profile_get((pqsamp_profile)profile, (pqsamp_center)center);
      uint64_t seed = UINT64_C(0x452821e638d01377) + profile * 19U + center;
      test_rng reference_source = test_rng_make(seed);
      pqsamp_rng reference_coins;
      pqsamp_state reference_state;
      pqsamp_batch reference;
      pqsamp_zero_side_pool reference_side_pool;
      uint64_t pre_bits;
      uint64_t finish_bits;
      uint32_t reference_live;
      uint32_t reference_accept;
      unsigned shares;

      PQSAMP_CHECK(params != NULL);
      PQSAMP_CHECK(pqsamp_rng_init(&reference_coins, test_randombytes,
                                   &reference_source) == PQSAMP_OK);
      reference_state.shares = 1U;
      reference_state.coins = &reference_coins;
      reference_state.masks = NULL;
      reference_state.stats = NULL;
      pqsamp_zero_side_pool_init(&reference_side_pool);
      PQSAMP_CHECK(pqsamp_sample_pre(&reference_state, &reference,
                                     &reference_live, params,
                                     &reference_side_pool, NULL) == PQSAMP_OK);
      pre_bits = reference_coins.bits_used;
      PQSAMP_CHECK(pqsamp_sample_finish(&reference_state, &reference,
                                        reference_live, &reference_accept,
                                        params) == PQSAMP_OK);
      finish_bits = reference_coins.bits_used;

      for (shares = 2U; shares <= PQSAMP_MAX_SHARES; shares++)
      {
        test_rng coin_source = test_rng_make(seed);
        test_rng mask_source =
            test_rng_make(UINT64_C(0xbe5466cf34e90c6c) + shares * 23U +
                          profile * 5U + center);
        pqsamp_rng coins;
        pqsamp_rng masks;
        pqsamp_state state;
        pqsamp_batch batch;
        pqsamp_zero_side_pool side_pool;
        uint32_t live;
        uint32_t accept;

        PQSAMP_CHECK(pqsamp_rng_init(&coins, test_randombytes, &coin_source) ==
                     PQSAMP_OK);
        PQSAMP_CHECK(pqsamp_rng_init(&masks, test_randombytes, &mask_source) ==
                     PQSAMP_OK);
        state.shares = shares;
        state.coins = &coins;
        state.masks = &masks;
        state.stats = NULL;
        pqsamp_zero_side_pool_init(&side_pool);
        PQSAMP_CHECK(pqsamp_sample_pre(&state, &batch, &live, params,
                                       &side_pool, NULL) == PQSAMP_OK);
        PQSAMP_CHECK(live == reference_live);
        PQSAMP_CHECK(coins.bits_used == pre_bits);
        PQSAMP_CHECK(batch_matches(&batch, &reference, shares) == 0);
        PQSAMP_CHECK(pqsamp_sample_finish(&state, &batch, live, &accept,
                                          params) == PQSAMP_OK);
        PQSAMP_CHECK(accept == reference_accept);
        PQSAMP_CHECK(coins.bits_used == finish_bits);
      }
    }
  }
  return 0;
}

static int staged_half_differential(void)
{
  unsigned profile;

  for (profile = 0; profile < 2U; profile++)
  {
    const pqsamp_params *params =
        pqsamp_profile_get((pqsamp_profile)profile, PQSAMP_CENTER_HALF);
    uint64_t seed = UINT64_C(0x452821e638d01378) + profile * 19U;
    test_rng reference_source = test_rng_make(seed);
    pqsamp_rng reference_coins;
    pqsamp_state reference_state;
    pqsamp_half_batch reference;
    pqsamp_word reference_y[PQSAMP_VALUE_BITS];
    uint64_t pre_bits;
    uint64_t finish_bits;
    uint32_t reference_live;
    uint32_t reference_accept;
    unsigned shares;

    PQSAMP_CHECK(params != NULL);
    PQSAMP_CHECK(pqsamp_rng_init(&reference_coins, test_randombytes,
                                 &reference_source) == PQSAMP_OK);
    reference_state.shares = 1U;
    reference_state.coins = &reference_coins;
    reference_state.masks = NULL;
    reference_state.stats = NULL;
    PQSAMP_CHECK(pqsamp_sample_half_pre(&reference_state, &reference,
                                        &reference_live, params) == PQSAMP_OK);
    pre_bits = reference_coins.bits_used;
    PQSAMP_CHECK(pqsamp_sample_half_finish(&reference_state, &reference,
                                           reference_live, &reference_accept,
                                           params) == PQSAMP_OK);
    finish_bits = reference_coins.bits_used;
    PQSAMP_CHECK(pqsamp_half_reconstruct(&reference_state, reference_y,
                                         &reference.value) == PQSAMP_OK);

    for (shares = 2U; shares <= PQSAMP_MAX_SHARES; shares++)
    {
      test_rng coin_source = test_rng_make(seed);
      test_rng mask_source = test_rng_make(UINT64_C(0xbe5466cf34e90c6d) +
                                           shares * 23U + profile * 5U);
      pqsamp_rng coins;
      pqsamp_rng masks;
      pqsamp_state state;
      pqsamp_half_batch batch;
      pqsamp_word y[PQSAMP_VALUE_BITS];
      uint32_t live;
      uint32_t accept;
      unsigned bit;

      PQSAMP_CHECK(pqsamp_rng_init(&coins, test_randombytes, &coin_source) ==
                   PQSAMP_OK);
      PQSAMP_CHECK(pqsamp_rng_init(&masks, test_randombytes, &mask_source) ==
                   PQSAMP_OK);
      state.shares = shares;
      state.coins = &coins;
      state.masks = &masks;
      state.stats = NULL;
      PQSAMP_CHECK(pqsamp_sample_half_pre(&state, &batch, &live, params) ==
                   PQSAMP_OK);
      PQSAMP_CHECK(live == reference_live);
      PQSAMP_CHECK(coins.bits_used == pre_bits);
      PQSAMP_CHECK(half_batch_matches(&batch, &reference, shares) == 0);
      PQSAMP_CHECK(pqsamp_sample_half_finish(&state, &batch, live, &accept,
                                             params) == PQSAMP_OK);
      PQSAMP_CHECK(accept == reference_accept);
      PQSAMP_CHECK(coins.bits_used == finish_bits);
      PQSAMP_CHECK(pqsamp_half_reconstruct(&state, y, &batch.value) ==
                   PQSAMP_OK);
      for (bit = 0; bit < PQSAMP_VALUE_BITS; bit++)
      {
        PQSAMP_CHECK(word_value(&y[bit], shares) == reference_y[bit].share[0]);
      }
    }
  }
  return 0;
}

static int reference_half_stages(const pqsamp_params *params, uint64_t seed,
                                 uint32_t *live, uint32_t *accept,
                                 int16_t y[PQSAMP_LANES], uint64_t *pre_bits,
                                 uint64_t *finish_bits)
{
  test_rng source = test_rng_make(seed);
  pqsamp_rng coins;
  uint32_t side;
  uint32_t proposal[PQSAMP_GEOM_BITS];
  uint32_t k_coin[PQSAMP_K_SAT_MAX];
  uint32_t boundary[PQSAMP_THRESHOLD_BITS];
  unsigned i;
  unsigned lane;

  PQSAMP_CHECK(pqsamp_rng_init(&coins, test_randombytes, &source) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_rng_word(&coins, &side) == PQSAMP_OK);
  for (i = 0; i < params->geom_count; i++)
  {
    PQSAMP_CHECK(pqsamp_rng_word(&coins, &proposal[i]) == PQSAMP_OK);
  }
  for (i = 0; i < params->k_sat; i++)
  {
    PQSAMP_CHECK(pqsamp_rng_word(&coins, &k_coin[i]) == PQSAMP_OK);
  }
  *pre_bits = coins.bits_used;
  for (i = 0; i < params->threshold_bits; i++)
  {
    PQSAMP_CHECK(pqsamp_rng_word(&coins, &boundary[i]) == PQSAMP_OK);
  }
  *finish_bits = coins.bits_used;
  *live = 0;
  *accept = 0;
  for (lane = 0; lane < PQSAMP_LANES; lane++)
  {
    unsigned g = params->geom_count;
    unsigned k = params->k_sat;
    unsigned s = (side >> lane) & 1U;
    uint64_t u = 0;

    for (i = 0; i < params->geom_count; i++)
    {
      if (((proposal[i] >> lane) & 1U) != 0U)
      {
        g = i;
        break;
      }
    }
    for (i = 0; i < params->k_sat; i++)
    {
      if (((k_coin[i] >> lane) & 1U) != 0U)
      {
        k = i;
        break;
      }
    }
    for (i = 0; i < params->threshold_bits; i++)
    {
      u |= (uint64_t)((boundary[i] >> lane) & 1U) << i;
    }
    y[lane] = 0;
    if (g < params->geom_count)
    {
      const pqsamp_candidate *entry = &params->side[s][g];

      y[lane] = entry->y;
      if (entry->valid != 0U && k >= entry->quotient)
      {
        *live |= UINT32_C(1) << lane;
        if (candidate_accept(entry, k, u) != 0)
        {
          *accept |= UINT32_C(1) << lane;
        }
      }
    }
  }
  return 0;
}

static int half_stage_reference(void)
{
  unsigned profile;

  for (profile = 0; profile < 2U; profile++)
  {
    const pqsamp_params *params =
        pqsamp_profile_get((pqsamp_profile)profile, PQSAMP_CENTER_HALF);
    uint64_t seed = UINT64_C(0x3c6ef372fe94f82b) + profile;
    test_rng source = test_rng_make(seed);
    pqsamp_rng coins;
    pqsamp_state state;
    pqsamp_half_batch batch;
    pqsamp_word reconstructed[PQSAMP_VALUE_BITS];
    int16_t expected_y[PQSAMP_LANES];
    uint64_t expected_pre_bits;
    uint64_t expected_finish_bits;
    uint32_t expected_live;
    uint32_t expected_accept;
    uint32_t live;
    uint32_t accept;
    unsigned lane;

    PQSAMP_CHECK(reference_half_stages(
                     params, seed, &expected_live, &expected_accept, expected_y,
                     &expected_pre_bits, &expected_finish_bits) == 0);
    PQSAMP_CHECK(pqsamp_rng_init(&coins, test_randombytes, &source) ==
                 PQSAMP_OK);
    state.shares = 1U;
    state.coins = &coins;
    state.masks = NULL;
    state.stats = NULL;
    PQSAMP_CHECK(pqsamp_sample_half_pre(&state, &batch, &live, params) ==
                 PQSAMP_OK);
    PQSAMP_CHECK(live == expected_live);
    PQSAMP_CHECK(coins.bits_used == expected_pre_bits);
    PQSAMP_CHECK(pqsamp_sample_half_finish(&state, &batch, live, &accept,
                                           params) == PQSAMP_OK);
    PQSAMP_CHECK(accept == expected_accept);
    PQSAMP_CHECK(coins.bits_used == expected_finish_bits);
    PQSAMP_CHECK(pqsamp_half_reconstruct(&state, reconstructed, &batch.value) ==
                 PQSAMP_OK);
    for (lane = 0; lane < PQSAMP_LANES; lane++)
    {
      if (((expected_live | expected_accept) & (UINT32_C(1) << lane)) != 0U)
      {
        uint16_t bits =
            (uint16_t)lane_value(reconstructed, PQSAMP_VALUE_BITS, lane, 1U);
        int16_t actual = bits > (uint16_t)INT16_MAX
                             ? (int16_t)((int32_t)bits - INT32_C(65536))
                             : (int16_t)bits;

        PQSAMP_CHECK(actual == expected_y[lane]);
      }
    }
  }
  return 0;
}

static int sample_tape(pqsamp_masked_i16 *out, size_t n, pqsamp_center center,
                       pqsamp_stats *stats, pqsamp_trace *trace)
{
  test_rng coin_source = test_rng_make(UINT64_C(0x9216d5d98979fb1b));
  test_rng mask_source = test_rng_make(UINT64_C(0xd1310ba698dfb5ac));
  pqsamp_rng coins;
  pqsamp_rng masks;

  PQSAMP_CHECK(pqsamp_rng_init(&coins, test_randombytes, &coin_source) ==
               PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_rng_init(&masks, test_randombytes, &mask_source) ==
               PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_sample_masked_trace(out, n, PQSAMP_PROFILE_S3_2, center,
                                          2U, &coins, &masks, stats,
                                          trace) == PQSAMP_OK);
  return 0;
}

static int prefix_pair(size_t short_n, size_t long_n, pqsamp_center center)
{
  pqsamp_masked_i16 short_out[65];
  pqsamp_masked_i16 long_out[65];
  size_t i;

  PQSAMP_CHECK(sample_tape(short_out, short_n, center, NULL, NULL) == 0);
  PQSAMP_CHECK(sample_tape(long_out, long_n, center, NULL, NULL) == 0);
  for (i = 0; i < short_n; i++)
  {
    PQSAMP_CHECK(test_reconstruct(&short_out[i], 2U) ==
                 test_reconstruct(&long_out[i], 2U));
  }
  return 0;
}

static int scheduler_boundaries(void)
{
  static const size_t counts[] = {0U, 1U, 31U, 32U, 33U, 63U, 64U, 65U};
  unsigned center;

  for (center = 0; center < 2U; center++)
  {
    unsigned i;

    for (i = 0; i < sizeof(counts) / sizeof(counts[0]); i++)
    {
      pqsamp_masked_i16 out[65];
      pqsamp_stats stats;
      pqsamp_trace trace;
      size_t j;

      PQSAMP_CHECK(sample_tape(counts[i] == 0U ? NULL : out, counts[i],
                               (pqsamp_center)center, &stats, &trace) == 0);
      if (counts[i] == 0U)
      {
        PQSAMP_CHECK(stats.candidate_batches == 0U);
        PQSAMP_CHECK(stats.coin_bits == 0U);
        PQSAMP_CHECK(stats.mask_bits == 0U);
        PQSAMP_CHECK(trace.raw_side_batches == 0U);
        PQSAMP_CHECK(trace.finished_batches == 0U);
        PQSAMP_CHECK(trace.reconstruction_batches == 0U);
      }
      else
      {
        size_t blocks = counts[i] / PQSAMP_LANES +
                        (counts[i] % PQSAMP_LANES != 0U ? 1U : 0U);

        PQSAMP_CHECK(stats.candidate_batches <=
                     blocks * PQSAMP_BATCHES_PER_BLOCK);
        PQSAMP_CHECK(stats.candidates ==
                     stats.candidate_batches * PQSAMP_LANES);
        if (center == PQSAMP_CENTER_ZERO)
        {
          PQSAMP_CHECK(trace.reconstruction_batches == 0U);
          PQSAMP_CHECK(stats.sec_and_calls == 95U * stats.candidate_batches +
                                                  trace.raw_side_batches +
                                                  52U * trace.finished_batches);
        }
        else
        {
          PQSAMP_CHECK(trace.raw_side_batches == 0U);
          PQSAMP_CHECK(trace.reconstruction_batches == blocks);
          PQSAMP_CHECK(stats.sec_and_calls ==
                       89U * stats.candidate_batches +
                           51U * trace.finished_batches +
                           3U * trace.reconstruction_batches);
        }
      }
      for (j = 0; j < counts[i]; j++)
      {
        int16_t value = test_reconstruct(&out[j], 2U);

        PQSAMP_CHECK(value >= -13 && value <= 13);
        PQSAMP_CHECK(out[j].share[2] == 0U);
        PQSAMP_CHECK(out[j].share[3] == 0U);
      }
    }
    PQSAMP_CHECK(prefix_pair(31U, 32U, (pqsamp_center)center) == 0);
    PQSAMP_CHECK(prefix_pair(32U, 33U, (pqsamp_center)center) == 0);
    PQSAMP_CHECK(prefix_pair(64U, 65U, (pqsamp_center)center) == 0);
  }
  return 0;
}

static int half_same_seed(void)
{
  static const uint32_t expected_hash[2] = {UINT32_C(3346943642),
                                            UINT32_C(935370197)};
  static const uint64_t expected_accepted[2] = {81U, 83U};
  static const uint64_t expected_coin_bits[2] = {14880U, 14496U};
  unsigned profile;

  for (profile = 0; profile < 2U; profile++)
  {
    test_rng source = test_rng_make(UINT64_C(0x5be0cd19137e2179) + profile);
    pqsamp_rng coins;
    pqsamp_masked_i16 out[65];
    pqsamp_stats stats;
    pqsamp_trace trace;
    uint32_t hash = UINT32_C(5381);
    unsigned i;

    PQSAMP_CHECK(pqsamp_rng_init(&coins, test_randombytes, &source) ==
                 PQSAMP_OK);
    PQSAMP_CHECK(pqsamp_sample_masked_trace(out, 65U, (pqsamp_profile)profile,
                                            PQSAMP_CENTER_HALF, 1U, &coins,
                                            NULL, &stats, &trace) == PQSAMP_OK);
    for (i = 0; i < 65U; i++)
    {
      int16_t value = test_reconstruct(&out[i], 1U);

      hash = hash * 33U + (uint32_t)((int32_t)value + INT32_C(32768));
    }
    PQSAMP_CHECK(hash == expected_hash[profile]);
    PQSAMP_CHECK(stats.candidate_batches == 4U);
    PQSAMP_CHECK(stats.accepted == expected_accepted[profile]);
    PQSAMP_CHECK(stats.coin_bits == expected_coin_bits[profile]);
    PQSAMP_CHECK(trace.finished_batches == 3U);
    PQSAMP_CHECK(trace.reconstruction_batches == 3U);
    PQSAMP_CHECK(stats.sec_and_calls ==
                 (profile == 0U ? UINT64_C(518) : UINT64_C(506)));
  }
  return 0;
}

static int zero_randombytes(void *context, uint8_t *out, size_t n)
{
  size_t i;

  (void)context;
  for (i = 0; i < n; i++)
  {
    out[i] = 0;
  }
  return 0;
}

typedef struct
{
  uint8_t byte;
  size_t calls;
  size_t fail_after;
} constant_source;

static int constant_randombytes(void *context, uint8_t *out, size_t n)
{
  constant_source *source = context;
  size_t i;

  if (source->calls >= source->fail_after)
  {
    return -1;
  }
  source->calls++;
  for (i = 0; i < n; i++)
  {
    out[i] = source->byte;
  }
  return 0;
}

static void fill_masked_output(pqsamp_masked_i16 *out, size_t n)
{
  size_t i;

  for (i = 0; i < n; i++)
  {
    unsigned share;

    for (share = 0; share < PQSAMP_MAX_SHARES; share++)
    {
      out[i].share[share] =
          (uint16_t)(UINT16_C(0x5a00) + (uint16_t)i + (uint16_t)share);
    }
  }
}

static int output_is_zero(const pqsamp_masked_i16 *out, size_t n)
{
  size_t i;

  for (i = 0; i < n; i++)
  {
    unsigned share;

    for (share = 0; share < PQSAMP_MAX_SHARES; share++)
    {
      if (out[i].share[share] != 0U)
      {
        return 0;
      }
    }
  }
  return 1;
}

static int zero_side_failures(void)
{
  enum
  {
    SAMPLE_COUNT = 33
  };
  unsigned random_failure;

  for (random_failure = 0; random_failure < 2U; random_failure++)
  {
    constant_source coin_source = {UINT8_MAX, 0,
                                   random_failure != 0U ? 63U : (size_t)-1};
    pqsamp_rng coins;
    pqsamp_masked_i16 out[SAMPLE_COUNT];
    pqsamp_stats stats;
    pqsamp_trace trace;
    int expected = random_failure != 0U ? PQSAMP_ERR_RANDOM : PQSAMP_ERR_BOUND;

    fill_masked_output(out, SAMPLE_COUNT);
    PQSAMP_CHECK(pqsamp_rng_init(&coins, constant_randombytes, &coin_source) ==
                 PQSAMP_OK);
    PQSAMP_CHECK(pqsamp_sample_masked_trace(
                     out, SAMPLE_COUNT, PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_ZERO,
                     1U, &coins, NULL, &stats, &trace) == expected);
    PQSAMP_CHECK(output_is_zero(out, SAMPLE_COUNT) != 0);
    PQSAMP_CHECK(trace.raw_side_batches == (random_failure != 0U ? 63U : 64U));
    PQSAMP_CHECK(trace.finished_batches == 0U);
    PQSAMP_CHECK(stats.candidate_batches == 0U);
    PQSAMP_CHECK(stats.sec_and_calls == trace.raw_side_batches);
    PQSAMP_CHECK(coins.error ==
                 (random_failure != 0U ? PQSAMP_ERR_RANDOM : PQSAMP_OK));
  }

  for (random_failure = 0; random_failure < 2U; random_failure++)
  {
    constant_source coin_source = {0, 0, 0};
    pqsamp_rng coins;
    pqsamp_masked_i16 out[1];
    pqsamp_stats stats;
    pqsamp_trace trace;

    fill_masked_output(out, 1U);
    PQSAMP_CHECK(pqsamp_rng_init(&coins, constant_randombytes, &coin_source) ==
                 PQSAMP_OK);
    if (random_failure != 0U)
    {
      coins.buffer = 0;
      coins.available = 32U;
    }
    PQSAMP_CHECK(pqsamp_sample_masked_trace(
                     out, 1U, PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_ZERO, 1U,
                     &coins, NULL, &stats, &trace) == PQSAMP_ERR_RANDOM);
    PQSAMP_CHECK(output_is_zero(out, 1U) != 0);
    PQSAMP_CHECK(trace.raw_side_batches == 0U);
    PQSAMP_CHECK(stats.sec_and_calls == 0U);
    PQSAMP_CHECK(coins.error == PQSAMP_ERR_RANDOM);
  }

  for (random_failure = 0; random_failure < 4U; random_failure++)
  {
    constant_source coin_source = {0, 0, (size_t)-1};
    constant_source mask_source = {0, 0, random_failure < 2U ? 0U : 1U};
    pqsamp_rng coins;
    pqsamp_rng masks;
    pqsamp_masked_i16 out[1];
    pqsamp_stats stats;
    pqsamp_trace trace;

    fill_masked_output(out, 1U);
    PQSAMP_CHECK(pqsamp_rng_init(&coins, constant_randombytes, &coin_source) ==
                 PQSAMP_OK);
    PQSAMP_CHECK(pqsamp_rng_init(&masks, constant_randombytes, &mask_source) ==
                 PQSAMP_OK);
    if ((random_failure & 1U) != 0U)
    {
      masks.buffer = 0;
      masks.available = 32U;
    }
    PQSAMP_CHECK(pqsamp_sample_masked_trace(
                     out, 1U, PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_ZERO, 2U,
                     &coins, &masks, &stats, &trace) == PQSAMP_ERR_RANDOM);
    PQSAMP_CHECK(output_is_zero(out, 1U) != 0);
    PQSAMP_CHECK(trace.raw_side_batches == 0U);
    PQSAMP_CHECK(stats.sec_and_calls == (random_failure == 3U ? 1U : 0U));
    PQSAMP_CHECK(coins.error == PQSAMP_OK);
    PQSAMP_CHECK(masks.error == PQSAMP_ERR_RANDOM);
  }

  {
    constant_source source = {0, 0, (size_t)-1};
    pqsamp_rng coins;
    pqsamp_rng masks;
    pqsamp_masked_i16 out[1];

    fill_masked_output(out, 1U);
    PQSAMP_CHECK(pqsamp_rng_init(&coins, constant_randombytes, &source) ==
                 PQSAMP_OK);
    PQSAMP_CHECK(pqsamp_rng_init(&masks, constant_randombytes, &source) ==
                 PQSAMP_OK);
    PQSAMP_CHECK(pqsamp_sample_masked(out, 1U, PQSAMP_PROFILE_S3_2,
                                      PQSAMP_CENTER_ZERO, 2U, &coins, &masks,
                                      NULL) == PQSAMP_ERR_PARAM);
    PQSAMP_CHECK(output_is_zero(out, 1U) != 0);
    PQSAMP_CHECK(source.calls == 0U);
  }
  return 0;
}

static int zero_output_sizes(void)
{
  static const size_t counts[] = {0U, 1U, 31U, 32U, 33U, 63U, 64U, 65U};
  unsigned shares;

  for (shares = 1U; shares <= PQSAMP_MAX_SHARES; shares++)
  {
    unsigned count_index;

    for (count_index = 0; count_index < sizeof(counts) / sizeof(counts[0]);
         count_index++)
    {
      test_rng coin_source = test_rng_make(UINT64_C(0x243f6a8885a308d3) +
                                           shares * 17U + count_index);
      test_rng mask_source = test_rng_make(UINT64_C(0x13198a2e03707344) +
                                           shares * 23U + count_index);
      pqsamp_rng coins;
      pqsamp_rng masks;
      pqsamp_masked_i16 out[65];
      pqsamp_stats stats;
      pqsamp_trace trace;
      size_t i;

      PQSAMP_CHECK(pqsamp_rng_init(&coins, test_randombytes, &coin_source) ==
                   PQSAMP_OK);
      PQSAMP_CHECK(pqsamp_rng_init(&masks, test_randombytes, &mask_source) ==
                   PQSAMP_OK);
      PQSAMP_CHECK(
          pqsamp_sample_masked_trace(
              counts[count_index] == 0U ? NULL : out, counts[count_index],
              PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_ZERO, shares, &coins,
              shares == 1U ? NULL : &masks, &stats, &trace) == PQSAMP_OK);
      PQSAMP_CHECK(trace.reconstruction_batches == 0U);
      PQSAMP_CHECK(stats.sec_and_calls == 95U * stats.candidate_batches +
                                              trace.raw_side_batches +
                                              52U * trace.finished_batches);
      for (i = 0; i < counts[count_index]; i++)
      {
        int16_t value = test_reconstruct(&out[i], shares);
        unsigned inactive;

        PQSAMP_CHECK(value >= -13 && value <= 13);
        for (inactive = shares; inactive < PQSAMP_MAX_SHARES; inactive++)
        {
          PQSAMP_CHECK(out[i].share[inactive] == 0U);
        }
      }
    }
  }
  return 0;
}

static int finite_failure(void)
{
  enum
  {
    SAMPLE_COUNT = 33
  };
  pqsamp_rng coins;
  pqsamp_masked_i16 out[SAMPLE_COUNT];
  pqsamp_stats stats;
  unsigned i;

  for (i = 0; i < SAMPLE_COUNT; i++)
  {
    unsigned share;

    for (share = 0; share < PQSAMP_MAX_SHARES; share++)
    {
      out[i].share[share] = UINT16_C(0x5a5a);
    }
  }
  PQSAMP_CHECK(pqsamp_rng_init(&coins, zero_randombytes, NULL) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_sample_masked(out, SAMPLE_COUNT, PQSAMP_PROFILE_S3_2,
                                    PQSAMP_CENTER_ZERO, 1U, &coins, NULL,
                                    &stats) == PQSAMP_ERR_BOUND);
  PQSAMP_CHECK(stats.candidate_batches == 8U);
  PQSAMP_CHECK(stats.candidates == 256U);
  PQSAMP_CHECK(coins.error == PQSAMP_OK);
  for (i = 0; i < SAMPLE_COUNT; i++)
  {
    unsigned share;

    for (share = 0; share < PQSAMP_MAX_SHARES; share++)
    {
      PQSAMP_CHECK(out[i].share[share] == 0U);
    }
  }
  return 0;
}

static int masked_samples(unsigned shares)
{
  test_rng coin_source = test_rng_make(UINT64_C(0xd1b54a32d192ed03) + shares);
  test_rng mask_source = test_rng_make(UINT64_C(0x94d049bb133111eb) + shares);
  pqsamp_rng coins;
  pqsamp_rng masks;
  pqsamp_stats stats;
  pqsamp_masked_i16 sample[64];
  unsigned i;

  PQSAMP_CHECK(pqsamp_rng_init(&coins, test_randombytes, &coin_source) ==
               PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_rng_init(&masks, test_randombytes, &mask_source) ==
               PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_sample_masked(sample, 64, PQSAMP_PROFILE_S3_2,
                                    PQSAMP_CENTER_HALF, shares, &coins, &masks,
                                    &stats) == PQSAMP_OK);
  PQSAMP_CHECK(stats.candidate_batches > 0U);
  PQSAMP_CHECK(stats.candidate_batches <= 8U);
  PQSAMP_CHECK(stats.candidates == stats.candidate_batches * PQSAMP_LANES);
  PQSAMP_CHECK(stats.accepted >= 64U);
  PQSAMP_CHECK(stats.sec_and_calls != 0U);
  PQSAMP_CHECK(stats.mask_bits != 0U);
  for (i = 0; i < 64U; i++)
  {
    int16_t value = test_reconstruct(&sample[i], shares);

    PQSAMP_CHECK(value >= -13 && value <= 13);
  }
  return 0;
}

static int masked_share_differential(void)
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
      uint64_t coin_seed =
          UINT64_C(0x243f6a8885a308d3) + profile * 11U + center;
      test_rng reference_source = test_rng_make(coin_seed);
      pqsamp_rng reference_coins;
      pqsamp_masked_i16 reference[SAMPLE_COUNT];
      pqsamp_stats reference_stats;
      unsigned shares;

      PQSAMP_CHECK(pqsamp_rng_init(&reference_coins, test_randombytes,
                                   &reference_source) == PQSAMP_OK);
      PQSAMP_CHECK(
          pqsamp_sample_masked(reference, SAMPLE_COUNT, (pqsamp_profile)profile,
                               (pqsamp_center)center, 1U, &reference_coins,
                               NULL, &reference_stats) == PQSAMP_OK);
      for (shares = 2; shares <= PQSAMP_MAX_SHARES; shares++)
      {
        test_rng coin_source = test_rng_make(coin_seed);
        test_rng mask_source =
            test_rng_make(UINT64_C(0x13198a2e03707344) + profile * 31U +
                          center * 7U + shares);
        pqsamp_rng coins;
        pqsamp_rng masks;
        pqsamp_masked_i16 sample[SAMPLE_COUNT];
        pqsamp_stats sample_stats;
        unsigned i;

        PQSAMP_CHECK(pqsamp_rng_init(&coins, test_randombytes, &coin_source) ==
                     PQSAMP_OK);
        PQSAMP_CHECK(pqsamp_rng_init(&masks, test_randombytes, &mask_source) ==
                     PQSAMP_OK);
        PQSAMP_CHECK(pqsamp_sample_masked(sample, SAMPLE_COUNT,
                                          (pqsamp_profile)profile,
                                          (pqsamp_center)center, shares, &coins,
                                          &masks, &sample_stats) == PQSAMP_OK);
        PQSAMP_CHECK(coins.bits_used == reference_coins.bits_used);
        PQSAMP_CHECK(sample_stats.candidate_batches ==
                     reference_stats.candidate_batches);
        PQSAMP_CHECK(sample_stats.candidates == reference_stats.candidates);
        PQSAMP_CHECK(sample_stats.accepted == reference_stats.accepted);
        PQSAMP_CHECK(sample_stats.sec_and_calls ==
                     reference_stats.sec_and_calls);
        PQSAMP_CHECK(sample_stats.coin_bits == reference_stats.coin_bits);
        for (i = 0; i < SAMPLE_COUNT; i++)
        {
          unsigned inactive;

          PQSAMP_CHECK(test_reconstruct(&sample[i], shares) ==
                       test_reconstruct(&reference[i], 1U));
          for (inactive = shares; inactive < PQSAMP_MAX_SHARES; inactive++)
          {
            PQSAMP_CHECK(sample[i].share[inactive] == 0U);
          }
        }
      }
    }
  }
  return 0;
}

static int rng_failure(void)
{
  test_rng source = test_rng_make(7U);
  pqsamp_rng rng;
  int16_t out[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  unsigned i;

  source.fail_after = 0;
  PQSAMP_CHECK(pqsamp_rng_init(&rng, test_randombytes, &source) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_sample(out, 8, PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_ZERO,
                             &rng, NULL) == PQSAMP_ERR_RANDOM);
  for (i = 0; i < 8U; i++)
  {
    PQSAMP_CHECK(out[i] == 0);
  }
  return 0;
}

static int masked_rng_failure(void)
{
  enum
  {
    SAMPLE_COUNT = 33,
    SHARES = 3
  };
  test_rng probe_coin_source = test_rng_make(UINT64_C(0xa4093822299f31d0));
  test_rng probe_mask_source = test_rng_make(UINT64_C(0x082efa98ec4e6c89));
  pqsamp_rng probe_coins;
  pqsamp_rng probe_masks;
  pqsamp_masked_i16 probe[SAMPLE_COUNT];
  unsigned fail_masks;

  PQSAMP_CHECK(pqsamp_rng_init(&probe_coins, test_randombytes,
                               &probe_coin_source) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_rng_init(&probe_masks, test_randombytes,
                               &probe_mask_source) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_sample_masked(probe, SAMPLE_COUNT,
                                    PQSAMP_PROFILE_S1521_1000,
                                    PQSAMP_CENTER_HALF, SHARES, &probe_coins,
                                    &probe_masks, NULL) == PQSAMP_OK);

  for (fail_masks = 0; fail_masks < 2U; fail_masks++)
  {
    test_rng coin_source = test_rng_make(UINT64_C(0xa4093822299f31d0));
    test_rng mask_source = test_rng_make(UINT64_C(0x082efa98ec4e6c89));
    pqsamp_rng coins;
    pqsamp_rng masks;
    pqsamp_masked_i16 out[SAMPLE_COUNT];
    unsigned i;

    if (fail_masks != 0U)
    {
      mask_source.fail_after = probe_mask_source.calls / 2U;
    }
    else
    {
      coin_source.fail_after = probe_coin_source.calls / 2U;
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
    PQSAMP_CHECK(pqsamp_rng_init(&coins, test_randombytes, &coin_source) ==
                 PQSAMP_OK);
    PQSAMP_CHECK(pqsamp_rng_init(&masks, test_randombytes, &mask_source) ==
                 PQSAMP_OK);
    PQSAMP_CHECK(pqsamp_sample_masked(out, SAMPLE_COUNT,
                                      PQSAMP_PROFILE_S1521_1000,
                                      PQSAMP_CENTER_HALF, SHARES, &coins,
                                      &masks, NULL) == PQSAMP_ERR_RANDOM);
    PQSAMP_CHECK((fail_masks != 0U ? masks.error : coins.error) ==
                 PQSAMP_ERR_RANDOM);
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

typedef struct
{
  test_rng rng;
  const pqsamp_stats *stats;
  const pqsamp_trace *trace;
  uint64_t gate;
} increment_source;

static int increment_randombytes(void *context, uint8_t *out, size_t n)
{
  increment_source *source = context;

  if (source->trace->finished_batches == 1U &&
      source->trace->reconstruction_batches == 0U &&
      source->stats->sec_and_calls == 140U + source->gate)
  {
    return -1;
  }
  return test_randombytes(&source->rng, out, n);
}

static void half_accept_source(word_source *source)
{
  const pqsamp_params *params =
      pqsamp_profile_get(PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_HALF);
  unsigned i;

  source->count = 0;
  source->offset = 0;
  push_side(source, params, 0U);
  for (i = 0; i < params->geom_count; i++)
  {
    push_word(source, i == 0U ? UINT32_MAX : 0U);
  }
  for (i = 0; i < params->k_sat; i++)
  {
    push_word(source, i == 1U ? UINT32_MAX : 0U);
  }
  push_value(source, 0U, params->threshold_bits);
  if ((source->count & 1U) != 0U)
  {
    push_word(source, 0U);
  }
}

static int reconstruction_rng_failure(void)
{
  uint64_t gate;

  for (gate = 0; gate < 3U; gate++)
  {
    word_source coin_source = {{0}, 0, 0};
    increment_source mask_source;
    pqsamp_rng coins;
    pqsamp_rng masks;
    pqsamp_masked_i16 out[1];
    pqsamp_stats stats;
    pqsamp_trace trace;
    unsigned share;

    half_accept_source(&coin_source);
    mask_source.rng = test_rng_make(UINT64_C(0xbb67ae8584caa73b) + gate);
    mask_source.stats = &stats;
    mask_source.trace = &trace;
    mask_source.gate = gate;
    for (share = 0; share < PQSAMP_MAX_SHARES; share++)
    {
      out[0].share[share] = (uint16_t)(UINT16_C(0x4321) + share);
    }
    PQSAMP_CHECK(pqsamp_rng_init(&coins, word_randombytes, &coin_source) ==
                 PQSAMP_OK);
    PQSAMP_CHECK(pqsamp_rng_init(&masks, increment_randombytes, &mask_source) ==
                 PQSAMP_OK);
    PQSAMP_CHECK(pqsamp_sample_masked_trace(
                     out, 1U, PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_HALF, 4U,
                     &coins, &masks, &stats, &trace) == PQSAMP_ERR_RANDOM);
    PQSAMP_CHECK(coins.error == PQSAMP_OK);
    PQSAMP_CHECK(masks.error == PQSAMP_ERR_RANDOM);
    PQSAMP_CHECK(trace.finished_batches == 1U);
    PQSAMP_CHECK(trace.reconstruction_batches == 0U);
    PQSAMP_CHECK(stats.sec_and_calls == 140U + gate);
    for (share = 0; share < PQSAMP_MAX_SHARES; share++)
    {
      PQSAMP_CHECK(out[0].share[share] == 0U);
      out[0].share[share] = UINT16_C(0x5a5a);
    }

    half_accept_source(&coin_source);
    PQSAMP_CHECK(pqsamp_rng_init(&coins, word_randombytes, &coin_source) ==
                 PQSAMP_OK);
    PQSAMP_CHECK(pqsamp_sample_masked_trace(
                     out, 1U, PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_HALF, 4U,
                     &coins, &masks, &stats, &trace) == PQSAMP_ERR_RANDOM);
    PQSAMP_CHECK(coins.error == PQSAMP_OK);
    PQSAMP_CHECK(masks.error == PQSAMP_ERR_RANDOM);
    for (share = 0; share < PQSAMP_MAX_SHARES; share++)
    {
      PQSAMP_CHECK(out[0].share[share] == 0U);
    }
  }
  return 0;
}

int main(void)
{
  unsigned shares;

  PQSAMP_CHECK(proposal_positions() == 0);
  PQSAMP_CHECK(zero_side_raw_pairs() == 0);
  PQSAMP_CHECK(zero_side_compaction() == 0);
  PQSAMP_CHECK(zero_side_fills() == 0);
  PQSAMP_CHECK(zero_side_script() == 0);
  PQSAMP_CHECK(zero_side_law() == 0);
  PQSAMP_CHECK(scalar_masked_semantics() == 0);
  PQSAMP_CHECK(half_profile_cases() == 0);
  PQSAMP_CHECK(half_reconstruction() == 0);
  PQSAMP_CHECK(staged_share_differential() == 0);
  PQSAMP_CHECK(staged_half_differential() == 0);
  PQSAMP_CHECK(half_stage_reference() == 0);
  PQSAMP_CHECK(vectors() == 0);
  PQSAMP_CHECK(
      plain_profile(PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_ZERO, 0.0, 1.622) == 0);
  PQSAMP_CHECK(
      plain_profile(PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_HALF, 0.5, 1.622) == 0);
  PQSAMP_CHECK(plain_profile(PQSAMP_PROFILE_S1521_1000, PQSAMP_CENTER_ZERO, 0.0,
                             1.669) == 0);
  PQSAMP_CHECK(plain_profile(PQSAMP_PROFILE_S1521_1000, PQSAMP_CENTER_HALF, 0.5,
                             1.669) == 0);
  for (shares = 2; shares <= PQSAMP_MAX_SHARES; shares++)
  {
    PQSAMP_CHECK(masked_samples(shares) == 0);
  }
  PQSAMP_CHECK(scheduler_boundaries() == 0);
  PQSAMP_CHECK(half_same_seed() == 0);
  PQSAMP_CHECK(masked_share_differential() == 0);
  PQSAMP_CHECK(finite_failure() == 0);
  PQSAMP_CHECK(zero_side_failures() == 0);
  PQSAMP_CHECK(zero_output_sizes() == 0);
  PQSAMP_CHECK(rng_failure() == 0);
  PQSAMP_CHECK(masked_rng_failure() == 0);
  PQSAMP_CHECK(reconstruction_rng_failure() == 0);
  puts("sampler: ok");
  return 0;
}
