# fast-secure-gaussian-sampler

Research implementation of the Maskaglia approach to discrete Gaussian
sampling for the HAWK post-quantum signature scheme.

This first milestone provides an unmasked C99 reference sampler, a deterministic
test RNG, a statistical distribution test, and a microbenchmark. It covers all
three HAWK dimensions (256, 512, and 1024) and both parity cosets.

## Status and security

This code is an early research baseline. `maskaglia_sample_ref()` is
variable-time, uses floating-point arithmetic, and returns an unmasked sample.
It is **not suitable for production or for protecting secrets**.

The masked, bitsliced, t-probing-secure gadget described in the paper is not
implemented yet. The current code establishes the API and distribution harness
against which that implementation can be developed.

## Reference sampler

For coset `c`, the desired output is an integer `x` in `2Z + c` with probability
proportional to

```text
exp(-x^2 / (2 tau^2)), where tau = 2 sigma_sign.
```

The reference sampler draws a magnitude index `k` from a geometric distribution
with parameter 1/2, sets the magnitude to `2k + c`, draws a sign, and performs
rejection sampling against a tight envelope. Rejecting the negative
representation of zero gives every integer in the even coset the same proposal
mass. This geometric/uniform decomposition is intentionally simple to inspect
before adding masking and bitslicing.

HAWK's signing standard deviations are:

| Parameter set | `sigma_sign` | Output `tau` |
|---:|---:|---:|
| 256 | 1.010 | 2.020 |
| 512 | 1.278 | 2.556 |
| 1024 | 1.299 | 2.598 |

## Build and run

```sh
make
make test
make bench
```

The distribution test samples all six parameter-set/coset combinations and
compares observed frequencies with the normalized target probabilities.

## API

```c
typedef void (*maskaglia_rng)(
    void *ctx,
    uint8_t *dst,
    size_t len
);

int maskaglia_sample_ref(
    maskaglia_rng rng,
    void *rng_ctx,
    unsigned parameter_set,
    unsigned coset,
    int32_t *out
);
```

Use `MASKAGLIA_HAWK_256`, `MASKAGLIA_HAWK_512`, or
`MASKAGLIA_HAWK_1024` for `parameter_set`.

## Research references

- C. Abou Haidar, T. Espitau, C. Hoffmann, and M. Tibouchi,
  [“Maskaglia: A New, Efficient Approach to Masked Discrete Gaussian
  Sampling”](https://eprint.iacr.org/2026/988), IACR ePrint 2026/988.
- [HAWK development repository](https://github.com/hawk-sign/dev), especially
  `src/hawk_sign.c` and `Reference_Implementation/tests/test_sampler.c`.
