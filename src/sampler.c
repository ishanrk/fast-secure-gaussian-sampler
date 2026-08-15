#include "common.h"

static void pqsamp_set_public(pqsamp_word *out, unsigned bits, uint64_t value)
{
  unsigned bit;

  for (bit = 0; bit < bits; bit++)
  {
    pqsamp_word_zero(&out[bit]);
    if (((value >> bit) & 1U) != 0U)
    {
      out[bit].share[0] = UINT32_MAX;
    }
  }
}

static void pqsamp_select_public(pqsamp_word *out, unsigned bits,
                                 uint64_t value, const pqsamp_word *selector,
                                 unsigned shares)
{
  unsigned bit;

  for (bit = 0; bit < bits; bit++)
  {
    if (((value >> bit) & 1U) != 0U)
    {
      unsigned share;

      for (share = 0; share < shares; share++)
      {
        out[bit].share[share] ^= selector->share[share];
      }
    }
  }
}

static int pqsamp_sec_bernoulli(pqsamp_state *state, pqsamp_word *out,
                                uint32_t numerator, uint32_t denominator)
{
  pqsamp_word bound[32];
  pqsamp_word threshold[32];
  uint32_t pending = UINT32_MAX;
  unsigned bits;
  unsigned round;

  if (numerator == 1U && denominator == 2U)
  {
    return pqsamp_uniform(state, out, 1U);
  }
  bits = pqsamp_bit_width_u32(denominator - 1U);
  pqsamp_set_public(threshold, bits, numerator);
  pqsamp_word_zero(out);
  if ((denominator & (denominator - 1U)) == 0U)
  {
    pqsamp_word uniform[32];
    int ret = pqsamp_uniform(state, uniform, bits);

    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    return pqsamp_sec_lt(state, out, uniform, threshold, bits);
  }
  pqsamp_set_public(bound, bits, denominator);
  for (round = 0; round < PQSAMP_SIDE_RETRIES && pending != 0U; round++)
  {
    pqsamp_word uniform[32];
    pqsamp_word valid;
    pqsamp_word value;
    uint32_t valid_lanes;
    uint32_t take;
    unsigned share;
    int ret = pqsamp_uniform(state, uniform, bits);

    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    ret = pqsamp_sec_lt(state, &valid, uniform, bound, bits);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    ret = pqsamp_sec_lt(state, &value, uniform, threshold, bits);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    ret = pqsamp_unmask(state, &valid, &valid_lanes);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    take = pending & valid_lanes;
    for (share = 0; share < state->shares; share++)
    {
      out->share[share] ^= value.share[share] & take;
    }
    pending &= ~valid_lanes;
  }
  return pending == 0U ? PQSAMP_OK : PQSAMP_ERR_BOUND;
}

static int pqsamp_sec_dlx(pqsamp_state *state, pqsamp_word *y,
                          pqsamp_word *quotient, pqsamp_word *boundary,
                          pqsamp_word *valid, const pqsamp_params *params)
{
  pqsamp_word uniform[PQSAMP_MAX_PLANES];
  pqsamp_word side;
  pqsamp_word previous;
  unsigned q_bits = pqsamp_bit_width_u32(params->k_sat);
  unsigned boundary_bits = params->threshold_bits + 1U;
  unsigned i;
  int ret;

  ret = pqsamp_sec_bernoulli(state, &side, params->side_num, params->side_den);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  ret = pqsamp_uniform(state, uniform, params->geom_count);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  for (i = 0; i < PQSAMP_VALUE_BITS; i++)
  {
    pqsamp_word_zero(&y[i]);
  }
  for (i = 0; i < q_bits; i++)
  {
    pqsamp_word_zero(&quotient[i]);
  }
  for (i = 0; i < boundary_bits; i++)
  {
    pqsamp_word_zero(&boundary[i]);
  }
  pqsamp_word_zero(valid);
  pqsamp_word_not(&previous, &uniform[0], state->shares);

  for (i = 0; i < params->geom_count; i++)
  {
    pqsamp_word indicator;
    pqsamp_word positive;
    pqsamp_word negative;
    const pqsamp_candidate *entry;

    if (i == 0U)
    {
      indicator = uniform[0];
    }
    else
    {
      ret = pqsamp_sec_and(state, &indicator, &previous, &uniform[i]);
      if (ret != PQSAMP_OK)
      {
        return ret;
      }
      pqsamp_word_xor(&previous, &previous, &indicator, state->shares);
    }
    ret = pqsamp_sec_and(state, &positive, &indicator, &side);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    pqsamp_word_xor(&negative, &indicator, &positive, state->shares);

    entry = &params->side[0][i];
    pqsamp_select_public(y, PQSAMP_VALUE_BITS, (uint16_t)entry->y, &negative,
                         state->shares);
    pqsamp_select_public(quotient, q_bits, entry->quotient, &negative,
                         state->shares);
    pqsamp_select_public(boundary, boundary_bits, entry->boundary_count,
                         &negative, state->shares);
    if (entry->valid != 0U)
    {
      pqsamp_word_xor(valid, valid, &negative, state->shares);
    }

    entry = &params->side[1][i];
    pqsamp_select_public(y, PQSAMP_VALUE_BITS, (uint16_t)entry->y, &positive,
                         state->shares);
    pqsamp_select_public(quotient, q_bits, entry->quotient, &positive,
                         state->shares);
    pqsamp_select_public(boundary, boundary_bits, entry->boundary_count,
                         &positive, state->shares);
    if (entry->valid != 0U)
    {
      pqsamp_word_xor(valid, valid, &positive, state->shares);
    }
  }
  return PQSAMP_OK;
}

static int pqsamp_sec_geom_sat(pqsamp_state *state, pqsamp_word *out,
                               unsigned saturation)
{
  pqsamp_word uniform[PQSAMP_MAX_PLANES];
  pqsamp_word previous;
  unsigned bits = pqsamp_bit_width_u32(saturation);
  unsigned i;
  int ret = pqsamp_uniform(state, uniform, saturation);

  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  for (i = 0; i < bits; i++)
  {
    pqsamp_word_zero(&out[i]);
  }
  pqsamp_word_not(&previous, &uniform[0], state->shares);
  for (i = 1; i < saturation; i++)
  {
    pqsamp_word indicator;

    ret = pqsamp_sec_and(state, &indicator, &previous, &uniform[i]);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    pqsamp_select_public(out, bits, i, &indicator, state->shares);
    pqsamp_word_xor(&previous, &previous, &indicator, state->shares);
  }
  pqsamp_select_public(out, bits, saturation, &previous, state->shares);
  return PQSAMP_OK;
}

static unsigned pqsamp_popcount32(uint32_t value)
{
#if defined(__GNUC__) || defined(__clang__)
  return (unsigned)__builtin_popcount(value);
#else
  unsigned count = 0;

  while (value != 0U)
  {
    value &= value - 1U;
    count++;
  }
  return count;
#endif
}

int pqsamp_sample_batch(pqsamp_state *state, pqsamp_word *out, uint32_t *accept,
                        const pqsamp_params *params)
{
  pqsamp_word quotient[8];
  pqsamp_word boundary[PQSAMP_MAX_PLANES];
  pqsamp_word valid;
  pqsamp_word k[8];
  pqsamp_word early;
  pqsamp_word equal;
  pqsamp_word uniform[PQSAMP_MAX_PLANES];
  pqsamp_word boundary_accept;
  pqsamp_word boundary_reject;
  pqsamp_word raw_accept;
  uint32_t valid_lanes;
  uint32_t early_lanes;
  uint32_t accept_lanes;
  unsigned q_bits = pqsamp_bit_width_u32(params->k_sat);
  unsigned threshold_bits = params->threshold_bits;
  int ret = pqsamp_sec_dlx(state, out, quotient, boundary, &valid, params);

  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  ret = pqsamp_sec_geom_sat(state, k, params->k_sat);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  ret = pqsamp_sec_lt(state, &early, k, quotient, q_bits);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  ret = pqsamp_sec_eq(state, &equal, k, quotient, q_bits);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  ret = pqsamp_uniform(state, uniform, threshold_bits);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  pqsamp_word_zero(&uniform[threshold_bits]);
  ret = pqsamp_sec_lt(state, &boundary_accept, uniform, boundary,
                      threshold_bits + 1U);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  /* Reject K < q; at K = q, accept exactly when U < boundary. */
  pqsamp_word_not(&boundary_reject, &boundary_accept, state->shares);
  ret = pqsamp_sec_and(state, &raw_accept, &equal, &boundary_reject);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  pqsamp_word_not(&raw_accept, &raw_accept, state->shares);
  ret = pqsamp_unmask(state, &valid, &valid_lanes);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  ret = pqsamp_unmask(state, &early, &early_lanes);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  ret = pqsamp_unmask(state, &raw_accept, &accept_lanes);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  *accept = valid_lanes & ~early_lanes & accept_lanes;
  if (state->stats != NULL)
  {
    state->stats->candidate_batches++;
    state->stats->candidates += PQSAMP_LANES;
    state->stats->accepted += pqsamp_popcount32(*accept);
  }
  return PQSAMP_OK;
}
