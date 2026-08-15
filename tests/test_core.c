#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "common.h"
#include "test.h"

static uint32_t pqsamp_test_word_value(const pqsamp_word *word, unsigned shares)
{
  uint32_t value = 0;
  unsigned share;

  for (share = 0; share < shares; share++)
  {
    value ^= word->share[share];
  }
  return value;
}

static void pqsamp_test_share_word(pqsamp_word *word, uint32_t value,
                                   unsigned shares, uint32_t salt)
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
} pqsamp_byte_source;

static int pqsamp_byte_randombytes(void *context, uint8_t *out, size_t size)
{
  pqsamp_byte_source *source = context;
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

static int pqsamp_test_rng_bits_order(void)
{
  pqsamp_byte_source source = {{0x53, 0xa9, 0x1c, 0xe0, 0x72, 0x44, 0x8d, 0xf1,
                                0x3b, 0x17, 0x90, 0x21, 0xca, 0x68, 0x5d, 0xb4},
                               0};
  pqsamp_rng rng;
  uint32_t value;

  PQSAMP_CHECK(pqsamp_rng_init(&rng, pqsamp_byte_randombytes, &source) ==
               PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_rng_bits(&rng, 5U, &value) == PQSAMP_OK);
  PQSAMP_CHECK(value == 0x13U);
  PQSAMP_CHECK(pqsamp_rng_bits(&rng, 17U, &value) == PQSAMP_OK);
  PQSAMP_CHECK(value == 0xe54aU);
  PQSAMP_CHECK(pqsamp_rng_bits_used(&rng) == 22U);
  return 0;
}

static int pqsamp_test_gadgets(unsigned shares)
{
  pqsamp_test_rng mask_source =
      pqsamp_test_rng_make(UINT64_C(0x123456789abcdef));
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

  PQSAMP_CHECK(pqsamp_rng_init(&masks, pqsamp_test_randombytes, &mask_source) ==
               PQSAMP_OK);
  state.shares = shares;
  state.coins = NULL;
  state.masks = shares == 1U ? NULL : &masks;
  state.stats = NULL;
  pqsamp_test_share_word(&left, UINT32_C(0x96696996), shares, 3U);
  pqsamp_test_share_word(&right, UINT32_C(0xf0cc3c5a), shares, 7U);
  PQSAMP_CHECK(pqsamp_sec_and(&state, &result, &left, &right) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_test_word_value(&result, shares) ==
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
    pqsamp_test_share_word(&x[bit], xp, shares, bit + 11U);
    pqsamp_test_share_word(&y[bit], yp, shares, bit + 29U);
  }
  PQSAMP_CHECK(pqsamp_sec_eq(&state, &result, x, y, 6U) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_test_word_value(&result, shares) == expected_eq);
  PQSAMP_CHECK(pqsamp_sec_leq(&state, &result, x, y, 6U) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_test_word_value(&result, shares) == expected_leq);
  PQSAMP_CHECK(pqsamp_sec_lt(&state, &result, x, y, 6U) == PQSAMP_OK);
  PQSAMP_CHECK(pqsamp_test_word_value(&result, shares) == expected_lt);
  return 0;
}

static int pqsamp_test_bitslice(void)
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

int main(void)
{
  unsigned shares;

  PQSAMP_CHECK(pqsamp_test_rng_bits_order() == 0);
  PQSAMP_CHECK(pqsamp_test_bitslice() == 0);
  for (shares = 1; shares <= PQSAMP_MAX_SHARES; shares++)
  {
    PQSAMP_CHECK(pqsamp_test_gadgets(shares) == 0);
  }
  puts("core: ok");
  return 0;
}
