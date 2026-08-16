#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "internal.h"
#include "test.h"

// xors active shares into one lane word
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

// splits one lane word into deterministic shares
static void share_word(pqsamp_word *word, uint32_t value, unsigned shares,
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

typedef struct
{
  uint8_t bytes[16];
  size_t offset;
} byte_source;

// fills bytes with an increasing test pattern
static int byte_randombytes(void *context, uint8_t *out, size_t size)
{
  byte_source *source = context;
  size_t i;

  if (source->offset + size > sizeof(source->bytes))
  {
    return -1;
  }
  for (i = 0; i < size; i++)
  {
    out[i] = source->bytes[source->offset + i];
  }
  source->offset += size;
  return 0;
}

// checks bit order refill and sticky rng failure
static int rng_bits_order(void)
{
  byte_source source = {{0x53, 0xa9, 0x1c, 0xe0, 0x72, 0x44, 0x8d, 0xf1, 0x3b,
                         0x17, 0x90, 0x21, 0xca, 0x68, 0x5d, 0xb4},
                        0};
  pqsamp_rng rng;
  uint32_t value;

  PQSAMP_CHECK(pqsamp_rng_init(&rng, byte_randombytes, &source) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_rng_bits(&rng, 5U, &value) == PQSAMP_OK);
  PQSAMP_CHECK(value == 0x13U);
  PQSAMP_CHECK(pqsamp_rng_bits(&rng, 17U, &value) == PQSAMP_OK);
  PQSAMP_CHECK(value == 0xe54aU);
  PQSAMP_CHECK(rng.bits_used == 22U);
  return 0;
}

// checks every shared boolean gadget
static int gadgets(unsigned shares)
{
  test_rng mask_source = test_rng_make(UINT64_C(0x123456789abcdef));
  pqsamp_rng masks;
  pqsamp_state state;
  pqsamp_word left;
  pqsamp_word right;
  pqsamp_word result;
  pqsamp_word x[6];
  pqsamp_word y[6];
  uint32_t expected_eq = 0;
  uint32_t expected_leq = 0;
  uint32_t expected_lt = 0;
  uint32_t unmasked;
  unsigned bit;
  unsigned lane;

  PQSAMP_CHECK(pqsamp_rng_init(&masks, test_randombytes, &mask_source) ==
               PQSAMP_OK);
  state.shares = shares;
  state.coins = NULL;
  state.masks = shares == 1U ? NULL : &masks;
  state.stats = NULL;
  share_word(&left, UINT32_C(0x96696996), shares, 3U);
  share_word(&right, UINT32_C(0xf0cc3c5a), shares, 7U);
  PQSAMP_CHECK(pqsamp_sec_and(&state, &result, &left, &right) == PQSAMP_OK);
  PQSAMP_CHECK(word_value(&result, shares) ==
               (UINT32_C(0x96696996) & UINT32_C(0xf0cc3c5a)));
  PQSAMP_CHECK(pqsamp_unmask(&state, &left, &unmasked) == PQSAMP_OK);
  PQSAMP_CHECK(unmasked == UINT32_C(0x96696996));

  for (bit = 0; bit < 6U; bit++)
  {
    uint32_t xp = 0;
    uint32_t yp = 0;

    for (lane = 0; lane < PQSAMP_LANES; lane++)
    {
      unsigned xv = (lane * 13U + 5U) & 63U;
      unsigned yv = (lane * 7U + 19U) & 63U;

      xp |= ((xv >> bit) & 1U) << lane;
      yp |= ((yv >> bit) & 1U) << lane;
      if (bit == 0U)
      {
        expected_eq |= (uint32_t)(xv == yv) << lane;
        expected_leq |= (uint32_t)(xv <= yv) << lane;
        expected_lt |= (uint32_t)(xv < yv) << lane;
      }
    }
    share_word(&x[bit], xp, shares, bit + 11U);
    share_word(&y[bit], yp, shares, bit + 29U);
  }
  PQSAMP_CHECK(pqsamp_sec_eq(&state, &result, x, y, 6U) == PQSAMP_OK);
  PQSAMP_CHECK(word_value(&result, shares) == expected_eq);
  PQSAMP_CHECK(pqsamp_sec_leq(&state, &result, x, y, 6U) == PQSAMP_OK);
  PQSAMP_CHECK(word_value(&result, shares) == expected_leq);
  PQSAMP_CHECK(pqsamp_sec_lt(&state, &result, x, y, 6U) == PQSAMP_OK);
  PQSAMP_CHECK(word_value(&result, shares) == expected_lt);
  return 0;
}

// checks pack and unpack on every bit lane pair
static int bitslice(void)
{
  pqsamp_masked_i16 input[PQSAMP_LANES];
  pqsamp_masked_i16 output[PQSAMP_LANES];
  pqsamp_word planes[PQSAMP_VALUE_BITS];
  unsigned lane;

  for (lane = 0; lane < PQSAMP_LANES; lane++)
  {
    input[lane].share[0] = (uint16_t)(lane * 2053U);
    input[lane].share[1] = (uint16_t)(lane * 3911U + 17U);
    input[lane].share[2] = 0;
    input[lane].share[3] = 0;
  }
  pqsamp_pack16(planes, input, 2U);
  pqsamp_unpack16(output, planes, 2U);
  for (lane = 0; lane < PQSAMP_LANES; lane++)
  {
    PQSAMP_CHECK(output[lane].share[0] == input[lane].share[0]);
    PQSAMP_CHECK(output[lane].share[1] == input[lane].share[1]);
  }
  return 0;
}

// clears one zero center test batch
static void batch_zero(pqsamp_batch *batch)
{
  unsigned i;

  for (i = 0; i < PQSAMP_VALUE_BITS; i++)
  {
    pqsamp_word_zero(&batch->y[i]);
  }
  for (i = 0; i < PQSAMP_K_BITS; i++)
  {
    pqsamp_word_zero(&batch->quotient[i]);
    pqsamp_word_zero(&batch->k[i]);
  }
  for (i = 0; i < PQSAMP_BOUNDARY_BITS; i++)
  {
    pqsamp_word_zero(&batch->boundary[i]);
  }
}

// clears one half center test batch
static void half_batch_zero(pqsamp_half_batch *batch)
{
  unsigned i;

  for (i = 0; i < PQSAMP_HALF_GEOM_BITS; i++)
  {
    pqsamp_word_zero(&batch->value.g[i]);
  }
  pqsamp_word_zero(&batch->value.side);
  for (i = 0; i < PQSAMP_K_BITS; i++)
  {
    pqsamp_word_zero(&batch->quotient[i]);
    pqsamp_word_zero(&batch->k[i]);
  }
  for (i = 0; i < PQSAMP_BOUNDARY_BITS; i++)
  {
    pqsamp_word_zero(&batch->boundary[i]);
  }
}

// writes one integer into one shared lane
static void set_lane(pqsamp_word *word, unsigned words, unsigned lane,
                     unsigned share, uint64_t value)
{
  unsigned bit;

  for (bit = 0; bit < words; bit++)
  {
    word[bit].share[share] |= (uint32_t)((value >> bit) & 1U) << lane;
  }
}

// reads one integer from one shared lane
static uint64_t get_lane(const pqsamp_word *word, unsigned words, unsigned lane,
                         unsigned share)
{
  uint64_t value = 0;
  unsigned bit;

  for (bit = 0; bit < words; bit++)
  {
    value |= (uint64_t)((word[bit].share[share] >> lane) & 1U) << bit;
  }
  return value;
}

// counts set lanes without compiler helpers
static unsigned popcount32(uint32_t x)
{
  unsigned count = 0;

  while (x != 0U)
  {
    x &= x - 1U;
    count++;
  }
  return count;
}

// removes and returns the first set lane
static unsigned next_lane(uint32_t *mask)
{
  unsigned lane = 0;
  uint32_t x = *mask;

  while ((x & 1U) == 0U)
  {
    x >>= 1;
    lane++;
  }
  *mask &= *mask - 1U;
  return lane;
}

// checks zero center compaction for one lane mask
static int compact_case(uint32_t mask, unsigned offset, unsigned shares)
{
  pqsamp_batch in;
  pqsamp_batch out;
  uint32_t expected_mask = mask;
  uint32_t remaining;
  unsigned capacity = PQSAMP_LANES - offset;
  unsigned taken = popcount32(mask);
  unsigned filled = offset;
  unsigned lane;
  unsigned share;

  if (taken > capacity)
  {
    taken = capacity;
  }
  batch_zero(&in);
  batch_zero(&out);
  for (lane = 0; lane < PQSAMP_LANES; lane++)
  {
    for (share = 0; share < shares; share++)
    {
      set_lane(in.y, PQSAMP_VALUE_BITS, lane, share,
               (uint64_t)(share * 257U + lane * 3U + 1U));
      set_lane(in.quotient, PQSAMP_K_BITS, lane, share,
               (uint64_t)(share * 32U + lane));
      set_lane(in.boundary, PQSAMP_BOUNDARY_BITS, lane, share,
               (uint64_t)(share + 1U) * UINT64_C(100000) + lane * 997U);
      set_lane(in.k, PQSAMP_K_BITS, lane, share,
               (uint64_t)((share * 17U + lane * 2U) & 127U));
    }
  }
  remaining = pqsamp_compact_batch(&out, &filled, &in, mask, shares);
  PQSAMP_CHECK(filled == offset + taken);
  for (lane = 0; lane < taken; lane++)
  {
    unsigned src = next_lane(&expected_mask);
    unsigned dst = offset + lane;

    for (share = 0; share < shares; share++)
    {
      PQSAMP_CHECK(get_lane(out.y, PQSAMP_VALUE_BITS, dst, share) ==
                   get_lane(in.y, PQSAMP_VALUE_BITS, src, share));
      PQSAMP_CHECK(get_lane(out.quotient, PQSAMP_K_BITS, dst, share) ==
                   get_lane(in.quotient, PQSAMP_K_BITS, src, share));
      PQSAMP_CHECK(get_lane(out.boundary, PQSAMP_BOUNDARY_BITS, dst, share) ==
                   get_lane(in.boundary, PQSAMP_BOUNDARY_BITS, src, share));
      PQSAMP_CHECK(get_lane(out.k, PQSAMP_K_BITS, dst, share) ==
                   get_lane(in.k, PQSAMP_K_BITS, src, share));
    }
  }
  PQSAMP_CHECK(remaining == expected_mask);
  for (lane = 0; lane < PQSAMP_LANES; lane++)
  {
    if (lane < offset || lane >= filled)
    {
      for (share = 0; share < PQSAMP_MAX_SHARES; share++)
      {
        PQSAMP_CHECK(get_lane(out.y, PQSAMP_VALUE_BITS, lane, share) == 0U);
        PQSAMP_CHECK(get_lane(out.quotient, PQSAMP_K_BITS, lane, share) == 0U);
        PQSAMP_CHECK(
            get_lane(out.boundary, PQSAMP_BOUNDARY_BITS, lane, share) == 0U);
        PQSAMP_CHECK(get_lane(out.k, PQSAMP_K_BITS, lane, share) == 0U);
      }
    }
    for (share = shares; share < PQSAMP_MAX_SHARES; share++)
    {
      PQSAMP_CHECK(get_lane(out.y, PQSAMP_VALUE_BITS, lane, share) == 0U);
      PQSAMP_CHECK(get_lane(out.quotient, PQSAMP_K_BITS, lane, share) == 0U);
      PQSAMP_CHECK(get_lane(out.boundary, PQSAMP_BOUNDARY_BITS, lane, share) ==
                   0U);
      PQSAMP_CHECK(get_lane(out.k, PQSAMP_K_BITS, lane, share) == 0U);
    }
  }
  return 0;
}

// checks half center compaction for one lane mask
static int half_compact_case(uint32_t mask, unsigned offset, unsigned shares)
{
  pqsamp_half_batch in;
  pqsamp_half_batch out;
  uint32_t expected_mask = mask;
  uint32_t remaining;
  unsigned capacity = PQSAMP_LANES - offset;
  unsigned taken = popcount32(mask);
  unsigned filled = offset;
  unsigned lane;
  unsigned share;

  if (taken > capacity)
  {
    taken = capacity;
  }
  half_batch_zero(&in);
  half_batch_zero(&out);
  for (lane = 0; lane < PQSAMP_LANES; lane++)
  {
    for (share = 0; share < shares; share++)
    {
      set_lane(in.value.g, PQSAMP_HALF_GEOM_BITS, lane, share,
               (uint64_t)((share * 7U + lane) & 15U));
      set_lane(&in.value.side, 1U, lane, share,
               (uint64_t)((share + lane) & 1U));
      set_lane(in.quotient, PQSAMP_K_BITS, lane, share,
               (uint64_t)(share * 32U + lane));
      set_lane(in.boundary, PQSAMP_BOUNDARY_BITS, lane, share,
               (uint64_t)(share + 1U) * UINT64_C(100000) + lane * 997U);
      set_lane(in.k, PQSAMP_K_BITS, lane, share,
               (uint64_t)((share * 17U + lane * 2U) & 127U));
    }
  }
  remaining = pqsamp_compact_half_batch(&out, &filled, &in, mask, shares);
  PQSAMP_CHECK(filled == offset + taken);
  for (lane = 0; lane < taken; lane++)
  {
    unsigned src = next_lane(&expected_mask);
    unsigned dst = offset + lane;

    for (share = 0; share < shares; share++)
    {
      PQSAMP_CHECK(get_lane(out.value.g, PQSAMP_HALF_GEOM_BITS, dst, share) ==
                   get_lane(in.value.g, PQSAMP_HALF_GEOM_BITS, src, share));
      PQSAMP_CHECK(get_lane(&out.value.side, 1U, dst, share) ==
                   get_lane(&in.value.side, 1U, src, share));
      PQSAMP_CHECK(get_lane(out.quotient, PQSAMP_K_BITS, dst, share) ==
                   get_lane(in.quotient, PQSAMP_K_BITS, src, share));
      PQSAMP_CHECK(get_lane(out.boundary, PQSAMP_BOUNDARY_BITS, dst, share) ==
                   get_lane(in.boundary, PQSAMP_BOUNDARY_BITS, src, share));
      PQSAMP_CHECK(get_lane(out.k, PQSAMP_K_BITS, dst, share) ==
                   get_lane(in.k, PQSAMP_K_BITS, src, share));
    }
  }
  PQSAMP_CHECK(remaining == expected_mask);
  for (lane = 0; lane < PQSAMP_LANES; lane++)
  {
    if (lane < offset || lane >= filled)
    {
      for (share = 0; share < PQSAMP_MAX_SHARES; share++)
      {
        PQSAMP_CHECK(
            get_lane(out.value.g, PQSAMP_HALF_GEOM_BITS, lane, share) == 0U);
        PQSAMP_CHECK(get_lane(&out.value.side, 1U, lane, share) == 0U);
        PQSAMP_CHECK(get_lane(out.quotient, PQSAMP_K_BITS, lane, share) == 0U);
        PQSAMP_CHECK(
            get_lane(out.boundary, PQSAMP_BOUNDARY_BITS, lane, share) == 0U);
        PQSAMP_CHECK(get_lane(out.k, PQSAMP_K_BITS, lane, share) == 0U);
      }
    }
    for (share = shares; share < PQSAMP_MAX_SHARES; share++)
    {
      PQSAMP_CHECK(get_lane(out.value.g, PQSAMP_HALF_GEOM_BITS, lane, share) ==
                   0U);
      PQSAMP_CHECK(get_lane(&out.value.side, 1U, lane, share) == 0U);
      PQSAMP_CHECK(get_lane(out.quotient, PQSAMP_K_BITS, lane, share) == 0U);
      PQSAMP_CHECK(get_lane(out.boundary, PQSAMP_BOUNDARY_BITS, lane, share) ==
                   0U);
      PQSAMP_CHECK(get_lane(out.k, PQSAMP_K_BITS, lane, share) == 0U);
    }
  }
  return 0;
}

// checks compaction patterns for every share count
static int compaction(void)
{
  static const uint32_t patterns[] = {0U,
                                      UINT32_MAX,
                                      UINT32_C(0x0000ffff),
                                      UINT32_C(0xffff0000),
                                      UINT32_C(0x55555555),
                                      UINT32_C(0xaaaaaaaa),
                                      UINT32_C(0x83a51f09),
                                      UINT32_C(0x6c42d7e1),
                                      UINT32_C(0x107fe288),
                                      UINT32_C(0xd91140b5)};
  static const unsigned offsets[] = {0U, 1U, 15U, 31U};
  unsigned shares;

  for (shares = 1U; shares <= PQSAMP_MAX_SHARES; shares++)
  {
    unsigned offset;

    for (offset = 0; offset < sizeof(offsets) / sizeof(offsets[0]); offset++)
    {
      unsigned i;

      for (i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++)
      {
        PQSAMP_CHECK(compact_case(patterns[i], offsets[offset], shares) == 0);
        PQSAMP_CHECK(half_compact_case(patterns[i], offsets[offset], shares) ==
                     0);
      }
      for (i = 0; i < PQSAMP_LANES; i++)
      {
        PQSAMP_CHECK(compact_case(UINT32_C(1) << i, offsets[offset], shares) ==
                     0);
        PQSAMP_CHECK(
            half_compact_case(UINT32_C(1) << i, offsets[offset], shares) == 0);
      }
      for (i = 0; i + 1U < PQSAMP_LANES; i++)
      {
        PQSAMP_CHECK(compact_case(UINT32_C(3) << i, offsets[offset], shares) ==
                     0);
        PQSAMP_CHECK(
            half_compact_case(UINT32_C(3) << i, offsets[offset], shares) == 0);
      }
    }
  }
  return 0;
}

// runs rng gadget bitslice and compaction checks
int main(void)
{
  unsigned shares;

  PQSAMP_CHECK(rng_bits_order() == 0);
  PQSAMP_CHECK(bitslice() == 0);
  PQSAMP_CHECK(compaction() == 0);
  for (shares = 1; shares <= PQSAMP_MAX_SHARES; shares++)
  {
    PQSAMP_CHECK(gadgets(shares) == 0);
  }
  puts("core: ok");
  return 0;
}
