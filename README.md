# fast-secure-gaussian-sampler

A fast and optimized C- implementation of the Maskaglia approach to discrete Gaussian
sampling used for Post Quantum Crypto Schemes


## Status and security



## Parameters

## Build and run

```sh
make
make test
make bench
```

The distribution test samples all six parameter-set/coset combinations and
compares observed frequencies with the normalized target probabilities.

## Research references

- C. Abou Haidar, T. Espitau, C. Hoffmann, and M. Tibouchi,
  [“Maskaglia: A New, Efficient Approach to Masked Discrete Gaussian
  Sampling”](https://eprint.iacr.org/2026/988), IACR ePrint 2026/988.
- [HAWK development repository](https://github.com/hawk-sign/dev), especially
  `src/hawk_sign.c` and `Reference_Implementation/tests/test_sampler.c`.
