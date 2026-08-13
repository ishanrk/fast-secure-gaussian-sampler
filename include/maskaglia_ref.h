#ifndef MASKAGLIA_REF_H
#define MASKAGLIA_REF_H

#include "maskaglia.h"

#ifdef __cplusplus
extern "C" {
#endif

/* rational sampler parameter s = sn / sd */
typedef struct {
	uint32_t sn;
	uint32_t sd;
	uint32_t gmax;  /* longest accepted geometric zero run */
	uint32_t tries; /* public proposal/rejection attempt bound */
} mg_ref_params;

typedef struct {
	uint64_t num;
	uint64_t den;
} mg_rat;

int mg_ref_params_init(mg_ref_params *p, uint32_t sn, uint32_t sd,
	uint32_t gmax, uint32_t tries);
long double mg_ref_sigma(const mg_ref_params *p);

/* proposal recipe for q_c(z) proportional to 2^(-abs(z-c)), c = t/2;
 * returns MG_EBOUND instead of clipping or exceeding the public bounds */
int mg_ref_dl(int32_t *z, unsigned t, const mg_ref_params *p, mg_rng *r);

/* exact rational rejection exponent */
int mg_ref_exp(mg_rat *e, int32_t z, unsigned t,
	const mg_ref_params *p);

/* diagnostic only: libm makes this approximate and variable-time */
int mg_ref_sample_fp(int32_t *z, unsigned t, const mg_ref_params *p,
	mg_rng *r);
int mg_ref_fill_fp(int32_t *z, size_t n, unsigned t,
	const mg_ref_params *p, mg_rng *r);

/* map z centered at t/2 to hawk's centered coset 2Z+t */
int mg_ref_hawk_map(int32_t *x, int32_t z, unsigned t);

#ifdef __cplusplus
}
#endif

#endif
