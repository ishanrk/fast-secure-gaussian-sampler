# fast-secure-gaussian-sampler

A small C99 library for fixed-profile discrete-Gaussian sampling with the
Maskaglia transform. It provides a readable scalar path, a portable 32-lane
Boolean-masked path, and a share-aware adapter boundary for PQC schemes.

This is research software. The C code follows the paper's PINI gadget structure,
but that does not prove that an optimizing compiler or a physical target
preserves the abstract leakage model. No production side-channel claim is made.

## What is implemented

- Exact integer runtime sampling: no floating point, logarithm, or `libm` in the
  library.
- Corrected finite Maskaglia acceptance with precomputed rational-profile
  tables, exact saturated acceptance geometry, and 43/45-bit boundary coins.
- Portable bitslicing over 32 lanes and Boolean outputs with 1--4 shares.
- CS20 PINI-style `SecAnd`, masked equality/comparison, exact masked Bernoulli,
  fused discrete-Laplace evaluation, and refreshed public decisions.
- Separate caller-owned streams for sampler coins and masking randomness.
- Four fixed candidate batches and fixed-address accepted-lane selection per
  block of up to 32 outputs. The worst built-in profile fails to fill a block
  with probability below `2^-94`; failure zeros the output and returns
  `PQSAMP_ERR_BOUND`.
- A masked-center adapter that samples both centers, selects without unmasking,
  and returns the shared representation `x = 2y - t`.
- Deterministic vectors, modest distribution tests, MPFR-directed profile
  checks, sanitizer/fuzzer/Valgrind targets, a CBMC packing proof, GCC/Clang
  builds, and freestanding Cortex-M4/RV32 compile targets.

The implementation is intentionally one portable backend. There is no empty
AVX2 facade or claimed hardware result: a native backend belongs here only when
it is real, differential-tested, and measured.

## Build and run

```sh
make                 # build/libpqsamp.a and build/libpqsamp_hawk.a
make test            # three focused test programs
make oracle          # verify every built-in table with MPFR directed rounding
make sanitize        # ASan + UBSan
make check-clang     # rebuild and test with Clang
make valgrind
make cbmc            # prove 32x16 pack/unpack round trips
make stack           # compiler stack-frame report
make demo            # scalar sampler and masked adapter examples
make bench           # scalar and shares 1--4 JSON Lines records
make install PREFIX=/usr/local DESTDIR=/tmp/package
```

`make cross-m4` uses Clang's freestanding Cortex-M4 target. `make cross-rv32`
uses `riscv64-linux-gnu-gcc` with an RV32 ABI; override `M4_CC`, `M4_AR`,
`RV32_CC`, or `RV32_AR` when the tools are elsewhere.
The optional adapter archive depends on the core archive, so static consumers
link it as `-lpqsamp_hawk -lpqsamp`.
The library itself has no external link dependency; only the offline oracle
requires MPFR and GMP.

## API

The public surface is [pqsamp.h](include/pqsamp.h). A random callback must fill
the complete request or fail.

```c
pqsamp_rng rng;
int16_t out[64];
const pqsamp_params *params =
    pqsamp_params_get(PQSAMP_PROFILE_S3_2, PQSAMP_CENTER_ZERO);

if (pqsamp_rng_init(&rng, randombytes, random_context) != PQSAMP_OK ||
    pqsamp_generate(out, 64, params, &rng, NULL) != PQSAMP_OK)
{
  /* handle failure */
}
```

For masked output, pass two distinct RNG contexts and retain all shares:

```c
pqsamp_masked_i16 out[64];

if (pqsamp_generate_masked(out, 64, params, 3, &sampler_coins,
                           &masking_randomness, NULL) != PQSAMP_OK)
{
  /* out is zeroed */
}
```

`pqsamp_reconstruct()` is for tests and explicitly unmasked consumers. A masked
consumer should operate on `pqsamp_masked_i16` directly.

## Runtime construction

For public rationals `s=p/q` and `c=p'/q'`, the generated record stores

```text
g   = gcd(2 q^2, p^2 q')
K0  = (2 p q q' / g)^2
Ly  = (2 q^2 |y q' - p'| - p^2 q') / g
Ly^2 = qy K0 + ry
```

The proposal is a finite discrete Laplace sample. Acceptance draws
`K=min(Geom(1/2), Ksat)`, where `Ksat=max(qy)+1`. It rejects for `K<qy`, accepts
for `K>qy`, and at equality accepts when a uniform `pU`-bit integer is below

```text
floor(2^pU (2^(1 - ry/K0) - 1)).
```

The exact saturation matters. The paper supplement's aggressive `K=40` cut
checks only aggregate accept/reject mass and makes far-tail outputs unreachable;
this implementation does not use it. It also handles the paper pseudocode's
all-zero geometric and complemented-threshold ambiguities explicitly. See
[design.md](docs/design.md) for the corrected control flow.

## Built-in research profiles

| `s` | center | implied sigma | support | `Ksat` | `pU` |
|---:|---:|---:|---:|---:|---:|
| `3/2` | `0` | `1.2739827004` | `[-13,13]` | 63 | 45 |
| `3/2` | `1/2` | `1.2739827004` | `[-13,13]` | 69 | 43 |
| `1521/1000` | `0` | `1.2918184582` | `[-13,13]` | 61 | 45 |
| `1521/1000` | `1/2` | `1.2918184582` | `[-13,13]` | 66 | 43 |

The public `pqsamp_params` descriptor also allows generated fixed profiles for
other PQC consumers. A record must include its own approximation/tail analysis;
passing `pqsamp_params_check()` proves only structural and integer bounds.
`make oracle` reproduces the built-in table rows, raw acceptance rates, batching
failure, and estimated order-256 Renyi figures recorded in
[results/profiles.json](results/profiles.json).

These profiles are not the official HAWK v1.1 distributions: HAWK lists signing
widths `1.278` and `1.299`. The adapter is therefore a working share-preserving
integration boundary and research demo, not a claim of HAWK KAT compatibility
or a complete masked HAWK signer. Official HAWK immediately consumes ordinary
samples in unmasked arithmetic; protecting the full signer also requires a
justified Boolean-to-arithmetic conversion and masked downstream arithmetic.

## Security boundary

The portable sampler has fixed candidate-batch count, fixed array bounds, no
heap allocation, no secret-indexed table access, fixed-address lane selection,
and no early unmasking of the sample. Rejection and validity masks are refreshed
before being made public, as allowed by the paper's model. The exact-fill RNG
error is sticky.

Still unproven and unmeasured:

- preservation of PINI under a particular compiler, optimization level, and
  ABI;
- transition, glitch, register-spill, power, or electromagnetic leakage;
- AVX2 performance, embedded cycle counts, or physical traces;
- end-to-end masked signing in any external scheme;
- a formal interval certificate for the complete Gaussian tail and width
  approximation of arbitrary custom profiles.

See [security.md](docs/security.md), [sources.md](docs/sources.md), and the
[Maskaglia paper](https://eprint.iacr.org/2026/988).
