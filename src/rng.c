#include "internal.h"

// loads one little endian word
static uint64_t load64(const uint8_t src[8])
{
  uint64_t value = 0;
  unsigned i;

  for (i = 0; i < 8; i++)
  {
    value |= (uint64_t)src[i] << (8U * i);
  }
  return value;
}

// fills the rng bit buffer
static int refill(pqsamp_rng *rng)
{
  uint8_t bytes[8];

  if (rng->randombytes(rng->context, bytes, sizeof(bytes)) != 0)
  {
    rng->error = PQSAMP_ERR_RANDOM;
    return rng->error;
  }
  rng->buffer = load64(bytes);
  rng->available = 64U;
  return PQSAMP_OK;
}

// starts a caller owned rng stream
int pqsamp_rng_init(pqsamp_rng *rng, pqsamp_randombytes randombytes,
                    void *context)
{
  if (rng == NULL || randombytes == NULL)
  {
    return PQSAMP_ERR_PARAM;
  }
  rng->randombytes = randombytes;
  rng->context = context;
  rng->buffer = 0;
  rng->bits_used = 0;
  rng->available = 0;
  rng->error = PQSAMP_OK;
  return PQSAMP_OK;
}

// reads up to 32 low bits from the stream
int pqsamp_rng_bits(pqsamp_rng *rng, unsigned count, uint32_t *value)
{
  uint32_t out = 0;
  unsigned written = 0;

  if (rng == NULL || value == NULL || count > 32U)
  {
    return PQSAMP_ERR_PARAM;
  }
  if (rng->error != PQSAMP_OK)
  {
    return rng->error;
  }
  while (written < count)
  {
    unsigned take;
    uint64_t mask;

    if (rng->available == 0U)
    {
      int ret = refill(rng);

      if (ret != PQSAMP_OK)
      {
        return ret;
      }
    }
    take = count - written;
    if (take > rng->available)
    {
      take = rng->available;
    }
    mask = take >= 32U ? UINT64_C(0xffffffff) : (UINT64_C(1) << take) - 1U;
    out |= (uint32_t)(rng->buffer & mask) << written;
    rng->buffer >>= take;
    rng->available -= take;
    rng->bits_used += take;
    written += take;
  }
  *value = out;
  return PQSAMP_OK;
}

// reads one 32 bit word from the stream
int pqsamp_rng_word(pqsamp_rng *rng, uint32_t *value)
{
  return pqsamp_rng_bits(rng, 32U, value);
}

// reads up to 64 low bits from the stream
int pqsamp_rng_bits64(pqsamp_rng *rng, unsigned count, uint64_t *value)
{
  uint32_t low;
  uint32_t high = 0;
  int ret;

  if (rng == NULL || value == NULL || count > 64U)
  {
    return PQSAMP_ERR_PARAM;
  }
  ret = pqsamp_rng_bits(rng, count > 32U ? 32U : count, &low);
  if (ret != PQSAMP_OK)
  {
    return ret;
  }
  if (count > 32U)
  {
    ret = pqsamp_rng_bits(rng, count - 32U, &high);
    if (ret != PQSAMP_OK)
    {
      return ret;
    }
  }
  *value = (uint64_t)low | ((uint64_t)high << 32);
  return PQSAMP_OK;
}

// returns a short error message
const char *pqsamp_strerror(int error)
{
  switch (error)
  {
    case PQSAMP_OK:
      return "success";
    case PQSAMP_ERR_PARAM:
      return "invalid parameter";
    case PQSAMP_ERR_RANDOM:
      return "random source failed";
    case PQSAMP_ERR_BOUND:
      return "public sampling bound exceeded";
    default:
      return "unknown error";
  }
}
