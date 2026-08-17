# fast-secure-gaussian-sampler

## What this is

A small portable C99 research library for fixed-profile discrete-Gaussian
sampling with the Maskaglia transform. It contains a readable scalar sampler,
a 32-lane Boolean-masked sampler, and a masked two-center research adapter.

The integer runtime has two built-in rational widths and centers `0` and `1/2`.
It has no heap or hidden RNG. Profile tables and candidate records are private.
The public API accepts only the four compiled profile and center combinations.

## Security status

The source follows the paper's Boolean masking and refresh structure. This is
not proof that a compiler or physical target preserves PINI. Optimized host
assembly spills shares and reuses registers, so the project makes no generic
side-channel-security or production claim. See [docs/security.md](docs/security.md).

The optional HAWK-shaped code is a masked two-center research adapter. It is
not a complete HAWK implementation or a known-answer-test compatible sampler.

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
portable masked path with one through four shares. The pinned `s=3/2`
32,768-sample trace is:

| center | stage 1 | raw side | stage 2 | reconstruction | secure ANDs | per sample |
|---|---:|---:|---:|---:|---:|---:|
| zero | 1432 | 1914 | 1258 | 0 | 203370 | 6.206359863 |
| half | 1339 | 0 | 1195 | 1024 | 183188 | 5.590454102 |

Gate accounting is documented in the [design](docs/design.md). The paper's
`4.9` and `4.0` figures use different profiles and accounting and are
comparison figures only. Host wall time is diagnostic; there are no AVX2 or
board-cycle results.

## Remaining work

Open evidence native backend and scheme tasks are in the
[roadmap](docs/roadmap.md).

## Primary references

- [Maskaglia](https://eprint.iacr.org/2026/988)
- [CS20 PINI composition](https://doi.org/10.1109/TIFS.2020.2971153)
- [HAWK v1.1](https://hawk-sign.info/hawk-spec.pdf)
- [design](docs/design.md)
- [security status](docs/security.md)
- [source ledger](docs/sources.md)
