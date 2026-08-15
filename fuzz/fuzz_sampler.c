#include <stddef.h>
#include <stdint.h>

#include "pqsamp.h"

typedef struct
{
  const uint8_t *data;
  size_t size;
  size_t offset;
} pqsamp_fuzz_input;

static int pqsamp_fuzz_randombytes(void *context, uint8_t *out, size_t size)
{
  pqsamp_fuzz_input *input = context;
  size_t i;

  if (size > input->size - input->offset)
  {
    return -1;
  }
  for (i = 0; i < size; i++)
  {
    out[i] = input->data[input->offset + i];
  }
  input->offset += size;
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  pqsamp_fuzz_input input = {data, size, 0};
  pqsamp_rng rng;
  int16_t sample = 0;
  int ret;

  ret = pqsamp_rng_init(&rng, pqsamp_fuzz_randombytes, &input);
  if (ret != PQSAMP_OK)
  {
    return 0;
  }
  ret = pqsamp_generate(
      &sample, 1,
      pqsamp_params_get(
          (size & 1U) != 0U ? PQSAMP_PROFILE_S1521_1000 : PQSAMP_PROFILE_S3_2,
          (size & 2U) != 0U ? PQSAMP_CENTER_HALF : PQSAMP_CENTER_ZERO),
      &rng, NULL);
  if (ret == PQSAMP_OK && (sample < -13 || sample > 13))
  {
    __builtin_trap();
  }
  return 0;
}
