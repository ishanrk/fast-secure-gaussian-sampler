#ifndef PQSAMP_H
#define PQSAMP_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#define PQSAMP_MAX_SHARES 4U

#if defined(__GNUC__) || defined(__clang__)
#define PQSAMP_WARN_UNUSED __attribute__((warn_unused_result))
#else
#define PQSAMP_WARN_UNUSED
#endif

  enum
  {
    PQSAMP_OK = 0,
    PQSAMP_ERR_PARAM = -1,
    PQSAMP_ERR_RANDOM = -2,
    PQSAMP_ERR_BOUND = -3
  };

  // fills all bytes or fails
  typedef int (*pqsamp_randombytes)(void *context, uint8_t *out, size_t n);

  typedef struct
  {
    pqsamp_randombytes randombytes;
    void *context;
    uint64_t buffer;
    uint64_t bits_used;
    unsigned available;
    int error;
  } pqsamp_rng;

  typedef enum
  {
    PQSAMP_PROFILE_S3_2 = 0,
    PQSAMP_PROFILE_S1521_1000 = 1
  } pqsamp_profile;

  typedef enum
  {
    PQSAMP_CENTER_ZERO = 0,
    PQSAMP_CENTER_HALF = 1
  } pqsamp_center;

  typedef struct
  {
    // shares xor to sample
    uint16_t share[PQSAMP_MAX_SHARES];
  } pqsamp_masked_i16;

  typedef struct
  {
    uint64_t candidate_batches;
    uint64_t candidates;
    uint64_t accepted;
    uint64_t sec_and_calls;
    uint64_t coin_bits;
    uint64_t mask_bits;
  } pqsamp_stats;

  PQSAMP_WARN_UNUSED int pqsamp_rng_init(pqsamp_rng *rng,
                                         pqsamp_randombytes randombytes,
                                         void *context);

  // clears output on failure
  PQSAMP_WARN_UNUSED int pqsamp_sample(int16_t *out, size_t n,
                                       pqsamp_profile profile,
                                       pqsamp_center center, pqsamp_rng *rng,
                                       pqsamp_stats *stats);

  // clears output on failure
  // masks null for one share
  PQSAMP_WARN_UNUSED int pqsamp_sample_masked(
      pqsamp_masked_i16 *out, size_t n, pqsamp_profile profile,
      pqsamp_center center, unsigned shares, pqsamp_rng *coins,
      pqsamp_rng *masks, pqsamp_stats *stats);

  const char *pqsamp_strerror(int error);

#if defined(__cplusplus)
}
#endif

#endif /* pqsamp_h */
