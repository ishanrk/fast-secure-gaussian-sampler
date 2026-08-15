#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "pqsamp.h"

typedef struct
{
  uint64_t value;
} pqsamp_bench_rng;

static const uint64_t pqsamp_bench_coin_seed = UINT64_C(0x243f6a8885a308d3);
static const uint64_t pqsamp_bench_mask_seed = UINT64_C(0x13198a2e03707344);

static uint64_t pqsamp_bench_next(pqsamp_bench_rng *rng)
{
  uint64_t value = rng->value;

  value ^= value >> 12;
  value ^= value << 25;
  value ^= value >> 27;
  rng->value = value;
  return value * UINT64_C(2685821657736338717);
}

static int pqsamp_bench_randombytes(void *context, uint8_t *out, size_t size)
{
  pqsamp_bench_rng *rng = context;
  size_t i;

  for (i = 0; i < size; i++)
  {
    if ((i & 7U) == 0U)
    {
      rng->value = pqsamp_bench_next(rng);
    }
    out[i] = (uint8_t)(rng->value >> (8U * (i & 7U)));
  }
  return 0;
}

static int pqsamp_bench_init(pqsamp_rng *coins, pqsamp_rng *masks,
                             pqsamp_bench_rng *coin_source,
                             pqsamp_bench_rng *mask_source)
{
  int ret;

  coin_source->value = pqsamp_bench_coin_seed;
  mask_source->value = pqsamp_bench_mask_seed;
  ret = pqsamp_rng_init(coins, pqsamp_bench_randombytes, coin_source);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  return pqsamp_rng_init(masks, pqsamp_bench_randombytes, mask_source);
}

static void pqsamp_bench_report(const char *backend, unsigned shares,
                                size_t samples, clock_t elapsed,
                                const pqsamp_stats *stats, int batched)
{
  double count = (double)samples;

  printf("{\"backend\":\"%s\",\"shares\":%u,", backend, shares);
  printf("\"samples\":%zu,\"seconds\":%.6f,", samples,
         (double)elapsed / CLOCKS_PER_SEC);
  printf("\"coin_bits_per_sample\":%.3f,", (double)stats->coin_bits / count);
  printf("\"mask_bits_per_sample\":%.3f,", (double)stats->mask_bits / count);
  if (batched != 0)
  {
    printf("\"batches_per_sample\":%.3f,",
           (double)stats->candidate_batches / count);
  }
  else
  {
    printf("\"candidates_per_sample\":%.3f,",
           (double)stats->candidates / count);
  }
  printf("\"sec_and_per_sample\":%.3f}\n",
         (double)stats->sec_and_calls / count);
}

int main(void)
{
  enum
  {
    SAMPLE_COUNT = 32768
  };
  const pqsamp_params *params =
      pqsamp_params_get(PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_ZERO);
  pqsamp_bench_rng coin_source;
  pqsamp_bench_rng mask_source;
  pqsamp_rng coins;
  pqsamp_rng masks;
  pqsamp_stats stats;
  int16_t *plain = malloc(sizeof(int16_t) * SAMPLE_COUNT);
  pqsamp_masked_i16 *masked = malloc(sizeof(pqsamp_masked_i16) * SAMPLE_COUNT);
  clock_t start;
  clock_t stop;
  unsigned shares;
  int ret;

  if (plain == NULL || masked == NULL || params == NULL)
  {
    free(masked);
    free(plain);
    return EXIT_FAILURE;
  }

  ret = pqsamp_bench_init(&coins, &masks, &coin_source, &mask_source);
  start = clock();
  if (ret == PQSAMP_OK && start != (clock_t)-1)
  {
    ret = pqsamp_generate(plain, SAMPLE_COUNT, params, &coins, &stats);
  }
  stop = clock();
  if (ret != PQSAMP_OK || start == (clock_t)-1 || stop == (clock_t)-1)
  {
    free(masked);
    free(plain);
    return EXIT_FAILURE;
  }
  pqsamp_bench_report("scalar", 1U, SAMPLE_COUNT, stop - start, &stats, 0);

  for (shares = 1U; shares <= PQSAMP_MAX_SHARES; shares++)
  {
    ret = pqsamp_bench_init(&coins, &masks, &coin_source, &mask_source);
    start = clock();
    if (ret == PQSAMP_OK && start != (clock_t)-1)
    {
      ret = pqsamp_generate_masked(masked, SAMPLE_COUNT, params, shares, &coins,
                                   shares == 1U ? NULL : &masks, &stats);
    }
    stop = clock();
    if (ret != PQSAMP_OK || start == (clock_t)-1 || stop == (clock_t)-1)
    {
      free(masked);
      free(plain);
      return EXIT_FAILURE;
    }
    pqsamp_bench_report("portable-u32", shares, SAMPLE_COUNT, stop - start,
                        &stats, 1);
  }

  free(masked);
  free(plain);
  return EXIT_SUCCESS;
}
