#include <stdio.h>
#include <stdlib.h>

#include "pqsamp_hawk.h"

static int pqsamp_hawk_example_randombytes(void *context, uint8_t *out,
                                           size_t size)
{
  FILE *stream = context;

  return fread(out, 1, size, stream) == size ? 0 : -1;
}

int main(void)
{
  pqsamp_masked_bit center[PQSAMP_LANES];
  pqsamp_masked_i16 sample[PQSAMP_LANES];
  pqsamp_rng coins;
  pqsamp_rng masks;
  FILE *coin_file = fopen("/dev/urandom", "rb");
  FILE *mask_file = fopen("/dev/urandom", "rb");
  unsigned i;
  int ret;

  if (coin_file == NULL || mask_file == NULL)
  {
    fputs("cannot open /dev/urandom\n", stderr);
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
  ret = pqsamp_rng_init(&coins, pqsamp_hawk_example_randombytes, coin_file);
  if (ret == PQSAMP_OK)
  {
    ret = pqsamp_rng_init(&masks, pqsamp_hawk_example_randombytes, mask_file);
  }
  for (i = 0; ret == PQSAMP_OK && i < PQSAMP_LANES; i++)
  {
    uint32_t mask;
    uint8_t bit = (uint8_t)(i & 1U);

    ret = pqsamp_rng_bits(&masks, 1U, &mask);
    center[i].share[1] = (uint8_t)mask;
    center[i].share[0] = (uint8_t)(mask ^ bit);
    center[i].share[2] = 0;
    center[i].share[3] = 0;
  }
  if (ret == PQSAMP_OK)
  {
    ret = pqsamp_hawk_sample_masked(sample, PQSAMP_LANES, PQSAMP_PROFILE_S3_2,
                                    center, 2U, &coins, &masks);
  }
  if (ret != PQSAMP_OK)
  {
    fprintf(stderr, "adapter failed: %s\n", pqsamp_strerror(ret));
    fclose(mask_file);
    fclose(coin_file);
    return EXIT_FAILURE;
  }
  for (i = 0; i < PQSAMP_LANES; i++)
  {
    int16_t value = pqsamp_reconstruct(&sample[i], 2U);

    if (((uint16_t)value & 1U) != (i & 1U))
    {
      fputs("adapter parity check failed\n", stderr);
      fclose(mask_file);
      fclose(coin_file);
      return EXIT_FAILURE;
    }
    printf("%d%c", value, i + 1U == PQSAMP_LANES ? '\n' : ' ');
  }
  fclose(mask_file);
  fclose(coin_file);
  return EXIT_SUCCESS;
}
