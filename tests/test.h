#ifndef PQSAMP_TEST_H
#define PQSAMP_TEST_H

#include <stdint.h>
#include <stdio.h>

#include "pqsamp.h"

#define PQSAMP_CHECK(condition)                                        \
  do                                                                   \
  {                                                                    \
    if (!(condition))                                                  \
    {                                                                  \
      fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
              #condition);                                             \
      return 1;                                                        \
    }                                                                  \
  } while (0)

typedef struct
{
  uint64_t state;
  size_t calls;
  size_t fail_after;
} test_rng;

// advances one deterministic test stream
static uint64_t test_next(test_rng *rng)
{
  uint64_t value = rng->state;

  value ^= value >> 12;
  value ^= value << 25;
  value ^= value >> 27;
  rng->state = value;
  return value * UINT64_C(2685821657736338717);
}

// fills bytes and can fail at one chosen call
static int test_randombytes(void *context, uint8_t *out, size_t size)
{
  test_rng *rng = context;
  size_t i;

  if (rng->calls >= rng->fail_after)
  {
    return -1;
  }
  rng->calls++;
  for (i = 0; i < size; i++)
  {
    if ((i & 7U) == 0U)
    {
      rng->state = test_next(rng);
    }
    out[i] = (uint8_t)(rng->state >> (8U * (i & 7U)));
  }
  return 0;
}

// creates one deterministic test stream
static test_rng test_rng_make(uint64_t seed)
{
  test_rng rng;

  rng.state = seed == 0U ? UINT64_C(1) : seed;
  rng.calls = 0;
  rng.fail_after = (size_t)-1;
  return rng;
}

// xors active shares into one signed sample
static inline int16_t test_reconstruct(const pqsamp_masked_i16 *value,
                                       unsigned shares)
{
  uint16_t bits = 0;
  int32_t out;
  unsigned share;

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

#endif /* pqsamp_test_h */
