#include "common.h"

#define PQSAMP_REFERENCE_RETRIES 256U

static unsigned pqsamp_ctz32(uint32_t value)
{
#if defined(__GNUC__) || defined(__clang__)
  return (unsigned)__builtin_ctz(value);
#else
  unsigned count = 0;

  while ((value & 1U) == 0U)
  {
    value >>= 1;
    count++;
  }
  return count;
#endif
}

static int pqsamp_reference_side(pqsamp_rng *rng, const pqsamp_params *params,
                                 unsigned *side)
{
  unsigned bits = pqsamp_bit_width_u32(params->side_den - 1U);
  unsigned round;

  for (round = 0; round < PQSAMP_SIDE_RETRIES; round++)
  {
    uint32_t value;
    int ret = pqsamp_rng_bits(rng, bits, &value);

    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    if (value < params->side_den)
    {
      *side = value < params->side_num ? 1U : 0U;
      return PQSAMP_OK;
    }
  }
  return PQSAMP_ERR_BOUND;
}

static int pqsamp_reference_geom_proposal(pqsamp_rng *rng, unsigned bits,
                                          unsigned *value)
{
  unsigned round;

  for (round = 0; round < PQSAMP_SIDE_RETRIES; round++)
  {
    unsigned offset = 0;
    unsigned found = bits;

    while (offset < bits)
    {
      uint32_t word;
      unsigned take = bits - offset;
      int ret;

      if (take > 32U)
      {
        take = 32U;
      }
      ret = pqsamp_rng_bits(rng, take, &word);

      if (ret != PQSAMP_OK)
      {
        return ret;
      }
      if (found == bits && word != 0U)
      {
        found = offset + pqsamp_ctz32(word);
      }
      offset += take;
    }
    if (found != bits)
    {
      *value = found;
      return PQSAMP_OK;
    }
  }
  return PQSAMP_ERR_BOUND;
}

static int pqsamp_reference_geom_sat(pqsamp_rng *rng, unsigned saturation,
                                     unsigned *value)
{
  unsigned i;

  for (i = 0; i < saturation; i++)
  {
    uint32_t bit;
    int ret = pqsamp_rng_bits(rng, 1U, &bit);

    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    if (bit != 0U)
    {
      *value = i;
      return PQSAMP_OK;
    }
  }
  *value = saturation;
  return PQSAMP_OK;
}

static int pqsamp_reference_sample(int16_t *out, const pqsamp_params *params,
                                   pqsamp_rng *rng, pqsamp_stats *stats)
{
  unsigned attempt;

  for (attempt = 0; attempt < PQSAMP_REFERENCE_RETRIES; attempt++)
  {
    const pqsamp_candidate *candidate;
    unsigned side;
    unsigned geometric;
    unsigned k;
    int ret = pqsamp_reference_side(rng, params, &side);

    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    ret = pqsamp_reference_geom_proposal(rng, params->geom_count, &geometric);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    candidate = &params->side[side][geometric];
    if (stats != NULL)
    {
      stats->candidates++;
    }
    if (candidate->valid == 0U)
    {
      continue;
    }
    ret = pqsamp_reference_geom_sat(rng, params->k_sat, &k);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    if (k < candidate->quotient)
    {
      continue;
    }
    if (k == candidate->quotient)
    {
      uint64_t uniform;

      ret = pqsamp_rng_bits64(rng, params->threshold_bits, &uniform);
      if (ret != PQSAMP_OK)
      {
        return ret;
      }
      if (uniform >= candidate->boundary_count)
      {
        continue;
      }
    }
    *out = candidate->y;
    if (stats != NULL)
    {
      stats->accepted++;
    }
    return PQSAMP_OK;
  }
  return PQSAMP_ERR_BOUND;
}

static void pqsamp_stats_clear(pqsamp_stats *stats)
{
  if (stats != NULL)
  {
    stats->candidate_batches = 0;
    stats->candidates = 0;
    stats->accepted = 0;
    stats->sec_and_calls = 0;
    stats->coin_bits = 0;
    stats->mask_bits = 0;
  }
}

static void pqsamp_clear_plain(int16_t *out, size_t count)
{
  size_t i;

  for (i = 0; i < count; i++)
  {
    out[i] = 0;
  }
}

int pqsamp_generate(int16_t *out, size_t count, const pqsamp_params *params,
                    pqsamp_rng *coins, pqsamp_stats *stats)
{
  uint64_t before;
  size_t i;
  int ret;

  pqsamp_stats_clear(stats);
  if ((out == NULL && count != 0U) || coins == NULL)
  {
    if (out != NULL)
    {
      pqsamp_clear_plain(out, count);
    }
    return PQSAMP_ERR_PARAM;
  }
  ret = pqsamp_params_check(params);
  if (ret != PQSAMP_OK)
  {
    if (out != NULL)
    {
      pqsamp_clear_plain(out, count);
    }
    return ret;
  }
  before = coins->bits_used;
  for (i = 0; i < count; i++)
  {
    ret = pqsamp_reference_sample(&out[i], params, coins, stats);
    if (ret != PQSAMP_OK)
    {
      pqsamp_clear_plain(out, count);
      if (stats != NULL)
      {
        stats->coin_bits = coins->bits_used - before;
      }
      return ret;
    }
  }
  if (stats != NULL)
  {
    stats->coin_bits = coins->bits_used - before;
  }
  return PQSAMP_OK;
}
