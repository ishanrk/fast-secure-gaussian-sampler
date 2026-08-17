#include "internal.h"

static uint32_t value_barrier(uint32_t value)
{
#if defined(__GNUC__) || defined(__clang__)
  __asm__ __volatile__("" : "+r"(value));
#else
  volatile uint32_t copy = value;

  value = copy;
#endif
  return value;
}

void pqsamp_word_zero(pqsamp_word *value)
{
  unsigned i;

  for (i = 0; i < PQSAMP_MAX_SHARES; i++)
  {
    value->share[i] = 0;
  }
}

void pqsamp_word_not(pqsamp_word *out, const pqsamp_word *value,
                     unsigned shares)
{
  unsigned i;

  *out = *value;
  out->share[0] ^= UINT32_MAX;
  for (i = shares; i < PQSAMP_MAX_SHARES; i++)
  {
    out->share[i] = 0;
  }
}

void pqsamp_word_xor(pqsamp_word *out, const pqsamp_word *left,
                     const pqsamp_word *right, unsigned shares)
{
  unsigned i;

  for (i = 0; i < shares; i++)
  {
    out->share[i] = left->share[i] ^ right->share[i];
  }
  for (; i < PQSAMP_MAX_SHARES; i++)
  {
    out->share[i] = 0;
  }
}

int pqsamp_uniform(pqsamp_state *state, pqsamp_word *out, unsigned bits)
{
  unsigned bit;

  for (bit = 0; bit < bits; bit++)
  {
    uint32_t value;
    unsigned share;
    int ret = pqsamp_rng_word(state->coins, &value);

    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    pqsamp_word_zero(&out[bit]);
    out[bit].share[0] = value;
    for (share = 1; share < state->shares; share++)
    {
      ret = pqsamp_rng_word(state->masks, &value);
      if (ret != PQSAMP_OK)
      {
        return ret;
      }
      out[bit].share[share] = value;
      out[bit].share[0] ^= value;
    }
  }
  return PQSAMP_OK;
}

// cs20 algorithm 2
PQSAMP_NOINLINE int pqsamp_sec_and(pqsamp_state *state, pqsamp_word *out,
                                   const pqsamp_word *left,
                                   const pqsamp_word *right)
{
  pqsamp_word x = *left;
  pqsamp_word y = *right;
  pqsamp_word result;
  uint32_t random[PQSAMP_MAX_SHARES][PQSAMP_MAX_SHARES] = {{0}};
  unsigned i;

  pqsamp_word_zero(&result);
  // fresh pair masks
  for (i = 0; i < state->shares; i++)
  {
    unsigned j;

    for (j = i + 1U; j < state->shares; j++)
    {
      int ret = pqsamp_rng_word(state->masks, &random[i][j]);

      if (ret != PQSAMP_OK)
      {
        return ret;
      }
      random[i][j] = value_barrier(random[i][j]);
      random[j][i] = random[i][j];
    }
  }
  for (i = 0; i < state->shares; i++)
  {
    unsigned j;
    uint32_t xi = value_barrier(x.share[i]);
    uint32_t zi = xi & value_barrier(y.share[i]);

    zi = value_barrier(zi);
    for (j = 0; j < state->shares; j++)
    {
      uint32_t u;
      uint32_t v;
      uint32_t term;

      if (i == j)
      {
        continue;
      }
      u = (~xi) & value_barrier(random[i][j]);
      v = value_barrier(y.share[j] ^ random[i][j]);
      term = xi & v;
      zi ^= value_barrier(u) ^ value_barrier(term);
      zi = value_barrier(zi);
    }
    result.share[i] = zi;
  }
  if (state->stats != NULL)
  {
    state->stats->sec_and_calls++;
  }
  *out = result;
  return PQSAMP_OK;
}

int pqsamp_sec_eq(pqsamp_state *state, pqsamp_word *out,
                  const pqsamp_word *left, const pqsamp_word *right,
                  unsigned bits)
{
  pqsamp_word result;
  unsigned bit;

  pqsamp_word_xor(&result, &left[0], &right[0], state->shares);
  for (bit = 1; bit < bits; bit++)
  {
    pqsamp_word a;
    pqsamp_word b;
    pqsamp_word product;
    int ret;

    pqsamp_word_not(&a, &result, state->shares);
    pqsamp_word_xor(&b, &left[bit], &right[bit], state->shares);
    pqsamp_word_not(&b, &b, state->shares);
    ret = pqsamp_sec_and(state, &product, &a, &b);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    pqsamp_word_not(&result, &product, state->shares);
  }
  pqsamp_word_not(out, &result, state->shares);
  return PQSAMP_OK;
}

int pqsamp_sec_leq(pqsamp_state *state, pqsamp_word *out,
                   const pqsamp_word *left, const pqsamp_word *right,
                   unsigned bits)
{
  pqsamp_word borrow;
  unsigned bit;

  pqsamp_word_zero(&borrow);
  for (bit = 0; bit < bits; bit++)
  {
    pqsamp_word a;
    pqsamp_word b;
    pqsamp_word product;
    int ret;

    pqsamp_word_xor(&a, &right[bit], &borrow, state->shares);
    pqsamp_word_xor(&b, &left[bit], &borrow, state->shares);
    ret = pqsamp_sec_and(state, &product, &a, &b);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
    pqsamp_word_xor(&borrow, &product, &left[bit], state->shares);
  }
  pqsamp_word_not(out, &borrow, state->shares);
  return PQSAMP_OK;
}

int pqsamp_sec_lt(pqsamp_state *state, pqsamp_word *out,
                  const pqsamp_word *left, const pqsamp_word *right,
                  unsigned bits)
{
  pqsamp_word leq;
  int ret = pqsamp_sec_leq(state, &leq, right, left, bits);

  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  pqsamp_word_not(out, &leq, state->shares);
  return PQSAMP_OK;
}

// refresh before unmask
PQSAMP_NOINLINE int pqsamp_unmask(pqsamp_state *state, const pqsamp_word *value,
                                  uint32_t *out)
{
  uint32_t refreshed[PQSAMP_MAX_SHARES];
  uint32_t result = 0;
  unsigned i;

  for (i = 0; i < state->shares; i++)
  {
    refreshed[i] = value->share[i];
  }
  for (i = 0; i < state->shares; i++)
  {
    unsigned j;

    for (j = i + 1U; j < state->shares; j++)
    {
      uint32_t random;
      int ret = pqsamp_rng_word(state->masks, &random);

      if (ret != PQSAMP_OK)
      {
        return ret;
      }
      refreshed[i] = value_barrier(refreshed[i] ^ random);
      refreshed[j] = value_barrier(refreshed[j] ^ random);
    }
  }
  for (i = 0; i < state->shares; i++)
  {
    result ^= value_barrier(refreshed[i]);
  }
  *out = value_barrier(result);
  return PQSAMP_OK;
}
