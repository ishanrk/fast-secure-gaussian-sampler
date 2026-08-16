#include "internal.h"

static const pqsamp_candidate s3_2_c0_side0[14] = {
    {0, 0, 1, UINT64_C(12464258977960)},
    {-1, 0, 1, UINT64_C(34846464499747)},
    {-2, 0, 1, UINT64_C(20399139746629)},
    {-3, 1, 1, UINT64_C(12464258977960)},
    {-4, 3, 1, UINT64_C(8932290491017)},
    {-5, 6, 1, UINT64_C(8932290491017)},
    {-6, 10, 1, UINT64_C(12464258977960)},
    {-7, 15, 1, UINT64_C(20399139746629)},
    {-8, 21, 1, UINT64_C(34846464499747)},
    {-9, 27, 1, UINT64_C(12464258977960)},
    {-10, 35, 1, UINT64_C(34846464499747)},
    {-11, 43, 1, UINT64_C(20399139746629)},
    {-12, 52, 1, UINT64_C(12464258977960)},
    {-13, 62, 1, UINT64_C(8932290491017)}};

static const pqsamp_candidate s3_2_c0_side1[14] = {
    {1, 0, 1, UINT64_C(34846464499747)},
    {2, 0, 1, UINT64_C(20399139746629)},
    {3, 1, 1, UINT64_C(12464258977960)},
    {4, 3, 1, UINT64_C(8932290491017)},
    {5, 6, 1, UINT64_C(8932290491017)},
    {6, 10, 1, UINT64_C(12464258977960)},
    {7, 15, 1, UINT64_C(20399139746629)},
    {8, 21, 1, UINT64_C(34846464499747)},
    {9, 27, 1, UINT64_C(12464258977960)},
    {10, 35, 1, UINT64_C(34846464499747)},
    {11, 43, 1, UINT64_C(20399139746629)},
    {12, 52, 1, UINT64_C(12464258977960)},
    {13, 62, 1, UINT64_C(8932290491017)},
    {14, 73, 0, UINT64_C(8932290491017)}};

static const pqsamp_candidate s3_2_ch_side0[14] = {
    {0, 0, 1, UINT64_C(6801502614557)},
    {-1, 0, 1, UINT64_C(8050242048584)},
    {-2, 0, 1, UINT64_C(1029776513046)},
    {-3, 2, 1, UINT64_C(3583726838779)},
    {-4, 5, 1, UINT64_C(8050242048584)},
    {-5, 8, 1, UINT64_C(3583726838779)},
    {-6, 12, 1, UINT64_C(1029776513046)},
    {-7, 18, 1, UINT64_C(8050242048584)},
    {-8, 24, 1, UINT64_C(6801502614557)},
    {-9, 31, 1, UINT64_C(6801502614557)},
    {-10, 39, 1, UINT64_C(8050242048584)},
    {-11, 47, 1, UINT64_C(1029776513046)},
    {-12, 57, 1, UINT64_C(3583726838779)},
    {-13, 68, 1, UINT64_C(8050242048584)}};

static const pqsamp_candidate s3_2_ch_side1[14] = {
    {1, 0, 1, UINT64_C(6801502614557)},   {2, 0, 1, UINT64_C(8050242048584)},
    {3, 0, 1, UINT64_C(1029776513046)},   {4, 2, 1, UINT64_C(3583726838779)},
    {5, 5, 1, UINT64_C(8050242048584)},   {6, 8, 1, UINT64_C(3583726838779)},
    {7, 12, 1, UINT64_C(1029776513046)},  {8, 18, 1, UINT64_C(8050242048584)},
    {9, 24, 1, UINT64_C(6801502614557)},  {10, 31, 1, UINT64_C(6801502614557)},
    {11, 39, 1, UINT64_C(8050242048584)}, {12, 47, 1, UINT64_C(1029776513046)},
    {13, 57, 1, UINT64_C(3583726838779)}, {14, 68, 0, UINT64_C(8050242048584)}};

static const pqsamp_candidate s1521_c0_side0[14] = {
    {0, 0, 1, UINT64_C(11943303152018)},
    {-1, 0, 1, UINT64_C(34668429421479)},
    {-2, 0, 1, UINT64_C(21680919705580)},
    {-3, 1, 1, UINT64_C(15666258502423)},
    {-4, 3, 1, UINT64_C(14765115959841)},
    {-5, 6, 1, UINT64_C(18710986678701)},
    {-6, 10, 1, UINT64_C(28694511986844)},
    {-7, 14, 1, UINT64_C(6398923553968)},
    {-8, 20, 1, UINT64_C(24285378123775)},
    {-9, 26, 1, UINT64_C(11527680870545)},
    {-10, 33, 1, UINT64_C(5119560371512)},
    {-11, 41, 1, UINT64_C(3014586030400)},
    {-12, 50, 1, UINT64_C(4584313795615)},
    {-13, 60, 1, UINT64_C(10295223127818)}};

static const pqsamp_candidate s1521_c0_side1[14] = {
    {1, 0, 1, UINT64_C(34668429421479)},
    {2, 0, 1, UINT64_C(21680919705580)},
    {3, 1, 1, UINT64_C(15666258502423)},
    {4, 3, 1, UINT64_C(14765115959841)},
    {5, 6, 1, UINT64_C(18710986678701)},
    {6, 10, 1, UINT64_C(28694511986844)},
    {7, 14, 1, UINT64_C(6398923553968)},
    {8, 20, 1, UINT64_C(24285378123775)},
    {9, 26, 1, UINT64_C(11527680870545)},
    {10, 33, 1, UINT64_C(5119560371512)},
    {11, 41, 1, UINT64_C(3014586030400)},
    {12, 50, 1, UINT64_C(4584313795615)},
    {13, 60, 1, UINT64_C(10295223127818)},
    {14, 71, 0, UINT64_C(21947383346127)}};

static const pqsamp_candidate s1521_ch_side0[14] = {
    {0, 0, 1, UINT64_C(6663586168507)},
    {-1, 0, 1, UINT64_C(8185800167027)},
    {-2, 0, 1, UINT64_C(1449270307217)},
    {-3, 2, 1, UINT64_C(4783421592168)},
    {-4, 4, 1, UINT64_C(1089364201182)},
    {-5, 8, 1, UINT64_C(7013653198539)},
    {-6, 12, 1, UINT64_C(5090914412147)},
    {-7, 17, 1, UINT64_C(4603082149935)},
    {-8, 23, 1, UINT64_C(5405370060845)},
    {-9, 30, 1, UINT64_C(7737747375054)},
    {-10, 37, 1, UINT64_C(1776220281917)},
    {-11, 46, 1, UINT64_C(6055804076220)},
    {-12, 55, 1, UINT64_C(2662979558870)},
    {-13, 65, 1, UINT64_C(915771967925)}};

static const pqsamp_candidate s1521_ch_side1[14] = {
    {1, 0, 1, UINT64_C(6663586168507)},   {2, 0, 1, UINT64_C(8185800167027)},
    {3, 0, 1, UINT64_C(1449270307217)},   {4, 2, 1, UINT64_C(4783421592168)},
    {5, 4, 1, UINT64_C(1089364201182)},   {6, 8, 1, UINT64_C(7013653198539)},
    {7, 12, 1, UINT64_C(5090914412147)},  {8, 17, 1, UINT64_C(4603082149935)},
    {9, 23, 1, UINT64_C(5405370060845)},  {10, 30, 1, UINT64_C(7737747375054)},
    {11, 37, 1, UINT64_C(1776220281917)}, {12, 46, 1, UINT64_C(6055804076220)},
    {13, 55, 1, UINT64_C(2662979558870)}, {14, 65, 0, UINT64_C(915771967925)}};

static const pqsamp_params s3_2_c0 = {.s_num = 3,
                                      .s_den = 2,
                                      .center_num = 0,
                                      .center_den = 1,
                                      .side_num = 1,
                                      .side_den = 3,
                                      .geom_count = 14,
                                      .k_sat = 63,
                                      .threshold_bits = 45,
                                      .side = {s3_2_c0_side0, s3_2_c0_side1}};
static const pqsamp_params s3_2_ch = {.s_num = 3,
                                      .s_den = 2,
                                      .center_num = 1,
                                      .center_den = 2,
                                      .side_num = 1,
                                      .side_den = 2,
                                      .geom_count = 14,
                                      .k_sat = 69,
                                      .threshold_bits = 43,
                                      .side = {s3_2_ch_side0, s3_2_ch_side1}};
static const pqsamp_params s1521_c0 = {
    .s_num = 1521,
    .s_den = 1000,
    .center_num = 0,
    .center_den = 1,
    .side_num = 1,
    .side_den = 3,
    .geom_count = 14,
    .k_sat = 61,
    .threshold_bits = 45,
    .side = {s1521_c0_side0, s1521_c0_side1}};
static const pqsamp_params s1521_ch = {
    .s_num = 1521,
    .s_den = 1000,
    .center_num = 1,
    .center_den = 2,
    .side_num = 1,
    .side_den = 2,
    .geom_count = 14,
    .k_sat = 66,
    .threshold_bits = 43,
    .side = {s1521_ch_side0, s1521_ch_side1}};

const pqsamp_params *pqsamp_profile_get(pqsamp_profile profile,
                                        pqsamp_center center)
{
  if (profile == PQSAMP_PROFILE_S3_2)
  {
    return center == PQSAMP_CENTER_ZERO   ? &s3_2_c0
           : center == PQSAMP_CENTER_HALF ? &s3_2_ch
                                          : NULL;
  }
  if (profile == PQSAMP_PROFILE_S1521_1000)
  {
    return center == PQSAMP_CENTER_ZERO   ? &s1521_c0
           : center == PQSAMP_CENTER_HALF ? &s1521_ch
                                          : NULL;
  }
  return NULL;
}

int pqsamp_profile_check(const pqsamp_params *params)
{
  unsigned side;
  unsigned i;
  uint64_t limit;

  if (params == NULL || params->s_num == 0U || params->s_den == 0U ||
      params->center_den == 0U || params->side_num == 0U ||
      params->side_num >= params->side_den || params->geom_count < 2U ||
      params->geom_count > PQSAMP_GEOM_BITS || params->k_sat < 2U ||
      params->k_sat > PQSAMP_K_SAT_MAX || params->threshold_bits == 0U ||
      params->threshold_bits > PQSAMP_THRESHOLD_BITS ||
      params->side[0] == NULL || params->side[1] == NULL)
  {
    return PQSAMP_ERR_PARAM;
  }
  limit = UINT64_C(1) << params->threshold_bits;
  for (side = 0; side < 2U; side++)
  {
    for (i = 0; i < params->geom_count; i++)
    {
      const pqsamp_candidate *candidate = &params->side[side][i];

      if (candidate->valid > 1U ||
          (candidate->valid != 0U && candidate->quotient >= params->k_sat) ||
          candidate->boundary_count > limit)
      {
        return PQSAMP_ERR_PARAM;
      }
    }
  }
  return PQSAMP_OK;
}

unsigned pqsamp_bit_width_u32(uint32_t value)
{
  unsigned bits = 0;

  do
  {
    bits++;
    value >>= 1;
  } while (value != 0U);
  return bits;
}
