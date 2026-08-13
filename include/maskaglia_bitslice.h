#ifndef MASKAGLIA_BITSLICE_H
#define MASKAGLIA_BITSLICE_H

#include "maskaglia.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MG_LANES 32
#define MG_PLANES16 16

/* bit b of lane i becomes bit i of plane b */
void mg_pack16(uint32_t out[MG_PLANES16],
	const uint16_t in[MG_LANES]);
void mg_unpack16(uint16_t out[MG_LANES],
	const uint32_t in[MG_PLANES16]);

#ifdef __cplusplus
}
#endif

#endif
