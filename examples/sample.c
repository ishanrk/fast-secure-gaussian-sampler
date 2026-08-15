#include <stdio.h>
#include <stdlib.h>

#include "pqsamp.h"

static int pqsamp_example_randombytes(void *context, uint8_t *out, size_t size)
{
  FILE *stream = context;

  return fread(out, 1, size, stream) == size ? 0 : -1;
}

int main(void)
{
  const pqsamp_params *params =
      pqsamp_params_get(PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_ZERO);
  pqsamp_stats stats;
  pqsamp_rng rng;
  int16_t samples[16];
  FILE *random = fopen("/dev/urandom", "rb");
  unsigned i;
  int ret;

  if (random == NULL)
  {
    fputs("cannot open /dev/urandom\n", stderr);
    return EXIT_FAILURE;
  }
  ret = pqsamp_rng_init(&rng, pqsamp_example_randombytes, random);
  if (ret == PQSAMP_OK)
  {
    ret = pqsamp_generate(samples, 16, params, &rng, &stats);
  }
  if (ret != PQSAMP_OK)
  {
    fprintf(stderr, "sampling failed: %s\n", pqsamp_strerror(ret));
    fclose(random);
    return EXIT_FAILURE;
  }
  for (i = 0; i < 16U; i++)
  {
    printf("%d%c", samples[i], i == 15U ? '\n' : ' ');
  }
  printf("candidates=%llu, coin_bits=%llu\n",
         (unsigned long long)stats.candidates,
         (unsigned long long)stats.coin_bits);
  fclose(random);
  return EXIT_SUCCESS;
}
