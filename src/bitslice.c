#include "internal.h"

static void transpose32(uint32_t value[PQSAMP_LANES])
{
  uint32_t mask = UINT32_C(0x0000ffff);
  unsigned width = 16U;

  while (width != 0U)
  {
    unsigned i = 0U;

    while (i < PQSAMP_LANES)
    {
      uint32_t swap = (value[i] ^ (value[i + width] >> width)) & mask;

      value[i] ^= swap;
      value[i + width] ^= swap << width;
      i = (i + width + 1U) & ~width;
    }
    width >>= 1;
    if (width != 0U)
    {
      mask ^= mask << width;
    }
  }
}

void pqsamp_pack16(pqsamp_word out[PQSAMP_VALUE_BITS],
                   const pqsamp_masked_i16 in[PQSAMP_LANES], unsigned shares)
{
  unsigned share;
  unsigned bit;

  for (bit = 0; bit < PQSAMP_VALUE_BITS; bit++)
  {
    for (share = 0; share < PQSAMP_MAX_SHARES; share++)
    {
      out[bit].share[share] = 0;
    }
  }
  for (share = 0; share < shares; share++)
  {
    uint32_t value[PQSAMP_LANES];
    unsigned lane;

    for (lane = 0; lane < PQSAMP_LANES; lane++)
    {
      value[PQSAMP_LANES - 1U - lane] = in[lane].share[share];
    }
    transpose32(value);
    for (bit = 0; bit < PQSAMP_VALUE_BITS; bit++)
    {
      out[bit].share[share] = value[PQSAMP_LANES - 1U - bit];
    }
  }
}

void pqsamp_unpack16(pqsamp_masked_i16 out[PQSAMP_LANES],
                     const pqsamp_word in[PQSAMP_VALUE_BITS], unsigned shares)
{
  unsigned lane;
  unsigned share;

  for (lane = 0; lane < PQSAMP_LANES; lane++)
  {
    for (share = 0; share < PQSAMP_MAX_SHARES; share++)
    {
      out[lane].share[share] = 0;
    }
  }
  for (share = 0; share < shares; share++)
  {
    uint32_t value[PQSAMP_LANES] = {0};
    unsigned bit;

    for (bit = 0; bit < PQSAMP_VALUE_BITS; bit++)
    {
      value[PQSAMP_LANES - 1U - bit] = in[bit].share[share];
    }
    transpose32(value);
    for (lane = 0; lane < PQSAMP_LANES; lane++)
    {
      out[lane].share[share] = (uint16_t)value[PQSAMP_LANES - 1U - lane];
    }
  }
}
