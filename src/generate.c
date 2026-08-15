#include "common.h"

static void pqsamp_clear_masked(pqsamp_masked_i16 *out, size_t count)
{
  size_t i;

  for (i = 0; i < count; i++)
  {
    unsigned share;

    for (share = 0; share < PQSAMP_MAX_SHARES; share++)
    {
      out[i].share[share] = 0;
    }
  }
}

static void pqsamp_stats_init(pqsamp_stats *stats)
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

static void pqsamp_store_accepted(pqsamp_masked_i16 block[PQSAMP_LANES],
                                  const pqsamp_masked_i16 lane[PQSAMP_LANES],
                                  uint32_t accepted, size_t wanted,
                                  size_t *filled, unsigned shares)
{
  size_t rank = *filled;
  unsigned i;

  for (i = 0; i < PQSAMP_LANES; i++)
  {
    uint32_t keep = (accepted >> i) & 1U;
    size_t slot;

    /* Scan every public destination instead of indexing by the accepted rank.
     */
    for (slot = 0; slot < wanted; slot++)
    {
      uint16_t mask = (uint16_t)(0U - (keep & (uint32_t)(rank == slot)));
      unsigned share;

      for (share = 0; share < shares; share++)
      {
        uint16_t old = block[slot].share[share];
        uint16_t next = lane[i].share[share];

        block[slot].share[share] = (uint16_t)(old ^ ((old ^ next) & mask));
      }
    }
    rank += keep;
  }
  *filled = rank;
}

int pqsamp_generate_masked(pqsamp_masked_i16 *out, size_t count,
                           const pqsamp_params *params, unsigned shares,
                           pqsamp_rng *coins, pqsamp_rng *masks,
                           pqsamp_stats *stats)
{
  pqsamp_state state;
  uint64_t coins_before;
  uint64_t masks_before = 0;
  size_t offset = 0;
  int ret;

  pqsamp_stats_init(stats);
  if ((out == NULL && count != 0U) || coins == NULL || shares == 0U ||
      shares > PQSAMP_MAX_SHARES ||
      (shares > 1U && (masks == NULL || masks == coins ||
                       (masks->randombytes == coins->randombytes &&
                        masks->context == coins->context))))
  {
    if (out != NULL)
    {
      pqsamp_clear_masked(out, count);
    }
    return PQSAMP_ERR_PARAM;
  }
  ret = pqsamp_params_check(params);
  if (ret != PQSAMP_OK)
  {
    if (out != NULL)
    {
      pqsamp_clear_masked(out, count);
    }
    return ret;
  }
  state.shares = shares;
  state.coins = coins;
  state.masks = masks;
  state.stats = stats;
  coins_before = coins->bits_used;
  if (masks != NULL)
  {
    masks_before = masks->bits_used;
  }

  while (offset < count)
  {
    pqsamp_masked_i16 block[PQSAMP_LANES];
    size_t wanted = count - offset;
    size_t filled = 0;
    unsigned batch;

    if (wanted > PQSAMP_LANES)
    {
      wanted = PQSAMP_LANES;
    }
    pqsamp_clear_masked(block, PQSAMP_LANES);
    for (batch = 0; batch < PQSAMP_FIXED_BATCHES; batch++)
    {
      pqsamp_word candidate[PQSAMP_VALUE_BITS];
      pqsamp_masked_i16 lane[PQSAMP_LANES];
      uint32_t accepted;

      ret = pqsamp_sample_batch(&state, candidate, &accepted, params);
      if (ret != PQSAMP_OK)
      {
        goto error;
      }
      pqsamp_unpack16(lane, candidate, shares);
      pqsamp_store_accepted(block, lane, accepted, wanted, &filled, shares);
    }
    if (filled < wanted)
    {
      ret = PQSAMP_ERR_BOUND;
      goto error;
    }
    {
      size_t i;

      for (i = 0; i < wanted; i++)
      {
        out[offset + i] = block[i];
      }
    }
    offset += wanted;
  }
  if (stats != NULL)
  {
    stats->coin_bits = coins->bits_used - coins_before;
    stats->mask_bits = masks == NULL ? 0U : masks->bits_used - masks_before;
  }
  return PQSAMP_OK;

error:
  pqsamp_clear_masked(out, count);
  if (stats != NULL)
  {
    stats->coin_bits = coins->bits_used - coins_before;
    stats->mask_bits = masks == NULL ? 0U : masks->bits_used - masks_before;
  }
  return ret;
}

int16_t pqsamp_reconstruct(const pqsamp_masked_i16 *value, unsigned shares)
{
  uint16_t bits = 0;
  int32_t out;
  unsigned share;

  if (value == NULL || shares == 0U || shares > PQSAMP_MAX_SHARES)
  {
    return 0;
  }
  for (share = 0; share < shares; share++)
  {
    bits ^= value->share[share];
  }
  out = (int32_t)bits;
  if (bits > (uint16_t)INT16_MAX)
  {
    out -= INT32_C(65536);
  }
  return (int16_t)out;
}
