#ifndef PQSAMP_ADAPTERS_HAWK_H
#define PQSAMP_ADAPTERS_HAWK_H

#include <stddef.h>
#include <stdint.h>

#include "pqsamp.h"

#if defined(__cplusplus)
extern "C"
{
#endif

  typedef struct
  {
    // xor of active low bits is the secret center bit
    uint8_t share[PQSAMP_MAX_SHARES];
  } pqsamp_masked_bit;

  // masked two center research adapter
  PQSAMP_WARN_UNUSED int pqsamp_hawk_sample_masked(
      pqsamp_masked_i16 *out, size_t n, pqsamp_profile profile,
      const pqsamp_masked_bit *center, unsigned shares, pqsamp_rng *coins,
      pqsamp_rng *masks);

#if defined(__cplusplus)
}
#endif

#endif /* pqsamp_adapters_hawk_h */
