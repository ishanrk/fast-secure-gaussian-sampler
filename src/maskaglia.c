#include "internal.h"

static void select_public(pqsamp_word *out, unsigned bits, uint64_t value,
                          const pqsamp_word *selector, unsigned shares)
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

void pqsamp_zero_side_pool_init(pqsamp_zero_side_pool *pool)
{
  pqsamp_word_zero(&pool->word[0]);
  pqsamp_word_zero(&pool->word[1]);
  pool->fill = 0;
}

static void zero_side_append(pqsamp_zero_side_pool *pool,
                             const pqsamp_word *value, uint32_t valid,
                             unsigned shares)
{
  while (valid != 0U)
  {
    unsigned src = pqsamp_ctz32(valid);
    unsigned dst = pool->fill;
    unsigned word = dst / PQSAMP_LANES;
    unsigned bit = dst % PQSAMP_LANES;
    uint32_t mask = UINT32_C(1) << bit;
    unsigned share;

    for (share = 0; share < shares; share++)
    {
      uint32_t selected = (value->share[share] >> src) & 1U;

      pool->word[word].share[share] =
          (pool->word[word].share[share] & ~mask) | (selected << bit);
    }
    pool->fill++;
    valid &= valid - 1U;
  }
}

int pqsamp_zero_side(pqsamp_state *state, pqsamp_zero_side_pool *pool,
                     pqsamp_word *out, pqsamp_trace *trace)
{
  unsigned round;

  for (round = 0; round < PQSAMP_SIDE_RETRIES && pool->fill < PQSAMP_LANES;
       round++)
  {
    pqsamp_word uniform[2];
    pqsamp_word bad;
    pqsamp_word valid;
    uint32_t valid_lanes;
    int ret = pqsamp_uniform(state, uniform, 2U);

    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    ret = pqsamp_sec_and(state, &bad, &uniform[0], &uniform[1]);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    pqsamp_word_not(&valid, &bad, state->shares);
    ret = pqsamp_unmask(state, &valid, &valid_lanes);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    if (trace != NULL)
    {
      trace->raw_side_batches++;
    }
    // u0 stays shared
    zero_side_append(pool, &uniform[0], valid_lanes, state->shares);
  }
  if (pool->fill < PQSAMP_LANES)
  {
    return PQSAMP_ERR_BOUND;
  }
  *out = pool->word[0];
  pool->word[0] = pool->word[1];
  pqsamp_word_zero(&pool->word[1]);
  pool->fill -= PQSAMP_LANES;
  return PQSAMP_OK;
}

static int sec_dlx(pqsamp_state *state, pqsamp_word *y, pqsamp_word *quotient,
                   pqsamp_word *boundary, pqsamp_word *valid,
                   const pqsamp_word *side, const pqsamp_params *params)
{
  pqsamp_word uniform[PQSAMP_GEOM_BITS];
  pqsamp_word previous;
  unsigned q_bits = pqsamp_bit_width_u32(params->k_sat);
  unsigned boundary_bits = params->threshold_bits + 1U;
  unsigned i;
  int ret;

  ret = pqsamp_uniform(state, uniform, params->geom_count);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  for (i = 0; i < PQSAMP_VALUE_BITS; i++)
  {
    pqsamp_word_zero(&y[i]);
  }
  for (i = 0; i < PQSAMP_K_BITS; i++)
  {
    pqsamp_word_zero(&quotient[i]);
  }
  for (i = 0; i < PQSAMP_BOUNDARY_BITS; i++)
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
    ret = pqsamp_sec_and(state, &positive, &indicator, side);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    pqsamp_word_xor(&negative, &indicator, &positive, state->shares);

    entry = &params->side[0][i];
    select_public(y, PQSAMP_VALUE_BITS, (uint16_t)entry->y, &negative,
                  state->shares);
    select_public(quotient, q_bits, entry->quotient, &negative, state->shares);
    select_public(boundary, boundary_bits, entry->boundary_count, &negative,
                  state->shares);
    if (entry->valid != 0U)
    {
      pqsamp_word_xor(valid, valid, &negative, state->shares);
    }

    entry = &params->side[1][i];
    select_public(y, PQSAMP_VALUE_BITS, (uint16_t)entry->y, &positive,
                  state->shares);
    select_public(quotient, q_bits, entry->quotient, &positive, state->shares);
    select_public(boundary, boundary_bits, entry->boundary_count, &positive,
                  state->shares);
    if (entry->valid != 0U)
    {
      pqsamp_word_xor(valid, valid, &positive, state->shares);
    }
  }
  return PQSAMP_OK;
}

static int sec_dlx_half(pqsamp_state *state, pqsamp_half_value *value,
                        pqsamp_word *quotient, pqsamp_word *boundary,
                        pqsamp_word *valid, const pqsamp_params *params)
{
  pqsamp_word uniform[PQSAMP_GEOM_BITS];
  pqsamp_word previous;
  pqsamp_word seen_first_one;
  pqsamp_word bad_terminal;
  unsigned q_bits = pqsamp_bit_width_u32(params->k_sat);
  unsigned boundary_bits = params->threshold_bits + 1U;
  unsigned i;
  int ret;

  ret = pqsamp_uniform(state, &value->side, 1U);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  ret = pqsamp_uniform(state, uniform, params->geom_count);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  for (i = 0; i < PQSAMP_HALF_GEOM_BITS; i++)
  {
    pqsamp_word_zero(&value->g[i]);
  }
  for (i = 0; i < PQSAMP_K_BITS; i++)
  {
    pqsamp_word_zero(&quotient[i]);
  }
  for (i = 0; i < PQSAMP_BOUNDARY_BITS; i++)
  {
    pqsamp_word_zero(&boundary[i]);
  }
  pqsamp_word_zero(&seen_first_one);
  pqsamp_word_zero(&bad_terminal);
  pqsamp_word_not(&previous, &uniform[0], state->shares);

  for (i = 0; i < params->geom_count; i++)
  {
    const pqsamp_candidate *entry = &params->side[0][i];
    pqsamp_word indicator;

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
    select_public(value->g, PQSAMP_HALF_GEOM_BITS, i, &indicator,
                  state->shares);
    select_public(quotient, q_bits, entry->quotient, &indicator, state->shares);
    select_public(boundary, boundary_bits, entry->boundary_count, &indicator,
                  state->shares);
    pqsamp_word_xor(&seen_first_one, &seen_first_one, &indicator,
                    state->shares);
    // reject g 13 s 1
    if (i + 1U == params->geom_count)
    {
      ret = pqsamp_sec_and(state, &bad_terminal, &indicator, &value->side);
      if (ret != PQSAMP_OK)
      {
        return ret;
      }
    }
  }
  pqsamp_word_xor(valid, &seen_first_one, &bad_terminal, state->shares);
  return PQSAMP_OK;
}

static int sec_geom_sat(pqsamp_state *state, pqsamp_word *out,
                        unsigned saturation)
{
  pqsamp_word uniform[PQSAMP_K_SAT_MAX];
  pqsamp_word previous;
  unsigned bits = pqsamp_bit_width_u32(saturation);
  unsigned i;
  int ret = pqsamp_uniform(state, uniform, saturation);

  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  for (i = 0; i < PQSAMP_K_BITS; i++)
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
    select_public(out, bits, i, &indicator, state->shares);
    pqsamp_word_xor(&previous, &previous, &indicator, state->shares);
  }
  select_public(out, bits, saturation, &previous, state->shares);
  return PQSAMP_OK;
}

static unsigned popcount32(uint32_t value)
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

int pqsamp_sample_pre(pqsamp_state *state, pqsamp_batch *batch, uint32_t *live,
                      const pqsamp_params *params,
                      pqsamp_zero_side_pool *side_pool, pqsamp_trace *trace)
{
  pqsamp_word side;
  pqsamp_word valid;
  pqsamp_word early;
  uint32_t valid_lanes;
  uint32_t early_lanes;
  unsigned q_bits = pqsamp_bit_width_u32(params->k_sat);
  int ret = pqsamp_zero_side(state, side_pool, &side, trace);

  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  ret = sec_dlx(state, batch->y, batch->quotient, batch->boundary, &valid,
                &side, params);

  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  ret = sec_geom_sat(state, batch->k, params->k_sat);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  ret = pqsamp_sec_lt(state, &early, batch->k, batch->quotient, q_bits);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
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
  // public rejection mask
  *live = valid_lanes & ~early_lanes;
  if (state->stats != NULL)
  {
    state->stats->candidate_batches++;
    state->stats->candidates += PQSAMP_LANES;
  }
  return PQSAMP_OK;
}

int pqsamp_sample_half_pre(pqsamp_state *state, pqsamp_half_batch *batch,
                           uint32_t *live, const pqsamp_params *params)
{
  pqsamp_word valid;
  pqsamp_word early;
  uint32_t valid_lanes;
  uint32_t early_lanes;
  unsigned q_bits = pqsamp_bit_width_u32(params->k_sat);
  int ret = sec_dlx_half(state, &batch->value, batch->quotient, batch->boundary,
                         &valid, params);

  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  ret = sec_geom_sat(state, batch->k, params->k_sat);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  ret = pqsamp_sec_lt(state, &early, batch->k, batch->quotient, q_bits);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
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
  // public rejection mask
  *live = valid_lanes & ~early_lanes;
  if (state->stats != NULL)
  {
    state->stats->candidate_batches++;
    state->stats->candidates += PQSAMP_LANES;
  }
  return PQSAMP_OK;
}

static int sample_finish(pqsamp_state *state, const pqsamp_word *k,
                         const pqsamp_word *quotient,
                         const pqsamp_word *boundary, uint32_t active,
                         uint32_t *accept, const pqsamp_params *params)
{
  pqsamp_word equal;
  pqsamp_word uniform[PQSAMP_BOUNDARY_BITS];
  pqsamp_word boundary_accept;
  pqsamp_word boundary_reject;
  pqsamp_word raw_accept;
  uint32_t accept_lanes;
  unsigned q_bits = pqsamp_bit_width_u32(params->k_sat);
  unsigned threshold_bits = params->threshold_bits;
  int ret = pqsamp_sec_eq(state, &equal, k, quotient, q_bits);

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
  pqsamp_word_not(&boundary_reject, &boundary_accept, state->shares);
  ret = pqsamp_sec_and(state, &raw_accept, &equal, &boundary_reject);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  pqsamp_word_not(&raw_accept, &raw_accept, state->shares);
  ret = pqsamp_unmask(state, &raw_accept, &accept_lanes);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  // public rejection mask
  *accept = accept_lanes & active;
  if (state->stats != NULL)
  {
    state->stats->accepted += popcount32(*accept);
  }
  return PQSAMP_OK;
}

int pqsamp_sample_finish(pqsamp_state *state, const pqsamp_batch *batch,
                         uint32_t active, uint32_t *accept,
                         const pqsamp_params *params)
{
  return sample_finish(state, batch->k, batch->quotient, batch->boundary,
                       active, accept, params);
}

int pqsamp_sample_half_finish(pqsamp_state *state,
                              const pqsamp_half_batch *batch, uint32_t active,
                              uint32_t *accept, const pqsamp_params *params)
{
  return sample_finish(state, batch->k, batch->quotient, batch->boundary,
                       active, accept, params);
}

// three gate reconstruction
int pqsamp_half_reconstruct(pqsamp_state *state,
                            pqsamp_word out[PQSAMP_VALUE_BITS],
                            const pqsamp_half_value *value)
{
  pqsamp_word a[PQSAMP_HALF_VALUE_BITS];
  pqsamp_word carry;
  unsigned bit;

  for (bit = 0; bit < PQSAMP_HALF_GEOM_BITS; bit++)
  {
    pqsamp_word complement;

    pqsamp_word_not(&complement, &value->g[bit], state->shares);
    pqsamp_word_xor(&a[bit], &value->side, &complement, state->shares);
  }
  pqsamp_word_not(&a[4], &value->side, state->shares);
  pqsamp_word_not(&out[0], &a[0], state->shares);
  carry = a[0];
  for (bit = 1; bit < 4U; bit++)
  {
    pqsamp_word next;
    int ret;

    pqsamp_word_xor(&out[bit], &a[bit], &carry, state->shares);
    ret = pqsamp_sec_and(state, &next, &a[bit], &carry);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    carry = next;
  }
  pqsamp_word_xor(&out[4], &a[4], &carry, state->shares);
  for (bit = PQSAMP_HALF_VALUE_BITS; bit < PQSAMP_VALUE_BITS; bit++)
  {
    out[bit] = out[4];
  }
  return PQSAMP_OK;
}
