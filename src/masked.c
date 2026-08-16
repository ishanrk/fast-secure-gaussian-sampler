#include "internal.h"

// finds the first set lane
static unsigned ctz32(uint32_t x)
{
#if defined(__GNUC__) || defined(__clang__)
  return (unsigned)__builtin_ctz(x);
#else
  unsigned n = 0;

  while ((x & 1U) == 0U)
  {
    x >>= 1;
    n++;
  }
  return n;
#endif
}

// clears a zero center candidate batch
static void batch_zero(pqsamp_batch *batch)
{
  unsigned i;

  for (i = 0; i < PQSAMP_VALUE_BITS; i++)
  {
    pqsamp_word_zero(&batch->y[i]);
  }
  for (i = 0; i < PQSAMP_K_BITS; i++)
  {
    pqsamp_word_zero(&batch->quotient[i]);
    pqsamp_word_zero(&batch->k[i]);
  }
  for (i = 0; i < PQSAMP_BOUNDARY_BITS; i++)
  {
    pqsamp_word_zero(&batch->boundary[i]);
  }
}

// clears a half center g and side value
static void half_value_zero(pqsamp_half_value *value)
{
  unsigned i;

  for (i = 0; i < PQSAMP_HALF_GEOM_BITS; i++)
  {
    pqsamp_word_zero(&value->g[i]);
  }
  pqsamp_word_zero(&value->side);
}

// clears a half center candidate batch
static void half_batch_zero(pqsamp_half_batch *batch)
{
  unsigned i;

  half_value_zero(&batch->value);
  for (i = 0; i < PQSAMP_K_BITS; i++)
  {
    pqsamp_word_zero(&batch->quotient[i]);
    pqsamp_word_zero(&batch->k[i]);
  }
  for (i = 0; i < PQSAMP_BOUNDARY_BITS; i++)
  {
    pqsamp_word_zero(&batch->boundary[i]);
  }
}

// copies one lane across shared bit planes
static void copy_lane(pqsamp_word *out, const pqsamp_word *in, unsigned words,
                      unsigned dst, unsigned src, unsigned shares)
{
  uint32_t dst_mask = UINT32_C(1) << dst;
  unsigned i;

  for (i = 0; i < words; i++)
  {
    unsigned share;

    for (share = 0; share < shares; share++)
    {
      uint32_t bit = (in[i].share[share] >> src) & 1U;

      out[i].share[share] = (out[i].share[share] & ~dst_mask) | (bit << dst);
    }
  }
}

// moves live zero center lanes into free batch lanes
uint32_t pqsamp_compact_batch(pqsamp_batch *out, unsigned *filled,
                              const pqsamp_batch *in, uint32_t live,
                              unsigned shares)
{
  while (live != 0U && *filled < PQSAMP_LANES)
  {
    unsigned src = ctz32(live);
    unsigned dst = *filled;

    copy_lane(out->y, in->y, PQSAMP_VALUE_BITS, dst, src, shares);
    copy_lane(out->quotient, in->quotient, PQSAMP_K_BITS, dst, src, shares);
    copy_lane(out->boundary, in->boundary, PQSAMP_BOUNDARY_BITS, dst, src,
              shares);
    copy_lane(out->k, in->k, PQSAMP_K_BITS, dst, src, shares);
    (*filled)++;
    live &= live - 1U;
  }
  return live;
}

// moves live half center lanes into free batch lanes
uint32_t pqsamp_compact_half_batch(pqsamp_half_batch *out, unsigned *filled,
                                   const pqsamp_half_batch *in, uint32_t live,
                                   unsigned shares)
{
  while (live != 0U && *filled < PQSAMP_LANES)
  {
    unsigned src = ctz32(live);
    unsigned dst = *filled;

    copy_lane(out->value.g, in->value.g, PQSAMP_HALF_GEOM_BITS, dst, src,
              shares);
    copy_lane(&out->value.side, &in->value.side, 1U, dst, src, shares);
    copy_lane(out->quotient, in->quotient, PQSAMP_K_BITS, dst, src, shares);
    copy_lane(out->boundary, in->boundary, PQSAMP_BOUNDARY_BITS, dst, src,
              shares);
    copy_lane(out->k, in->k, PQSAMP_K_BITS, dst, src, shares);
    (*filled)++;
    live &= live - 1U;
  }
  return live;
}

// appends accepted zero center samples to output
static void append_accepted(pqsamp_masked_i16 *out, size_t n, size_t *written,
                            const pqsamp_batch *batch, uint32_t accept,
                            unsigned shares)
{
  pqsamp_masked_i16 lane[PQSAMP_LANES];

  pqsamp_unpack16(lane, batch->y, shares);
  while (accept != 0U && *written < n)
  {
    unsigned i = ctz32(accept);

    out[*written] = lane[i];
    (*written)++;
    accept &= accept - 1U;
  }
}

// finishes and copies one zero center batch
static int finish_pending(pqsamp_state *state, pqsamp_masked_i16 *out, size_t n,
                          size_t *written, const pqsamp_batch *pending,
                          uint32_t active, const pqsamp_params *params,
                          pqsamp_trace *trace)
{
  uint32_t accept;
  int rc = pqsamp_sample_finish(state, pending, active, &accept, params);

  if (rc != PQSAMP_OK)
  {
    return rc;
  }
  if (trace != NULL)
  {
    trace->finished_batches++;
  }
  append_accepted(out, n, written, pending, accept, state->shares);
  return PQSAMP_OK;
}

// fills zero center output from bounded batches
static int sample_full(pqsamp_masked_i16 *out, size_t n, pqsamp_state *state,
                       const pqsamp_params *params, size_t max_batches,
                       pqsamp_trace *trace)
{
  pqsamp_batch pending;
  pqsamp_zero_side_pool side_pool;
  size_t batch_count;
  size_t written = 0;
  unsigned filled = 0;
  int rc;

  batch_zero(&pending);
  pqsamp_zero_side_pool_init(&side_pool);
  for (batch_count = 0; batch_count < max_batches && written < n; batch_count++)
  {
    pqsamp_batch fresh;
    uint32_t live;

    rc = pqsamp_sample_pre(state, &fresh, &live, params, &side_pool, trace);
    if (rc != PQSAMP_OK)
    {
      return rc;
    }
    while (live != 0U)
    {
      live =
          pqsamp_compact_batch(&pending, &filled, &fresh, live, state->shares);
      if (filled == PQSAMP_LANES)
      {
        rc = finish_pending(state, out, n, &written, &pending, UINT32_MAX,
                            params, trace);
        if (rc != PQSAMP_OK)
        {
          return rc;
        }
        batch_zero(&pending);
        filled = 0;
        if (written == n)
        {
          break;
        }
      }
    }
  }
  if (written < n && filled != 0U)
  {
    uint32_t active = (UINT32_C(1) << filled) - 1U;

    rc = finish_pending(state, out, n, &written, &pending, active, params,
                        trace);
    if (rc != PQSAMP_OK)
    {
      return rc;
    }
  }
  return written == n ? PQSAMP_OK : PQSAMP_ERR_BOUND;
}

// reconstructs and writes accepted half center values
static int flush_half(pqsamp_state *state, pqsamp_masked_i16 *out, size_t n,
                      size_t *written, pqsamp_half_value *fifo,
                      unsigned *filled, pqsamp_trace *trace)
{
  pqsamp_word y[PQSAMP_VALUE_BITS];
  pqsamp_masked_i16 lane[PQSAMP_LANES];
  unsigned count = *filled;
  unsigned i;
  int rc = pqsamp_half_reconstruct(state, y, fifo);

  if (rc != PQSAMP_OK)
  {
    return rc;
  }
  if (trace != NULL)
  {
    trace->reconstruction_batches++;
  }
  pqsamp_unpack16(lane, y, state->shares);
  if ((size_t)count > n - *written)
  {
    count = (unsigned)(n - *written);
  }
  for (i = 0; i < count; i++)
  {
    out[*written + i] = lane[i];
  }
  *written += count;
  *filled = 0;
  half_value_zero(fifo);
  return PQSAMP_OK;
}

// stores accepted half center records until one flush
static int append_half(pqsamp_state *state, pqsamp_masked_i16 *out, size_t n,
                       size_t *written, pqsamp_half_value *fifo,
                       unsigned *filled, const pqsamp_half_value *value,
                       uint32_t accept, pqsamp_trace *trace)
{
  while (accept != 0U && *written < n)
  {
    unsigned src;
    int rc;

    if ((size_t)*filled == n - *written)
    {
      return flush_half(state, out, n, written, fifo, filled, trace);
    }
    src = ctz32(accept);
    copy_lane(fifo->g, value->g, PQSAMP_HALF_GEOM_BITS, *filled, src,
              state->shares);
    copy_lane(&fifo->side, &value->side, 1U, *filled, src, state->shares);
    (*filled)++;
    accept &= accept - 1U;
    if (*filled == PQSAMP_LANES || (size_t)*filled == n - *written)
    {
      rc = flush_half(state, out, n, written, fifo, filled, trace);
      if (rc != PQSAMP_OK)
      {
        return rc;
      }
    }
  }
  return PQSAMP_OK;
}

// finishes one half center batch and queues accepted records
static int finish_half_pending(pqsamp_state *state, pqsamp_masked_i16 *out,
                               size_t n, size_t *written,
                               pqsamp_half_value *fifo, unsigned *fifo_filled,
                               const pqsamp_half_batch *pending,
                               uint32_t active, const pqsamp_params *params,
                               pqsamp_trace *trace)
{
  uint32_t accept;
  int rc = pqsamp_sample_half_finish(state, pending, active, &accept, params);

  if (rc != PQSAMP_OK)
  {
    return rc;
  }
  if (trace != NULL)
  {
    trace->finished_batches++;
  }
  return append_half(state, out, n, written, fifo, fifo_filled, &pending->value,
                     accept, trace);
}

// fills half center output from bounded batches
static int sample_half(pqsamp_masked_i16 *out, size_t n, pqsamp_state *state,
                       const pqsamp_params *params, size_t max_batches,
                       pqsamp_trace *trace)
{
  pqsamp_half_batch pending;
  pqsamp_half_value fifo;
  size_t batch_count;
  size_t written = 0;
  unsigned filled = 0;
  unsigned fifo_filled = 0;
  int rc;

  half_batch_zero(&pending);
  half_value_zero(&fifo);
  for (batch_count = 0; batch_count < max_batches && written + fifo_filled < n;
       batch_count++)
  {
    pqsamp_half_batch fresh;
    uint32_t live;

    rc = pqsamp_sample_half_pre(state, &fresh, &live, params);
    if (rc != PQSAMP_OK)
    {
      return rc;
    }
    while (live != 0U)
    {
      live = pqsamp_compact_half_batch(&pending, &filled, &fresh, live,
                                       state->shares);
      if (filled == PQSAMP_LANES)
      {
        rc = finish_half_pending(state, out, n, &written, &fifo, &fifo_filled,
                                 &pending, UINT32_MAX, params, trace);
        if (rc != PQSAMP_OK)
        {
          return rc;
        }
        half_batch_zero(&pending);
        filled = 0;
        if (written + fifo_filled == n)
        {
          break;
        }
      }
    }
  }
  if (written + fifo_filled < n && filled != 0U)
  {
    uint32_t active = (UINT32_C(1) << filled) - 1U;

    rc = finish_half_pending(state, out, n, &written, &fifo, &fifo_filled,
                             &pending, active, params, trace);
    if (rc != PQSAMP_OK)
    {
      return rc;
    }
  }
  return written == n ? PQSAMP_OK : PQSAMP_ERR_BOUND;
}

// runs the masked sampler and records internal counters
int pqsamp_sample_masked_trace(pqsamp_masked_i16 *out, size_t n,
                               pqsamp_profile profile, pqsamp_center center,
                               unsigned shares, pqsamp_rng *coins,
                               pqsamp_rng *masks, pqsamp_stats *stats,
                               pqsamp_trace *trace)
{
  const pqsamp_params *params = pqsamp_profile_get(profile, center);
  pqsamp_state state;
  uint64_t coins_before;
  uint64_t masks_before = 0;
  size_t blocks;
  size_t max_batches;
  int rc;

  stats_clear(stats);
  if (trace != NULL)
  {
    trace->raw_side_batches = 0;
    trace->finished_batches = 0;
    trace->reconstruction_batches = 0;
  }
  if ((out == NULL && n != 0U) || coins == NULL || shares == 0U ||
      shares > PQSAMP_MAX_SHARES ||
      (shares > 1U && streams_are_distinct(coins, masks) == 0))
  {
    if (out != NULL)
    {
      masked_clear(out, n);
    }
    return PQSAMP_ERR_PARAM;
  }
  rc = pqsamp_profile_check(params);
  if (rc != PQSAMP_OK)
  {
    if (out != NULL)
    {
      masked_clear(out, n);
    }
    return rc;
  }
  if (n == 0U)
  {
    return PQSAMP_OK;
  }
  blocks = n / PQSAMP_LANES + (n % PQSAMP_LANES != 0U ? 1U : 0U);
  if (blocks > SIZE_MAX / PQSAMP_BATCHES_PER_BLOCK)
  {
    masked_clear(out, n);
    return PQSAMP_ERR_PARAM;
  }
  max_batches = blocks * PQSAMP_BATCHES_PER_BLOCK;
  masked_clear(out, n);
  state.shares = shares;
  state.coins = coins;
  state.masks = masks;
  state.stats = stats;
  coins_before = coins->bits_used;
  if (masks != NULL)
  {
    masks_before = masks->bits_used;
  }

  if (center == PQSAMP_CENTER_HALF)
  {
    rc = sample_half(out, n, &state, params, max_batches, trace);
  }
  else
  {
    rc = sample_full(out, n, &state, params, max_batches, trace);
  }
  if (rc != PQSAMP_OK)
  {
    masked_clear(out, n);
  }
  if (stats != NULL)
  {
    stats->coin_bits = coins->bits_used - coins_before;
    stats->mask_bits = masks == NULL ? 0U : masks->bits_used - masks_before;
  }
  return rc;
}

// returns boolean shared gaussian samples
int pqsamp_sample_masked(pqsamp_masked_i16 *out, size_t n,
                         pqsamp_profile profile, pqsamp_center center,
                         unsigned shares, pqsamp_rng *coins, pqsamp_rng *masks,
                         pqsamp_stats *stats)
{
  return pqsamp_sample_masked_trace(out, n, profile, center, shares, coins,
                                    masks, stats, NULL);
}
