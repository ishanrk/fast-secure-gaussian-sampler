#ifndef PQSAMP_H
#define PQSAMP_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#define PQSAMP_VERSION_MAJOR 0
#define PQSAMP_VERSION_MINOR 2
#define PQSAMP_VERSION_PATCH 0
#define PQSAMP_PARAMS_ABI_VERSION 1U

#define PQSAMP_LANES 32U
#define PQSAMP_MAX_SHARES 4U
#define PQSAMP_BATCH_CANDIDATES 128U

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

  /* Return zero only after filling all size bytes. */
  typedef int (*pqsamp_randombytes)(void *context, uint8_t *out, size_t size);

  typedef struct
  {
    pqsamp_randombytes randombytes;
    void *context;
    uint64_t buffer;
    uint64_t bits_used;
    unsigned available;
    int error;
  } pqsamp_rng;

  typedef struct
  {
    int16_t y;
    uint8_t quotient;
    uint8_t valid;
    uint64_t boundary_count;
  } pqsamp_candidate;

  /*
   * A public profile generated and validated offline.  Both side tables live
   * for every call and contain geom_count rows.  They encode floor(c)-G and
   * floor(c)+G+1, respectively.
   */
  typedef struct
  {
    uint32_t abi_version;
    uint32_t s_num;
    uint32_t s_den;
    int32_t center_num;
    uint32_t center_den;
    uint32_t side_num;
    uint32_t side_den;
    unsigned geom_count;
    unsigned k_sat;
    unsigned threshold_bits;
    const pqsamp_candidate *side[2];
  } pqsamp_params;

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
    /* XOR of the active shares is the 16-bit two's-complement sample. */
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

  /* Initialize an LSB-first, caller-owned random-bit stream. */
  PQSAMP_WARN_UNUSED int pqsamp_rng_init(pqsamp_rng *rng,
                                         pqsamp_randombytes randombytes,
                                         void *context);

  PQSAMP_WARN_UNUSED int pqsamp_rng_bits(pqsamp_rng *rng, unsigned count,
                                         uint32_t *value);

  uint64_t pqsamp_rng_bits_used(const pqsamp_rng *rng);

  const pqsamp_params *pqsamp_params_get(pqsamp_profile profile,
                                         pqsamp_center center);

  PQSAMP_WARN_UNUSED int pqsamp_params_check(const pqsamp_params *params);

  /*
   * Generate scalar samples.  On failure, out[0..count) is zeroed.  A zero
   * count permits out to be NULL.
   */
  PQSAMP_WARN_UNUSED int pqsamp_generate(int16_t *out, size_t count,
                                         const pqsamp_params *params,
                                         pqsamp_rng *coins,
                                         pqsamp_stats *stats);

  /*
   * Generate Boolean-shared samples.  For shares > 1, coins and masks must be
   * cryptographically independent streams; equal callback/context pairs are
   * rejected.  One share is the unmasked bitsliced diagnostic path and permits
   * masks to be NULL.  On failure, out[0..count) is zeroed.
   *
   * Stats are reset per call.  accepted includes valid overproduction that the
   * fixed scheduler does not return.
   */
  PQSAMP_WARN_UNUSED int pqsamp_generate_masked(
      pqsamp_masked_i16 *out, size_t count, const pqsamp_params *params,
      unsigned shares, pqsamp_rng *coins, pqsamp_rng *masks,
      pqsamp_stats *stats);

  /* Diagnostic helper.  value must be non-NULL and shares must be 1..4. */
  int16_t pqsamp_reconstruct(const pqsamp_masked_i16 *value, unsigned shares);

  const char *pqsamp_strerror(int error);

#if defined(__cplusplus)
}
#endif

#endif /* PQSAMP_H */
