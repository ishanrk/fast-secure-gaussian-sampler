#include "internal.h"

#define PQSAMP_SCALAR_RETRIES 256U

static unsigned ctz32(uint32_t x)
{
#if defined(__GNUC__) || defined(__clang__)
  return (unsigned)__builtin_ctz(x);
#else
  unsigned n = 0;

  while ((x & 1U) == 0U)
  {
    x >>= 1;
    n++;
  }
  return n;
#endif
}

static int draw_side(pqsamp_rng *rng, const pqsamp_params *params,
                     unsigned *side)
{
  unsigned bits = pqsamp_bit_width_u32(params->side_den - 1U);
  unsigned i;

  for (i = 0; i < PQSAMP_SIDE_RETRIES; i++)
  {
    uint32_t x;
    int rc = pqsamp_rng_bits(rng, bits, &x);

    if (rc != PQSAMP_OK)
    {
      return rc;
    }
    if (x < params->side_den)
    {
      *side = x < params->side_num ? 1U : 0U;
      return PQSAMP_OK;
    }
  }
  return PQSAMP_ERR_BOUND;
}

int pqsamp_scalar_geom(pqsamp_rng *rng, unsigned bits, unsigned *value)
{
  unsigned offset = 0;
  unsigned found = bits;

  while (offset < bits)
  {
    uint32_t word;
    unsigned take = bits - offset;
    int rc;

    if (take > 32U)
    {
      take = 32U;
    }
    rc = pqsamp_rng_bits(rng, take, &word);
    if (rc != PQSAMP_OK)
    {
      return rc;
    }
    if (found == bits && word != 0U)
    {
      found = offset + ctz32(word);
    }
    offset += take;
  }
  // all zero means invalid proposal
  *value = found;
  return PQSAMP_OK;
}

static int draw_geom_sat(pqsamp_rng *rng, unsigned saturation, unsigned *value)
{
  unsigned i;

  for (i = 0; i < saturation; i++)
  {
    uint32_t bit;
    int rc = pqsamp_rng_bits(rng, 1U, &bit);

    if (rc != PQSAMP_OK)
    {
      return rc;
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

static int sample_one(int16_t *out, const pqsamp_params *params,
                      pqsamp_rng *rng, pqsamp_stats *stats)
{
  unsigned attempt;

  for (attempt = 0; attempt < PQSAMP_SCALAR_RETRIES; attempt++)
  {
    const pqsamp_candidate *entry;
    unsigned side;
    unsigned geometric;
    unsigned k;
    uint64_t u = 0;
    int rc = draw_side(rng, params, &side);

    if (rc != PQSAMP_OK)
    {
      return rc;
    }
    rc = pqsamp_scalar_geom(rng, params->geom_count, &geometric);
    if (rc != PQSAMP_OK)
    {
      return rc;
    }
    if (stats != NULL)
    {
      stats->candidates++;
    }
    if (geometric == params->geom_count)
    {
      continue;
    }
    entry = &params->side[side][geometric];
    if (entry->valid == 0U)
    {
      continue;
    }
    rc = draw_geom_sat(rng, params->k_sat, &k);
    if (rc != PQSAMP_OK)
    {
      return rc;
    }
    if (k == entry->quotient)
    {
      rc = pqsamp_rng_bits64(rng, params->threshold_bits, &u);
      if (rc != PQSAMP_OK)
      {
        return rc;
      }
    }
    if (candidate_accept(entry, k, u) == 0)
    {
      continue;
    }
    *out = entry->y;
    if (stats != NULL)
    {
      stats->accepted++;
    }
    return PQSAMP_OK;
  }
  return PQSAMP_ERR_BOUND;
}

static void clear_plain(int16_t *out, size_t n)
{
  size_t i;

  for (i = 0; i < n; i++)
  {
    out[i] = 0;
  }
}

int pqsamp_sample(int16_t *out, size_t n, pqsamp_profile profile,
                  pqsamp_center center, pqsamp_rng *rng, pqsamp_stats *stats)
{
  const pqsamp_params *params = pqsamp_profile_get(profile, center);
  uint64_t before;
  size_t i;
  int rc;

  stats_clear(stats);
  if ((out == NULL && n != 0U) || rng == NULL)
  {
    if (out != NULL)
    {
      clear_plain(out, n);
    }
    return PQSAMP_ERR_PARAM;
  }
  rc = pqsamp_profile_check(params);
  if (rc != PQSAMP_OK)
  {
    if (out != NULL)
    {
      clear_plain(out, n);
    }
    return rc;
  }
  before = rng->bits_used;
  for (i = 0; i < n; i++)
  {
    rc = sample_one(&out[i], params, rng, stats);
    if (rc != PQSAMP_OK)
    {
      clear_plain(out, n);
      if (stats != NULL)
      {
        stats->coin_bits = rng->bits_used - before;
      }
      return rc;
    }
  }
  if (stats != NULL)
  {
    stats->coin_bits = rng->bits_used - before;
  }
  return PQSAMP_OK;
}
