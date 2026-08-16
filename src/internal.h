#ifndef PQSAMP_SRC_INTERNAL_H
#define PQSAMP_SRC_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "pqsamp.h"

#define PQSAMP_LANES 32U
#define PQSAMP_VALUE_BITS 16U
#define PQSAMP_HALF_VALUE_BITS 5U
#define PQSAMP_HALF_GEOM_BITS 4U
#define PQSAMP_GEOM_BITS 14U
#define PQSAMP_K_BITS 7U
#define PQSAMP_K_SAT_MAX 69U
#define PQSAMP_THRESHOLD_BITS 45U
#define PQSAMP_BOUNDARY_BITS (PQSAMP_THRESHOLD_BITS + 1U)
#define PQSAMP_BATCHES_PER_BLOCK 4U
#define PQSAMP_SIDE_RETRIES 64U

#if defined(__GNUC__) || defined(__clang__)
#define PQSAMP_NOINLINE __attribute__((noinline))
#else
#define PQSAMP_NOINLINE
#endif

typedef struct
{
  int16_t y;
  uint8_t quotient;
  uint8_t valid;
  uint64_t boundary_count;
} pqsamp_candidate;

typedef struct
{
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

typedef struct
{
  uint32_t share[PQSAMP_MAX_SHARES];
} pqsamp_word;

typedef struct
{
  pqsamp_word y[PQSAMP_VALUE_BITS];
  pqsamp_word quotient[PQSAMP_K_BITS];
  pqsamp_word boundary[PQSAMP_BOUNDARY_BITS];
  pqsamp_word k[PQSAMP_K_BITS];
} pqsamp_batch;

typedef struct
{
  pqsamp_word word[2];
  unsigned fill;
} pqsamp_zero_side_pool;

typedef struct
{
  pqsamp_word g[PQSAMP_HALF_GEOM_BITS];
  pqsamp_word side;
} pqsamp_half_value;

typedef struct
{
  pqsamp_half_value value;
  pqsamp_word quotient[PQSAMP_K_BITS];
  pqsamp_word boundary[PQSAMP_BOUNDARY_BITS];
  pqsamp_word k[PQSAMP_K_BITS];
} pqsamp_half_batch;

typedef struct
{
  uint64_t raw_side_batches;
  uint64_t finished_batches;
  uint64_t reconstruction_batches;
} pqsamp_trace;

typedef struct
{
  unsigned shares;
  pqsamp_rng *coins;
  pqsamp_rng *masks;
  pqsamp_stats *stats;
} pqsamp_state;

// applies the scalar rejection rule
static inline int candidate_accept(const pqsamp_candidate *entry, unsigned k,
                                   uint64_t u)
{
  if (entry->valid == 0U || k < entry->quotient)
  {
    return 0;
  }
  return k > entry->quotient || u < entry->boundary_count;
}

// clears optional sampling counters
static inline void stats_clear(pqsamp_stats *stats)
{
  if (stats != NULL)
  {
    stats->candidate_batches = 0;
    stats->candidates = 0;
    stats->accepted = 0;
    stats->sec_and_calls = 0;
    stats->coin_bits = 0;
    stats->mask_bits = 0;
  }
}

// clears every share in the output
static inline void masked_clear(pqsamp_masked_i16 *out, size_t n)
{
  size_t i;

  for (i = 0; i < n; i++)
  {
    unsigned share;

    for (share = 0; share < PQSAMP_MAX_SHARES; share++)
    {
      out[i].share[share] = 0;
    }
  }
}

// rejects one rng stream reused for coins and masks
static inline int streams_are_distinct(const pqsamp_rng *coins,
                                       const pqsamp_rng *masks)
{
  return masks != NULL && masks != coins &&
         (masks->randombytes != coins->randombytes ||
          masks->context != coins->context);
}

// reads up to 32 low bits from the stream
int pqsamp_rng_bits(pqsamp_rng *rng, unsigned count, uint32_t *value);
// reads one 32 bit word from the stream
int pqsamp_rng_word(pqsamp_rng *rng, uint32_t *value);
// reads up to 64 low bits from the stream
int pqsamp_rng_bits64(pqsamp_rng *rng, unsigned count, uint64_t *value);

// finds one fixed profile and center table
const pqsamp_params *pqsamp_profile_get(pqsamp_profile profile,
                                        pqsamp_center center);
// checks that a profile matches the supported bounds
int pqsamp_profile_check(const pqsamp_params *params);
// counts the bits needed to store a value
unsigned pqsamp_bit_width_u32(uint32_t value);

// packs 32 shared samples into bit planes
void pqsamp_pack16(pqsamp_word out[PQSAMP_VALUE_BITS],
                   const pqsamp_masked_i16 in[PQSAMP_LANES], unsigned shares);
// unpacks bit planes into 32 shared samples
void pqsamp_unpack16(pqsamp_masked_i16 out[PQSAMP_LANES],
                     const pqsamp_word in[PQSAMP_VALUE_BITS], unsigned shares);

// shares uniform coin words with fresh masks
int pqsamp_uniform(pqsamp_state *state, pqsamp_word *out, unsigned bits);
// combines shared words with fresh pair masks
int pqsamp_sec_and(pqsamp_state *state, pqsamp_word *out,
                   const pqsamp_word *left, const pqsamp_word *right);
// tests two shared integers for equality
int pqsamp_sec_eq(pqsamp_state *state, pqsamp_word *out,
                  const pqsamp_word *left, const pqsamp_word *right,
                  unsigned bits);
// tests whether one shared integer is at most another
int pqsamp_sec_leq(pqsamp_state *state, pqsamp_word *out,
                   const pqsamp_word *left, const pqsamp_word *right,
                   unsigned bits);
// tests whether one shared integer is below another
int pqsamp_sec_lt(pqsamp_state *state, pqsamp_word *out,
                  const pqsamp_word *left, const pqsamp_word *right,
                  unsigned bits);
// refreshes every share before revealing the value
int pqsamp_unmask(pqsamp_state *state, const pqsamp_word *value, uint32_t *out);
// clears every share in one word
void pqsamp_word_zero(pqsamp_word *value);
// flips a shared word without revealing it
void pqsamp_word_not(pqsamp_word *out, const pqsamp_word *value,
                     unsigned shares);
// xors two shared words share by share
void pqsamp_word_xor(pqsamp_word *out, const pqsamp_word *left,
                     const pqsamp_word *right, unsigned shares);

// clears a zero center side pool
void pqsamp_zero_side_pool_init(pqsamp_zero_side_pool *pool);
// returns 32 side bits with exact one third chance of one
int pqsamp_zero_side(pqsamp_state *state, pqsamp_zero_side_pool *pool,
                     pqsamp_word *out, pqsamp_trace *trace);
// builds one zero center candidate batch
int pqsamp_sample_pre(pqsamp_state *state, pqsamp_batch *batch, uint32_t *live,
                      const pqsamp_params *params,
                      pqsamp_zero_side_pool *side_pool, pqsamp_trace *trace);
// finishes a zero center pending batch
int pqsamp_sample_finish(pqsamp_state *state, const pqsamp_batch *batch,
                         uint32_t active, uint32_t *accept,
                         const pqsamp_params *params);
// builds one half center candidate batch
int pqsamp_sample_half_pre(pqsamp_state *state, pqsamp_half_batch *batch,
                           uint32_t *live, const pqsamp_params *params);
// finishes a half center pending batch
int pqsamp_sample_half_finish(pqsamp_state *state,
                              const pqsamp_half_batch *batch, uint32_t active,
                              uint32_t *accept, const pqsamp_params *params);
// reconstructs accepted half center values with three and gates
int pqsamp_half_reconstruct(pqsamp_state *state,
                            pqsamp_word out[PQSAMP_VALUE_BITS],
                            const pqsamp_half_value *value);
// moves live zero center lanes into free batch lanes
uint32_t pqsamp_compact_batch(pqsamp_batch *out, unsigned *filled,
                              const pqsamp_batch *in, uint32_t live,
                              unsigned shares);
// moves live half center lanes into free batch lanes
uint32_t pqsamp_compact_half_batch(pqsamp_half_batch *out, unsigned *filled,
                                   const pqsamp_half_batch *in, uint32_t live,
                                   unsigned shares);
// runs the masked sampler and records internal counters
int pqsamp_sample_masked_trace(pqsamp_masked_i16 *out, size_t n,
                               pqsamp_profile profile, pqsamp_center center,
                               unsigned shares, pqsamp_rng *coins,
                               pqsamp_rng *masks, pqsamp_stats *stats,
                               pqsamp_trace *trace);
// counts zeros before the first one
int pqsamp_scalar_geom(pqsamp_rng *rng, unsigned bits, unsigned *value);

#endif /* pqsamp_src_internal_h */
