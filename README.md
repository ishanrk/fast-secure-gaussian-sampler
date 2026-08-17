# fast-secure-gaussian-sampler - Maskaglia Implementation

## Purpose

This repository implements Maskaglia in portable C99 (and is my attempt to try and attempt a paper's algorithms from scratch)

Maskaglia is a rejection sampler for discrete Gaussian distributions. A
discrete Gaussian assigns each integer `x` a weight proportional to
`exp(-(x-c)^2 / (2 sigma^2))`. These distributions appear in lattice based
cryptography. 

Important: As you may have guessed, sampling this distribution is not that hard, however, protecting it from power analysis attacks
or side channel attacks is hard and this paper attempts to do that.

<img width="766" height="541" alt="image" src="https://github.com/user-attachments/assets/a2cf9227-57e3-4242-94f7-debc7233df19" />
                  *A discrete gaussian distribution*

The [Maskaglia paper](https://eprint.iacr.org/2026/988) replaces a large
cumulative distribution table with a discrete Laplace proposal and a rejection
test. The proposal and the test use uniform bits and geometric random values.
The construction comes from a discrete form of a normal sampler related to
Marsaglia. Its bit operations can be bitsliced and Boolean masked (to protect from power analysis)

[![Continuous and discrete Gaussian rejection templates from the Maskaglia paper](https://raw.githubusercontent.com/ishanrk/fast-secure-gaussian-sampler/main/docs/assets/maskaglia-figure-1.png)](https://eprint.iacr.org/2026/988)

*Continuous vs Discrete Algorithms from the Maskaglia paper. Cropped from page 9. [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).*

This repository exists to reproduce the paper in a small C library and measure
its cost. It measures random bits and masked AND gates. Its API can be used in fixed
profile experiments for post quantum cryptography.

## Quick Maskaglia Explanation

Each sample follows five steps.

1. The caller supplies random bytes. The library buffers those bytes and reads
   random bits from them.

2. Geometric random values and a side bit produce a discrete Laplace
   candidate around center `0` or center `1/2`.

3. A fixed profile gives the exact integer quotient and remainder for the
   candidate rejection exponent. The sampling path uses integer arithmetic.
   It does not call a floating point function.

4. A second geometric value handles the integer part of the rejection test. A
   bounded uniform value handles its fractional part. The candidate is then
   accepted or rejected.

5. The scalar path returns the accepted integer. The masked path processes 32
   candidates at once. It keeps each bit as one to four Boolean shares. Only
   refreshed validity and rejection bits become public.

[![General rejection sampling diagram with acceptance and rejection zones](https://raw.githubusercontent.com/ishanrk/fast-secure-gaussian-sampler/main/docs/assets/rejection-sampling.png)](https://commons.wikimedia.org/wiki/File:Rejection-sampling.svg)

*General rejection sampling diagram by Mantheflan. [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).*


## Scope

The public API supports two fixed widths.

1. `s = 3/2` gives `sigma = 1.2739827004320286`.

2. `s = 1521/1000` gives `sigma = 1.2918184582380770`.

Each width supports center `0` and center `1/2`. Each compiled distribution has
support from `-13` through `13`. The finite support and integer boundary counts
approximate the ideal Gaussian. They do not make it exact.

These widths are not the official HAWK widths. The HAWK adapter shows how a
masked two center consumer can call the sampler and map `z` to `2z-t`. It does
not provide HAWK known answer compatibility. It does not implement signing.
The [HAWK v1.1 specification](https://hawk-sign.info/hawk-spec.pdf) defines the
scheme boundary used by the adapter.

New profiles require generated tables and an independent error certificate.
The library does not accept unchecked tables at runtime.

## Code in the repository

1. [include/pqsamp.h](include/pqsamp.h) defines the public C API.

2. [src/scalar.c](src/scalar.c) contains the readable scalar sampler.

3. [src/maskaglia.c](src/maskaglia.c) contains the masked candidate path.

4. [src/gadgets.c](src/gadgets.c) contains the Boolean masking gadgets.

5. [src/masked.c](src/masked.c) contains the 32 lane scheduler.

6. [src/profiles.c](src/profiles.c) contains the fixed private tables.

7. [adapters/hawk](adapters/hawk) contains the two center research adapter.

8. [tools/profile_oracle.c](tools/profile_oracle.c) checks the profile tables
   with MPFR.

9. [examples/sample.c](examples/sample.c) is a complete runnable example.

## Build

The default build creates `build/libpqsamp.a`.

```sh
make
make test
make demo
make bench
```

The adapter is a separate library.

```sh
make adapter
```

The full validation targets are shown below. The oracle needs MPFR and GMP.
The remaining targets need the tools named by their command.

```sh
make oracle
ASAN_OPTIONS=detect_leaks=0 make sanitize
make check-clang
make valgrind
make fuzz
make cbmc
make cross-m4
make cross-rv32
make assembly
make stack
```

## Use

The application owns every output buffer and every random source. The random
callback must fill the complete request or return failure. The library has no
heap allocation and no hidden random source.

```c
pqsamp_rng rng;
int16_t out[32];
int rc;

rc = pqsamp_rng_init(&rng, randombytes, context);
if (rc == PQSAMP_OK)
{
  rc = pqsamp_sample(out, 32, PQSAMP_PROFILE_S3_2,
                     PQSAMP_CENTER_ZERO, &rng, NULL);
}
```

`pqsamp_sample_masked` returns Boolean shared samples. Two independent random
streams are required when the share count is greater than one. One stream
creates sampler coins. The other creates masks and gadget randomness. A masked
consumer should keep every share and should not reconstruct the sample.

Run `make demo` for a complete scalar and masked example.

## Measured result

The benchmark uses 32768 samples from the `s = 3/2` profile. The current
portable masked path uses `6.206359863` secure AND calls per zero center sample.
It uses `5.590454102` per half center sample. These are operation counts.

 Run `make bench` to reproduce this repository's counters.

The profile oracle records candidate acceptance rates from about `0.72` to
`0.77`. It also checks the finite boundary counts and reports the estimated
Rényi error. The checked values are stored in
[results/profiles.json](results/profiles.json).

## Security 

Boolean masking splits a sensitive value into shares whose XOR is the value.
The source follows the paper's masking structure. Secure AND uses fresh pair
randomness based on the PINI construction of
[Cassiers and Standaert](https://doi.org/10.1109/TIFS.2020.2971153). Refresh
before recombination follows the structure used by
[Azouaoui et al](https://doi.org/10.46586/tches.v2023.i4.58-79).

This does not prove that compiled code is secure against side channels. There is no proof that GCC or Clang preserves
the source masking argument, and showing that this not the goal of my code.

This library is research code.

## References

1. Calvin Abou Haidar. Thomas Espitau. Clément Hoffmann. Mehdi Tibouchi.
   [Maskaglia: A New, Efficient Approach to Masked Discrete Gaussian
   Sampling](https://eprint.iacr.org/2026/988). CRYPTO 2026.

2. Gaëtan Cassiers. François-Xavier Standaert.
   [Trivially and Efficiently Composing Masked Gadgets With Probe Isolating
   Non-Interference](https://doi.org/10.1109/TIFS.2020.2971153). IEEE TIFS
   2020.

3. Olivier Bronchain. Gaëtan Cassiers.
   [Bitslicing Arithmetic and Boolean Masking Conversions for Fun and
   Profit](https://eprint.iacr.org/2022/158). TCHES 2022.

4. The HAWK team. [HAWK v1.1](https://hawk-sign.info/hawk-spec.pdf). 2025.

5. The complete bibliography and the exact role of each source are in
   [docs/sources.md](docs/sources.md) and [docs/refs.bib](docs/refs.bib).

## License

This project is released under the [MIT License](LICENSE).
