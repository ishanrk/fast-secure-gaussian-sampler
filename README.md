# Implementing the Side Channel Resistant Discrete Sampler Maskaglia

This repository is a small C++ testbed for discrete Gaussian sampling. It
currently contains scalar CDT and Knuth–Yao reference samplers. The Maskaglia
entry point is present but is not implemented yet.

## Status and security

The CDT and Knuth–Yao code is unmasked reference code for distribution and
performance comparisons. It is not a side-channel-resistant or production
implementation. `maskaglia_sample_ref` currently returns an error without
sampling.

## Parameters and API

All three entry points use the callback type in `include/maskaglia.hpp`. A random
callback returns zero after filling the requested output, and a sampler returns
zero after writing one sample. Invalid inputs and random-source failures return
`-1`.

Parameter sets `256`, `512`, and `1024` select Gaussian widths `1.010`, `1.278`,
and `1.299`. The returned integer has standard deviation twice that width and
lies in `2Z + coset`. Coset `0` produces even integers and coset `1` produces
odd integers.

## CDT reference

`cdt_sample_ref` compares 78 random bits with cumulative tail thresholds and
uses a separate random sign bit. Its tables come from the HAWK signing sampler.
The complete table is scanned for each sample.

## Knuth–Yao reference

`knuth_yao_sample_ref` walks a 64-column discrete distribution generating tree.
Its probability rows are generated from the same HAWK Gaussian parameters and
sum exactly to `2^64` for each parameter-set and coset pair.

## Random source

The test and benchmark programs use a deterministic SplitMix64 stream. It is
not a cryptographic random source. `src/rng_shake.cpp` is still a placeholder.

## Build and run

The Makefile builds the project as C++17.

```sh
make
./bin/test_distribution
./bin/bench_sampler
```

The distribution test draws one million samples from every parameter-set and
coset pair for both implemented samplers. It checks parity, histogram error,
mean, variance, invalid inputs, and random-source failures. The benchmark also
draws one million samples per pair and includes random callback cost.

## Research references

C. Abou Haidar, T. Espitau, C. Hoffmann, and M. Tibouchi,
[“Maskaglia: A New, Efficient Approach to Masked Discrete Gaussian
Sampling”](https://eprint.iacr.org/2026/988), IACR ePrint 2026/988.

The [HAWK development repository](https://github.com/hawk-sign/dev), especially
`src/hawk_sign.c` and `Reference_Implementation/tests/test_sampler.c`, provides
the signing parameters and CDT thresholds used by the reference code.

D. E. Knuth and A. C. Yao, “The Complexity of Nonuniform Random Number
Generation,” in *Algorithms and Complexity: New Directions and Recent
Results*, 1976.
