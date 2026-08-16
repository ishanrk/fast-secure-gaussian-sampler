#include "pqsamp_hawk.h"

#include "internal.h"

// packs shared center bits into one plane
static void pack_center(pqsamp_word *out,
                        const pqsamp_masked_bit center[PQSAMP_LANES],
                        unsigned shares)
{
  unsigned share;

  pqsamp_word_zero(out);
  for (share = 0; share < shares; share++)
  {
    unsigned lane;

    for (lane = 0; lane < PQSAMP_LANES; lane++)
    {
      out->share[share] |= (uint32_t)(center[lane].share[share] & 1U) << lane;
    }
  }
}

// selects one sample then maps it to the scheme value
static int select_center(pqsamp_state *state, pqsamp_word *out,
                         const pqsamp_word *zero, const pqsamp_word *half,
                         const pqsamp_word *center)
{
  pqsamp_word selected[PQSAMP_VALUE_BITS];
  pqsamp_word borrow = *center;
  unsigned bit;

  for (bit = 0; bit < PQSAMP_VALUE_BITS; bit++)
  {
    pqsamp_word difference;
    pqsamp_word product;
    int ret;

    pqsamp_word_xor(&difference, &zero[bit], &half[bit], state->shares);
    ret = pqsamp_sec_and(state, &product, &difference, center);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    pqsamp_word_xor(&selected[bit], &zero[bit], &product, state->shares);
  }

  out[0] = *center;
  for (bit = 1; bit < PQSAMP_VALUE_BITS; bit++)
  {
    pqsamp_word not_value;
    pqsamp_word next;
    int ret;

    pqsamp_word_xor(&out[bit], &selected[bit - 1U], &borrow, state->shares);
    pqsamp_word_not(&not_value, &selected[bit - 1U], state->shares);
    ret = pqsamp_sec_and(state, &next, &not_value, &borrow);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    borrow = next;
  }
  return PQSAMP_OK;
}

// samples both centers then selects with shared center bits
int pqsamp_hawk_sample_masked(pqsamp_masked_i16 *out, size_t n,
                              pqsamp_profile profile,
                              const pqsamp_masked_bit *center, unsigned shares,
                              pqsamp_rng *coins, pqsamp_rng *masks)
{
  pqsamp_state state;
  size_t offset = 0;

  if ((out == NULL && n != 0U) || (center == NULL && n != 0U) ||
      coins == NULL || shares < 2U || shares > PQSAMP_MAX_SHARES ||
      streams_are_distinct(coins, masks) == 0)
  {
    if (out != NULL)
    {
      masked_clear(out, n);
    }
    return PQSAMP_ERR_PARAM;
  }
  state.shares = shares;
  state.coins = coins;
  state.masks = masks;
  state.stats = NULL;

  while (offset < n)
  {
    pqsamp_masked_i16 zero[PQSAMP_LANES];
    pqsamp_masked_i16 half[PQSAMP_LANES];
    pqsamp_masked_i16 selected[PQSAMP_LANES];
    pqsamp_masked_bit center_block[PQSAMP_LANES] = {{{0}}};
    pqsamp_word zero_planes[PQSAMP_VALUE_BITS];
    pqsamp_word half_planes[PQSAMP_VALUE_BITS];
    pqsamp_word out_planes[PQSAMP_VALUE_BITS];
    pqsamp_word center_plane;
    size_t block = n - offset;
    size_t i;
    int ret;

    if (block > PQSAMP_LANES)
    {
      block = PQSAMP_LANES;
    }
    for (i = 0; i < block; i++)
    {
      center_block[i] = center[offset + i];
    }
    ret = pqsamp_sample_masked(zero, block, profile, PQSAMP_CENTER_ZERO, shares,
                               coins, masks, NULL);
    if (ret != PQSAMP_OK)
    {
      masked_clear(out, n);
      return ret;
    }
    ret = pqsamp_sample_masked(half, block, profile, PQSAMP_CENTER_HALF, shares,
                               coins, masks, NULL);
    if (ret != PQSAMP_OK)
    {
      masked_clear(out, n);
      return ret;
    }
    for (i = block; i < PQSAMP_LANES; i++)
    {
      unsigned share;

      for (share = 0; share < PQSAMP_MAX_SHARES; share++)
      {
        zero[i].share[share] = 0;
        half[i].share[share] = 0;
      }
    }
    pqsamp_pack16(zero_planes, zero, shares);
    pqsamp_pack16(half_planes, half, shares);
    pack_center(&center_plane, center_block, shares);
    ret = select_center(&state, out_planes, zero_planes, half_planes,
                        &center_plane);
    if (ret != PQSAMP_OK)
    {
      masked_clear(out, n);
      return ret;
    }
    pqsamp_unpack16(selected, out_planes, shares);
    for (i = 0; i < block; i++)
    {
      out[offset + i] = selected[i];
    }
    offset += block;
  }
  return PQSAMP_OK;
}
