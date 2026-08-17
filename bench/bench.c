#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "internal.h"

typedef struct
{
  uint64_t value;
} bench_rng;

static const uint64_t coin_seed = UINT64_C(0x243f6a8885a308d3);
static const uint64_t mask_seed = UINT64_C(0x13198a2e03707344);

static uint64_t next(bench_rng *rng)
{
  uint64_t value = rng->value;

  value ^= value >> 12;
  value ^= value << 25;
  value ^= value >> 27;
  rng->value = value;
  return value * UINT64_C(2685821657736338717);
}

static int randombytes(void *context, uint8_t *out, size_t size)
{
  bench_rng *rng = context;
  size_t i;

  for (i = 0; i < size; i++)
  {
    if ((i & 7U) == 0U)
    {
      rng->value = next(rng);
    }
    out[i] = (uint8_t)(rng->value >> (8U * (i & 7U)));
  }
  return 0;
}

static int init_rngs(pqsamp_rng *coins, pqsamp_rng *masks,
                     bench_rng *coin_source, bench_rng *mask_source)
{
  int ret;

  coin_source->value = coin_seed;
  mask_source->value = mask_seed;
  ret = pqsamp_rng_init(coins, randombytes, coin_source);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  return pqsamp_rng_init(masks, randombytes, mask_source);
}

static void report(const char *backend, const char *center, unsigned shares,
                   size_t samples, clock_t elapsed, const pqsamp_stats *stats,
                   const pqsamp_trace *trace)
{
  double count = (double)samples;

  printf("{\"backend\":\"%s\",\"center\":\"%s\",", backend, center);
  printf("\"shares\":%u,", shares);
  printf("\"samples\":%zu,\"seconds\":%.6f,", samples,
         (double)elapsed / CLOCKS_PER_SEC);
  printf("\"coin_bits_per_sample\":%.3f,", (double)stats->coin_bits / count);
  printf("\"mask_bits_per_sample\":%.3f,", (double)stats->mask_bits / count);
  printf("\"batches_per_sample\":%.6f,",
         (double)stats->candidate_batches / count);
  printf("\"raw_side_batches\":%llu,",
         (unsigned long long)trace->raw_side_batches);
  printf("\"stage_two_batches\":%llu,",
         (unsigned long long)trace->finished_batches);
  printf("\"reconstruction_batches\":%llu,",
         (unsigned long long)trace->reconstruction_batches);
  printf("\"candidates_per_sample\":%.3f,", (double)stats->candidates / count);
  printf("\"sec_and_per_sample\":%.9f}\n",
         (double)stats->sec_and_calls / count);
}

int main(void)
{
  enum
  {
    SAMPLE_COUNT = 32768
  };
  bench_rng coin_source;
  bench_rng mask_source;
  pqsamp_rng coins;
  pqsamp_rng masks;
  pqsamp_stats stats;
  pqsamp_trace trace;
  int16_t *plain = malloc(sizeof(int16_t) * SAMPLE_COUNT);
  pqsamp_masked_i16 *masked = malloc(sizeof(pqsamp_masked_i16) * SAMPLE_COUNT);
  clock_t start;
  clock_t stop;
  unsigned center;
  unsigned shares;
  int ret;

  if (plain == NULL || masked == NULL)
  {
    free(masked);
    free(plain);
    return EXIT_FAILURE;
  }

  for (center = 0; center < 2U; center++)
  {
    const char *name = center == 0U ? "zero" : "half";
    pqsamp_center value =
        center == 0U ? PQSAMP_CENTER_ZERO : PQSAMP_CENTER_HALF;

    ret = init_rngs(&coins, &masks, &coin_source, &mask_source);
    start = clock();
    if (ret == PQSAMP_OK && start != (clock_t)-1)
    {
      ret = pqsamp_sample(plain, SAMPLE_COUNT, PQSAMP_PROFILE_S3_2, value,
                          &coins, &stats);
    }
    stop = clock();
    if (ret != PQSAMP_OK || start == (clock_t)-1 || stop == (clock_t)-1)
    {
      free(masked);
      free(plain);
      return EXIT_FAILURE;
    }
    trace.raw_side_batches = 0;
    trace.finished_batches = 0;
    trace.reconstruction_batches = 0;
    report("scalar", name, 1U, SAMPLE_COUNT, stop - start, &stats, &trace);

    for (shares = 1U; shares <= PQSAMP_MAX_SHARES; shares++)
    {
      ret = init_rngs(&coins, &masks, &coin_source, &mask_source);
      start = clock();
      if (ret == PQSAMP_OK && start != (clock_t)-1)
      {
        ret = pqsamp_sample_masked_trace(
            masked, SAMPLE_COUNT, PQSAMP_PROFILE_S3_2, value, shares, &coins,
            shares == 1U ? NULL : &masks, &stats, &trace);
      }
      stop = clock();
      if (ret != PQSAMP_OK || start == (clock_t)-1 || stop == (clock_t)-1)
      {
        free(masked);
        free(plain);
        return EXIT_FAILURE;
      }
      if ((value == PQSAMP_CENTER_ZERO &&
           stats.sec_and_calls != 95U * stats.candidate_batches +
                                      trace.raw_side_batches +
                                      52U * trace.finished_batches) ||
          (value == PQSAMP_CENTER_HALF &&
           stats.sec_and_calls != 89U * stats.candidate_batches +
                                      51U * trace.finished_batches +
                                      3U * trace.reconstruction_batches))
      {
        free(masked);
        free(plain);
        return EXIT_FAILURE;
      }
      report("portable-u32", name, shares, SAMPLE_COUNT, stop - start, &stats,
             &trace);
    }
  }

  free(masked);
  free(plain);
  return EXIT_SUCCESS;
}
