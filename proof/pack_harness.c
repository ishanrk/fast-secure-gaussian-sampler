#include <assert.h>
#include <stdint.h>

#include "common.h"

uint16_t nondet_uint16_t(void);

int main(void)
{
  pqsamp_masked_i16 input[PQSAMP_LANES];
  pqsamp_masked_i16 output[PQSAMP_LANES];
  pqsamp_word planes[PQSAMP_VALUE_BITS];
  unsigned lane;

  for (lane = 0; lane < PQSAMP_LANES; lane++)
  {
    unsigned share;

    for (share = 0; share < PQSAMP_MAX_SHARES; share++)
    {
      input[lane].share[share] = nondet_uint16_t();
    }
  }
  pqsamp_pack16(planes, input, PQSAMP_MAX_SHARES);
  pqsamp_unpack16(output, planes, PQSAMP_MAX_SHARES);
  for (lane = 0; lane < PQSAMP_LANES; lane++)
  {
    unsigned share;

    for (share = 0; share < PQSAMP_MAX_SHARES; share++)
    {
      assert(output[lane].share[share] == input[lane].share[share]);
    }
  }
  return 0;
}
