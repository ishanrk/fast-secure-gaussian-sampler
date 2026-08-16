#include <inttypes.h>
#include <mpfr.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"

#define PQSAMP_ORACLE_PREC 512U
#define PQSAMP_ORACLE_ALPHA 256U
#define PQSAMP_ORACLE_MIN_Y (-13)
#define PQSAMP_ORACLE_MAX_Y 13
#define PQSAMP_ORACLE_SUPPORT 27U
#define PQSAMP_ORACLE_IDEAL_BOUND 1024

typedef struct
{
  const char *acceptance;
  const char *failure;
  const char *renyi;
} oracle_expected;

static const oracle_expected expected_results[2][2] = {
    {{"0.7207801827980715", "-94.3657", "87.0105"},
     {"0.7645028325020456", "-115.6042", "82.5025"}},
    {{"0.7228802869050787", "-95.2942", "85.3386"},
     {"0.7667303292849444", "-116.8059", "80.3455"}}};

// finds the greatest common divisor
static uint64_t gcd_u64(uint64_t left, uint64_t right)
{
  while (right != 0U)
  {
    uint64_t next = left % right;

    left = right;
    right = next;
  }
  return left;
}

// multiplies two words and reports overflow
static int mul_checked(uint64_t left, uint64_t right, uint64_t *out)
{
  if (left != 0U && right > UINT64_MAX / left)
  {
    return -1;
  }
  *out = left * right;
  return 0;
}

// copies one integer into an mpfr value
static int set_u64(mpfr_t out, uint64_t value)
{
  char text[32];

  if (snprintf(text, sizeof(text), "%" PRIu64, value) < 0)
  {
    return -1;
  }
  return mpfr_set_str(out, text, 10, MPFR_RNDN);
}

// returns the integer floor of an mpfr value
static int get_floor(const mpfr_t value, uint64_t *out)
{
  char text[32];

  if (mpfr_snprintf(text, sizeof(text), "%.0RDf", value) < 0)
  {
    return -1;
  }
  *out = (uint64_t)strtoull(text, NULL, 10);
  return 0;
}

// recomputes one exact boundary count
static int boundary_count(uint64_t residue, uint64_t k0, unsigned bits,
                          uint64_t *out)
{
  mpfr_t r;
  mpfr_t modulus;
  mpfr_t lower;
  mpfr_t upper;
  uint64_t floor_lower;
  uint64_t floor_upper;
  int ret = -1;

  mpfr_init2(r, PQSAMP_ORACLE_PREC);
  mpfr_init2(modulus, PQSAMP_ORACLE_PREC);
  mpfr_init2(lower, PQSAMP_ORACLE_PREC);
  mpfr_init2(upper, PQSAMP_ORACLE_PREC);
  if (set_u64(r, residue) != 0 || set_u64(modulus, k0) != 0)
  {
    goto cleanup;
  }

  mpfr_div(lower, r, modulus, MPFR_RNDU);
  mpfr_ui_sub(lower, 1U, lower, MPFR_RNDD);
  mpfr_exp2(lower, lower, MPFR_RNDD);
  mpfr_sub_ui(lower, lower, 1U, MPFR_RNDD);
  mpfr_mul_2ui(lower, lower, bits, MPFR_RNDD);

  mpfr_div(upper, r, modulus, MPFR_RNDD);
  mpfr_ui_sub(upper, 1U, upper, MPFR_RNDU);
  mpfr_exp2(upper, upper, MPFR_RNDU);
  mpfr_sub_ui(upper, upper, 1U, MPFR_RNDU);
  mpfr_mul_2ui(upper, upper, bits, MPFR_RNDU);

  if (get_floor(lower, &floor_lower) == 0 &&
      get_floor(upper, &floor_upper) == 0 && floor_lower == floor_upper)
  {
    *out = floor_lower;
    ret = 0;
  }

cleanup:
  mpfr_clear(upper);
  mpfr_clear(lower);
  mpfr_clear(modulus);
  mpfr_clear(r);
  return ret;
}

// returns the integer below the profile center
static int64_t center_floor(const pqsamp_params *params)
{
  int64_t numerator = params->center_num;
  int64_t denominator = params->center_den;
  int64_t result = numerator / denominator;

  if (numerator < 0 && numerator % denominator != 0)
  {
    result--;
  }
  return result;
}

// computes the accepted mass for one table row
static int candidate_mass(mpfr_t out, const pqsamp_candidate *entry,
                          const pqsamp_params *params, unsigned side,
                          unsigned geometric)
{
  mpfr_t side_probability;
  mpfr_t acceptance;
  uint32_t side_weight =
      side == 0U ? params->side_den - params->side_num : params->side_num;
  int ret = -1;

  mpfr_init2(side_probability, PQSAMP_ORACLE_PREC);
  mpfr_init2(acceptance, PQSAMP_ORACLE_PREC);
  mpfr_set_ui(side_probability, side_weight, MPFR_RNDN);
  mpfr_div_ui(side_probability, side_probability, params->side_den, MPFR_RNDN);
  mpfr_div_2ui(side_probability, side_probability, geometric + 1U, MPFR_RNDN);

  if (entry->valid == 0U)
  {
    mpfr_set_zero(out, 0);
    ret = 0;
    goto cleanup;
  }
  if (set_u64(acceptance, entry->boundary_count) != 0)
  {
    goto cleanup;
  }
  mpfr_div_2ui(acceptance, acceptance, params->threshold_bits, MPFR_RNDN);
  mpfr_add_ui(acceptance, acceptance, 1U, MPFR_RNDN);
  mpfr_div_2ui(acceptance, acceptance, entry->quotient + 1U, MPFR_RNDN);
  mpfr_mul(out, side_probability, acceptance, MPFR_RNDN);
  ret = 0;

cleanup:
  mpfr_clear(acceptance);
  mpfr_clear(side_probability);
  return ret;
}

// computes the chance that one fixed batch cap fails
static void block_failure(mpfr_t out, const mpfr_t acceptance)
{
  mpfr_t rejected;
  mpfr_t term;
  mpfr_t sum;
  unsigned k;

  mpfr_init2(rejected, PQSAMP_ORACLE_PREC);
  mpfr_init2(term, PQSAMP_ORACLE_PREC);
  mpfr_init2(sum, PQSAMP_ORACLE_PREC);
  mpfr_ui_sub(rejected, 1U, acceptance, MPFR_RNDN);
  mpfr_pow_ui(term, rejected, PQSAMP_LANES * PQSAMP_BATCHES_PER_BLOCK,
              MPFR_RNDN);
  mpfr_set(sum, term, MPFR_RNDN);
  for (k = 0; k < 31U; k++)
  {
    mpfr_mul(term, term, acceptance, MPFR_RNDN);
    mpfr_div(term, term, rejected, MPFR_RNDN);
    mpfr_mul_ui(term, term, PQSAMP_LANES * PQSAMP_BATCHES_PER_BLOCK - k,
                MPFR_RNDN);
    mpfr_div_ui(term, term, k + 1U, MPFR_RNDN);
    mpfr_add(sum, sum, term, MPFR_RNDN);
  }
  mpfr_log2(out, sum, MPFR_RNDN);
  mpfr_clear(sum);
  mpfr_clear(term);
  mpfr_clear(rejected);
}

// computes one ideal gaussian weight
static void ideal_weight(mpfr_t out, const pqsamp_params *params, int y)
{
  int64_t distance = (int64_t)y * params->center_den - params->center_num;

  mpfr_set_si(out, (long)distance, MPFR_RNDN);
  mpfr_sqr(out, out, MPFR_RNDN);
  mpfr_mul_ui(out, out, params->s_den, MPFR_RNDN);
  mpfr_mul_ui(out, out, params->s_den, MPFR_RNDN);
  mpfr_div_ui(out, out, params->center_den, MPFR_RNDN);
  mpfr_div_ui(out, out, params->center_den, MPFR_RNDN);
  mpfr_div_ui(out, out, params->s_num, MPFR_RNDN);
  mpfr_div_ui(out, out, params->s_num, MPFR_RNDN);
  mpfr_neg(out, out, MPFR_RNDN);
  mpfr_exp2(out, out, MPFR_RNDN);
}

// computes the renyi gap from the ideal profile
static int renyi_bound(mpfr_t out, mpfr_t mass[PQSAMP_ORACLE_SUPPORT],
                       const mpfr_t total, const pqsamp_params *params)
{
  mpfr_t ideal_normalizer;
  mpfr_t ideal;
  mpfr_t actual;
  mpfr_t scratch;
  mpfr_t sum;
  mpfr_t terms[PQSAMP_ORACLE_SUPPORT];
  unsigned i;
  int y;
  int ret = -1;

  mpfr_init2(ideal_normalizer, PQSAMP_ORACLE_PREC);
  mpfr_init2(ideal, PQSAMP_ORACLE_PREC);
  mpfr_init2(actual, PQSAMP_ORACLE_PREC);
  mpfr_init2(scratch, PQSAMP_ORACLE_PREC);
  mpfr_init2(sum, PQSAMP_ORACLE_PREC);
  for (i = 0; i < PQSAMP_ORACLE_SUPPORT; i++)
  {
    mpfr_init2(terms[i], PQSAMP_ORACLE_PREC);
  }

  mpfr_set_zero(ideal_normalizer, 0);
  for (y = -PQSAMP_ORACLE_IDEAL_BOUND; y <= PQSAMP_ORACLE_IDEAL_BOUND; y++)
  {
    ideal_weight(ideal, params, y);
    mpfr_add(ideal_normalizer, ideal_normalizer, ideal, MPFR_RNDN);
  }
  for (i = 0; i < PQSAMP_ORACLE_SUPPORT; i++)
  {
    y = (int)i + PQSAMP_ORACLE_MIN_Y;
    if (mpfr_zero_p(mass[i]) != 0)
    {
      goto cleanup;
    }
    mpfr_div(actual, mass[i], total, MPFR_RNDN);
    ideal_weight(ideal, params, y);
    mpfr_div(ideal, ideal, ideal_normalizer, MPFR_RNDN);
    mpfr_log(terms[i], actual, MPFR_RNDN);
    mpfr_mul_ui(terms[i], terms[i], PQSAMP_ORACLE_ALPHA, MPFR_RNDN);
    mpfr_log(scratch, ideal, MPFR_RNDN);
    mpfr_mul_si(scratch, scratch, 1L - (long)PQSAMP_ORACLE_ALPHA, MPFR_RNDN);
    mpfr_add(terms[i], terms[i], scratch, MPFR_RNDN);
  }

  mpfr_set(scratch, terms[0], MPFR_RNDN);
  for (i = 1; i < PQSAMP_ORACLE_SUPPORT; i++)
  {
    if (mpfr_greater_p(terms[i], scratch) != 0)
    {
      mpfr_set(scratch, terms[i], MPFR_RNDN);
    }
  }
  mpfr_set_zero(sum, 0);
  for (i = 0; i < PQSAMP_ORACLE_SUPPORT; i++)
  {
    mpfr_sub(actual, terms[i], scratch, MPFR_RNDN);
    mpfr_exp(actual, actual, MPFR_RNDN);
    mpfr_add(sum, sum, actual, MPFR_RNDN);
  }
  mpfr_log(sum, sum, MPFR_RNDN);
  mpfr_add(sum, sum, scratch, MPFR_RNDN);
  mpfr_div_ui(sum, sum, PQSAMP_ORACLE_ALPHA - 1U, MPFR_RNDN);
  if (mpfr_sgn(sum) <= 0)
  {
    goto cleanup;
  }
  mpfr_log2(out, sum, MPFR_RNDN);
  mpfr_neg(out, out, MPFR_RNDN);
  ret = 0;

cleanup:
  for (i = 0; i < PQSAMP_ORACLE_SUPPORT; i++)
  {
    mpfr_clear(terms[i]);
  }
  mpfr_clear(sum);
  mpfr_clear(scratch);
  mpfr_clear(actual);
  mpfr_clear(ideal);
  mpfr_clear(ideal_normalizer);
  return ret;
}

// checks every metric for one compiled profile
static int metrics(const pqsamp_params *params, const oracle_expected *expected,
                   mpfr_t acceptance, mpfr_t failure, mpfr_t renyi)
{
  mpfr_t mass[PQSAMP_ORACLE_SUPPORT];
  mpfr_t candidate;
  char actual_acceptance[32];
  char actual_failure[32];
  char actual_renyi[32];
  unsigned i;
  unsigned side;
  int ret = -1;

  mpfr_init2(candidate, PQSAMP_ORACLE_PREC);
  for (i = 0; i < PQSAMP_ORACLE_SUPPORT; i++)
  {
    mpfr_init2(mass[i], PQSAMP_ORACLE_PREC);
    mpfr_set_zero(mass[i], 0);
  }
  mpfr_set_zero(acceptance, 0);
  for (side = 0; side < 2U; side++)
  {
    unsigned geometric;

    for (geometric = 0; geometric < params->geom_count; geometric++)
    {
      const pqsamp_candidate *entry = &params->side[side][geometric];

      if (candidate_mass(candidate, entry, params, side, geometric) != 0)
      {
        goto cleanup;
      }
      mpfr_add(acceptance, acceptance, candidate, MPFR_RNDN);
      if (entry->valid != 0U)
      {
        int index = entry->y - PQSAMP_ORACLE_MIN_Y;

        if (index < 0 || index >= (int)PQSAMP_ORACLE_SUPPORT)
        {
          goto cleanup;
        }
        mpfr_add(mass[(unsigned)index], mass[(unsigned)index], candidate,
                 MPFR_RNDN);
      }
    }
  }

  block_failure(failure, acceptance);
  if (renyi_bound(renyi, mass, acceptance, params) != 0 ||
      mpfr_snprintf(actual_acceptance, sizeof(actual_acceptance), "%.16RNf",
                    acceptance) < 0 ||
      mpfr_snprintf(actual_failure, sizeof(actual_failure), "%.4RNf", failure) <
          0 ||
      mpfr_snprintf(actual_renyi, sizeof(actual_renyi), "%.4RNf", renyi) < 0 ||
      strcmp(actual_acceptance, expected->acceptance) != 0 ||
      strcmp(actual_failure, expected->failure) != 0 ||
      strcmp(actual_renyi, expected->renyi) != 0)
  {
    fprintf(stderr, "profile metric mismatch: %s %s %s\n", actual_acceptance,
            actual_failure, actual_renyi);
    goto cleanup;
  }
  ret = 0;

cleanup:
  for (i = 0; i < PQSAMP_ORACLE_SUPPORT; i++)
  {
    mpfr_clear(mass[i]);
  }
  mpfr_clear(candidate);
  return ret;
}

// checks all rows and metrics for one profile center
static int check_profile(pqsamp_profile profile, pqsamp_center center)
{
  const pqsamp_params *params = pqsamp_profile_get(profile, center);
  const oracle_expected *expected =
      &expected_results[(unsigned)profile][(unsigned)center];
  uint64_t q_squared;
  uint64_t p_squared;
  uint64_t gcd_left;
  uint64_t gcd_right;
  uint64_t gcd;
  uint64_t base;
  uint64_t k0;
  int64_t floor_center;
  unsigned max_quotient = 0;
  unsigned side;
  mpfr_t acceptance;
  mpfr_t failure;
  mpfr_t renyi;
  int ret = -1;

  if (params == NULL || pqsamp_profile_check(params) != PQSAMP_OK ||
      mul_checked(params->s_den, params->s_den, &q_squared) != 0 ||
      mul_checked(params->s_num, params->s_num, &p_squared) != 0 ||
      mul_checked(2U, q_squared, &gcd_left) != 0 ||
      mul_checked(p_squared, params->center_den, &gcd_right) != 0)
  {
    return -1;
  }
  gcd = gcd_u64(gcd_left, gcd_right);
  if (mul_checked(2U, params->s_num, &base) != 0 ||
      mul_checked(base, params->s_den, &base) != 0 ||
      mul_checked(base, params->center_den, &base) != 0)
  {
    return -1;
  }
  base /= gcd;
  if (mul_checked(base, base, &k0) != 0)
  {
    return -1;
  }

  floor_center = center_floor(params);
  for (side = 0; side < 2U; side++)
  {
    unsigned geometric;

    for (geometric = 0; geometric < params->geom_count; geometric++)
    {
      const pqsamp_candidate *entry = &params->side[side][geometric];
      int64_t expected_y =
          side == 0U ? floor_center - geometric : floor_center + geometric + 1;
      int64_t distance =
          (int64_t)entry->y * params->center_den - params->center_num;
      uint64_t numerator;
      int64_t linear;
      uint64_t square;
      uint64_t threshold;
      uint64_t quotient;
      uint64_t residue;
      unsigned valid =
          entry->y >= PQSAMP_ORACLE_MIN_Y && entry->y <= PQSAMP_ORACLE_MAX_Y
              ? 1U
              : 0U;

      if (distance < 0)
      {
        distance = -distance;
      }
      if (mul_checked(2U, q_squared, &numerator) != 0 ||
          mul_checked(numerator, (uint64_t)distance, &numerator) != 0)
      {
        return -1;
      }
      linear = ((int64_t)numerator - (int64_t)gcd_right) / (int64_t)gcd;
      if (mul_checked((uint64_t)(linear < 0 ? -linear : linear),
                      (uint64_t)(linear < 0 ? -linear : linear), &square) != 0)
      {
        return -1;
      }
      quotient = square / k0;
      residue = square % k0;
      if (boundary_count(residue, k0, params->threshold_bits, &threshold) !=
              0 ||
          entry->y != expected_y || entry->valid != valid ||
          entry->quotient != quotient || entry->boundary_count != threshold)
      {
        fprintf(stderr, "profile %u/%u side %u row %u mismatch\n",
                (unsigned)profile, (unsigned)center, side, geometric);
        return -1;
      }
      if (valid != 0U && quotient > max_quotient)
      {
        max_quotient = (unsigned)quotient;
      }
    }
  }
  if (params->k_sat != max_quotient + 1U)
  {
    return -1;
  }

  mpfr_init2(acceptance, PQSAMP_ORACLE_PREC);
  mpfr_init2(failure, PQSAMP_ORACLE_PREC);
  mpfr_init2(renyi, PQSAMP_ORACLE_PREC);
  if (metrics(params, expected, acceptance, failure, renyi) == 0)
  {
    const char *profile_name =
        profile == PQSAMP_PROFILE_S3_2 ? "3/2" : "1521/1000";
    const char *center_name = center == PQSAMP_CENTER_ZERO ? "0" : "1/2";

    mpfr_printf("s=%s c=%s K0=%" PRIu64
                " Ksat=%u pU=%u raw_accept=%.16RNf"
                " block_failure_log2=%.4RNf renyi256_bits=%.4RNf: ok\n",
                profile_name, center_name, k0, params->k_sat,
                params->threshold_bits, acceptance, failure, renyi);
    ret = 0;
  }
  mpfr_clear(renyi);
  mpfr_clear(failure);
  mpfr_clear(acceptance);
  return ret;
}

// checks every compiled profile with mpfr
int main(void)
{
  unsigned profile;

  printf("built-in profile oracle: MPFR %u-bit, alpha=%u\n", PQSAMP_ORACLE_PREC,
         PQSAMP_ORACLE_ALPHA);
  for (profile = 0; profile < 2U; profile++)
  {
    unsigned center;

    for (center = 0; center < 2U; center++)
    {
      if (check_profile((pqsamp_profile)profile, (pqsamp_center)center) != 0)
      {
        mpfr_free_cache();
        return EXIT_FAILURE;
      }
    }
  }
  mpfr_free_cache();
  return EXIT_SUCCESS;
}
