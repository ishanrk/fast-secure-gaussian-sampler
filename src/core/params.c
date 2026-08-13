#include "maskaglia_ref.h"

#include <math.h>

#include "../internal/common.h"

int
mg_ref_params_init(mg_ref_params *p, uint32_t sn, uint32_t sd,
	uint32_t gmax, uint32_t tries)
{
	uint64_t sn2;
	uint64_t sd2;
	uint64_t den;

	if (p == NULL || sn == 0 || sd == 0 || tries == 0) {
		return MG_EINVAL;
	}
	if (!mg_mul64(sn, sn, &sn2) || !mg_mul64(sd, sd, &sd2)
		|| !mg_mul64(sn2, sd2, &den) || !mg_mul64(den, 4, &den)) {
		return MG_EOVERFLOW;
	}
	p->sn = sn;
	p->sd = sd;
	p->gmax = gmax;
	p->tries = tries;
	return MG_OK;
}

long double
mg_ref_sigma(const mg_ref_params *p)
{
	long double s;

	if (p == NULL || p->sn == 0 || p->sd == 0) {
		return 0.0L;
	}
	s = (long double)p->sn / (long double)p->sd;
	return s / sqrtl(2.0L * logl(2.0L));
}
