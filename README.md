# fast-secure-gaussian-sampler

## What this is

A small portable C99 research library for fixed-profile discrete-Gaussian
sampling with the Maskaglia transform. It contains a readable scalar sampler,
a 32-lane Boolean-masked sampler, and a masked two-center research adapter.

## Implemented status

- two built-in rational widths and centers `0` and `1/2`
- integer-only runtime sampling with no heap or hidden RNG
- 32 bitsliced lanes with one through four Boolean shares
- CS20-style secure AND, masked comparison, equality, Bernoulli, and proposal
- separate caller-owned streams for sample coins and masking randomness
- two-stage early rejection with pooled public survivor compaction
- a finite cap of four candidate batches per requested 32-output block
- complete output clearing on RNG, parameter, or public bound failure
- MPFR profile oracle, three focused test programs, fuzzing, and CBMC packing
- a separate `libpqsamp_hawk.a` adapter for masked selection and `x=2y-t`

Profile tables and candidate records are private. The public API accepts only
the four compiled profile and center combinations.

## Security status

The source follows the paper's Boolean masking and refresh structure. This is
not proof that a compiler or physical target preserves PINI. Optimized host
assembly spills shares and reuses registers, so the project makes no generic
side-channel-security or production claim. See [docs/security.md](docs/security.md).

The HAWK-shaped code is a masked two-center research adapter, not a complete
HAWK implementation and not a HAWK known-answer-test compatible sampler.

## Build and validation

```sh
make                    # build/libpqsamp.a
make adapter            # build/libpqsamp_hawk.a
make test
make bench
make oracle             # requires MPFR and GMP
ASAN_OPTIONS=detect_leaks=0 make sanitize
make check-clang
make assembly
make stack
make valgrind
make fuzz
make cbmc
make cross-m4
make cross-rv32
make demo
```

## API example

```c
pqsamp_rng coins;
pqsamp_rng masks;
int16_t plain[32];
pqsamp_masked_i16 shared[32];

if (pqsamp_rng_init(&coins, randombytes, coin_context) != PQSAMP_OK ||
    pqsamp_rng_init(&masks, randombytes, mask_context) != PQSAMP_OK ||
    pqsamp_sample(plain, 32, PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_ZERO,
                  &coins, NULL) != PQSAMP_OK ||
    pqsamp_sample_masked(shared, 32, PQSAMP_PROFILE_S3_2,
                         PQSAMP_CENTER_HALF, 3, &coins, &masks,
                         NULL) != PQSAMP_OK)
{
  /* handle failure */
}
```

The random callback must fill the complete request or fail. For two or more
shares, `coins` and `masks` must be cryptographically independent. Masked
consumers should retain all shares rather than reconstructing the sample.

## Layout

```text
include/pqsamp.h        public API
src/scalar.c            readable scalar sampler
src/maskaglia.c         masked Maskaglia candidate path
src/gadgets.c           Boolean masking gadgets
src/masked.c            pooled scheduler and public compaction
src/profiles.c          private fixed profiles
adapters/hawk/          masked two-center research adapter
tools/profile_oracle.c  offline MPFR verification
tests/                  core sampler and adapter checks
```

## Performance status

`make bench` emits JSON Lines for both centers on the scalar path and the
portable masked path with one through four shares. On the built-in `s=3/2`
profile, the zero-center side pool uses exactly one secure AND per raw
Bernoulli batch. Its analytic expectation is about `6.153732` secure AND calls
per output. Across 256 independent 32,768-sample runs, the observed mean was
`6.165533`, with a range of `6.120667` to `6.210724`. The pinned trace uses
1,914 raw side batches, 1,432 stage-one batches, and 1,258 stage-two batches:
`203370 / 32768 = 6.206359863` secure AND calls per output. This individual
trace is not a regression threshold.

The half-center path postpones `Y` reconstruction until final acceptance. Its
pinned trace uses exactly `183188 / 32768 = 5.5904541015625` secure AND calls
per output; 256 independent runs had mean `5.594260` and range `5.560944` to
`5.631226`. The paper's `4.0` and `4.9` figures use different profiles and
accounting, so they are comparison figures rather than thresholds attained by
this implementation. Host wall time is diagnostic only; there are no AVX2
results or board cycle claims.

## Remaining work

- complete interval certificates for tails quantization and width error
- reviewed compiler and timing evidence on named targets
- measured AVX2 Cortex-M4 and RV32 implementations
- Boolean-to-arithmetic conversion and masked downstream scheme arithmetic
- physical leakage experiments and reproducible raw measurements
- profile retuning and reconciliation with the paper's accounting model
- a complete formal composition argument for the exact C scheduler

## Primary references

- [Maskaglia](https://eprint.iacr.org/2026/988)
- [CS20 PINI composition](https://doi.org/10.1109/TIFS.2020.2971153)
- [HAWK v1.1](https://hawk-sign.info/hawk-spec.pdf)
- [source ledger](docs/sources.md)
