#ifndef PQSAMP_TEST_H
#define PQSAMP_TEST_H

#include <stdint.h>
#include <stdio.h>

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
} pqsamp_test_rng;

static uint64_t pqsamp_test_next(pqsamp_test_rng *rng)
{
  uint64_t value = rng->state;

  value ^= value >> 12;
  value ^= value << 25;
  value ^= value >> 27;
  rng->state = value;
  return value * UINT64_C(2685821657736338717);
}

static int pqsamp_test_randombytes(void *context, uint8_t *out, size_t size)
{
  pqsamp_test_rng *rng = context;
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
      rng->state = pqsamp_test_next(rng);
    }
    out[i] = (uint8_t)(rng->state >> (8U * (i & 7U)));
  }
  return 0;
}

static pqsamp_test_rng pqsamp_test_rng_make(uint64_t seed)
{
  pqsamp_test_rng rng;

  rng.state = seed == 0U ? UINT64_C(1) : seed;
  rng.calls = 0;
  rng.fail_after = (size_t)-1;
  return rng;
}

#endif /* PQSAMP_TEST_H */
