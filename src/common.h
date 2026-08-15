#ifndef PQSAMP_SRC_COMMON_H
#define PQSAMP_SRC_COMMON_H

#include <stddef.h>
#include <stdint.h>

#include "pqsamp.h"

#define PQSAMP_VALUE_BITS 16U
#define PQSAMP_MAX_PLANES 80U
#define PQSAMP_FIXED_BATCHES (PQSAMP_BATCH_CANDIDATES / PQSAMP_LANES)
#define PQSAMP_SIDE_RETRIES 64U

#if (PQSAMP_BATCH_CANDIDATES % PQSAMP_LANES) != 0U
#error "PQSAMP_BATCH_CANDIDATES must be a multiple of PQSAMP_LANES"
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PQSAMP_NOINLINE __attribute__((noinline))
#else
#define PQSAMP_NOINLINE
#endif

typedef struct
{
  uint32_t share[PQSAMP_MAX_SHARES];
} pqsamp_word;

typedef struct
{
  unsigned shares;
  pqsamp_rng *coins;
  pqsamp_rng *masks;
  pqsamp_stats *stats;
} pqsamp_state;

int pqsamp_rng_word(pqsamp_rng *rng, uint32_t *value);
int pqsamp_rng_bits64(pqsamp_rng *rng, unsigned count, uint64_t *value);
unsigned pqsamp_bit_width_u32(uint32_t value);

void pqsamp_pack16(pqsamp_word out[PQSAMP_VALUE_BITS],
                   const pqsamp_masked_i16 in[PQSAMP_LANES], unsigned shares);
void pqsamp_unpack16(pqsamp_masked_i16 out[PQSAMP_LANES],
                     const pqsamp_word in[PQSAMP_VALUE_BITS], unsigned shares);

int pqsamp_uniform(pqsamp_state *state, pqsamp_word *out, unsigned bits);
int pqsamp_sec_and(pqsamp_state *state, pqsamp_word *out,
                   const pqsamp_word *left, const pqsamp_word *right);
int pqsamp_sec_eq(pqsamp_state *state, pqsamp_word *out,
                  const pqsamp_word *left, const pqsamp_word *right,
                  unsigned bits);
int pqsamp_sec_leq(pqsamp_state *state, pqsamp_word *out,
                   const pqsamp_word *left, const pqsamp_word *right,
                   unsigned bits);
int pqsamp_sec_lt(pqsamp_state *state, pqsamp_word *out,
                  const pqsamp_word *left, const pqsamp_word *right,
                  unsigned bits);
int pqsamp_unmask(pqsamp_state *state, const pqsamp_word *value, uint32_t *out);
void pqsamp_word_zero(pqsamp_word *value);
void pqsamp_word_not(pqsamp_word *out, const pqsamp_word *value,
                     unsigned shares);
void pqsamp_word_xor(pqsamp_word *out, const pqsamp_word *left,
                     const pqsamp_word *right, unsigned shares);
void pqsamp_word_xor_mask(pqsamp_word *out, const pqsamp_word *value,
                          uint32_t mask, unsigned shares);

int pqsamp_sample_batch(pqsamp_state *state, pqsamp_word *out, uint32_t *accept,
                        const pqsamp_params *params);

#endif /* PQSAMP_SRC_COMMON_H */
