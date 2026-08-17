#include <stdio.h>
#include <stdlib.h>

#include "pqsamp.h"

static int randombytes(void *context, uint8_t *out, size_t n)
{
  FILE *stream = context;

  return fread(out, 1, n, stream) == n ? 0 : -1;
}

int main(void)
{
  pqsamp_masked_i16 masked[8];
  pqsamp_stats stats;
  pqsamp_rng coins;
  pqsamp_rng masks;
  int16_t sample[16];
  FILE *coin_file = fopen("/dev/urandom", "rb");
  FILE *mask_file = fopen("/dev/urandom", "rb");
  unsigned i;
  int rc;

  if (coin_file == NULL || mask_file == NULL)
  {
    if (mask_file != NULL)
    {
      fclose(mask_file);
    }
    if (coin_file != NULL)
    {
      fclose(coin_file);
    }
    return EXIT_FAILURE;
  }
  rc = pqsamp_rng_init(&coins, randombytes, coin_file);
  if (rc == PQSAMP_OK)
  {
    rc = pqsamp_rng_init(&masks, randombytes, mask_file);
  }
  if (rc == PQSAMP_OK)
  {
    rc = pqsamp_sample(sample, 16, PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_ZERO,
                       &coins, &stats);
  }
  if (rc == PQSAMP_OK)
  {
    rc = pqsamp_sample_masked(masked, 8, PQSAMP_PROFILE_S3_2,
                              PQSAMP_CENTER_HALF, 2U, &coins, &masks, NULL);
  }
  if (rc != PQSAMP_OK)
  {
    fprintf(stderr, "sampling failed: %s\n", pqsamp_strerror(rc));
    fclose(mask_file);
    fclose(coin_file);
    return EXIT_FAILURE;
  }
  for (i = 0; i < 16U; i++)
  {
    printf("%d%c", sample[i], i == 15U ? '\n' : ' ');
  }
  printf("candidates=%llu coin_bits=%llu masked=8x2-shares\n",
         (unsigned long long)stats.candidates,
         (unsigned long long)stats.coin_bits);
  fclose(mask_file);
  fclose(coin_file);
  return EXIT_SUCCESS;
}
