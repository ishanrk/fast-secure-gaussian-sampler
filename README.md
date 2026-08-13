# Fast Secure Gaussian Sampler

A portable C99 research implementation of the Maskaglia approach to discrete
Gaussian sampling. The project is organized as a library so the scalar
correctness model, future bitsliced masked core, architecture backends, and
scheme adapters remain separate.

This first checkpoint establishes the unmasked reference that later masked
implementations will be tested against. It is **not side-channel secure** and
is **not production cryptography**.

## Checkpoint status

| Component | Status |
|---|---|
| Exact buffered bit stream | Implemented |
| Geometric sampler | Implemented with a word-level `ctz` fast path |
| Discrete-Laplace proposals for `c = 0` and `c = 1/2` | Implemented |
| Checked rational rejection exponent | Implemented |
| Floating diagnostic sampler | Implemented; approximate and variable-time |
| 32-lane, 16-bit bitslice transpose | Implemented with a SWAR transpose |
| Maskaglia `SecGeom`, `SecDLX`, and exact finite proxy | Not implemented |
| Masked production sampler and oblivious lane scheduler | Disabled |
| HAWK research adapter | Disabled |

`MG_HAVE_MASKED` and `MG_HAVE_HAWK_ADAPTER` are both `0`. Defining
`MG_ENABLE_MASKED` is a compile-time error, so the library cannot silently fall
back to an unmasked implementation.

## Build and test

```sh
make                 # build/libgaussian_sampler.a
make test            # deterministic, exact-math, packing, and statistics tests
make sanitize        # AddressSanitizer + UndefinedBehaviorSanitizer
make bench           # scalar and pack/unpack microbenchmarks
```

The default build uses strict portable C99 with no `-march=native`, heap
allocation, or hidden random source. Callers provide an exact-fill RNG
callback. Sampler coins and future masking randomness will use separate
contexts.

## Implemented transform

Write `t` for the center bit and `c = t/2`. The unbounded proposal targeted by
the decoder is

\[
q_c(z) \propto 2^{-|z-c|}, \qquad z \in \mathbb Z.
\]

`mg_rng_geom()` counts zero bits before the first one. For `t = 0`, the
proposal removes the duplicate negative zero. For `t = 1`, the two outputs at
geometric distance `k` are `-k` and `k+1`.

`gmax` and `tries` are explicit public failure bounds. A geometric code longer
than `gmax`, or an exhausted proposal/rejection budget, returns `MG_EBOUND`
without clipping or producing a sample. Silently reinitializing and retrying
would condition the output on success, so this checkpoint does not claim a
production failure or tail bound; that analysis remains on the roadmap.

For rational `s = sn/sd`, Maskaglia accepts with probability

\[
2^{-e(z)}, \qquad
e(z)=\left(\frac{|z-c|}{s}-\frac{s}{2}\right)^2.
\]

The reference stores this exponent exactly as

\[
e(z)=\frac{(d\,sd^2-sn^2)^2}{4sn^2sd^2},
\qquad d=|2z-t|.
\]

The identity

\[
|z-c|+e(z)=\frac{(z-c)^2}{s^2}+\frac{s^2}{4}
\]

shows that accepted samples have the intended discrete-Gaussian kernel. Tests
check the identity for both centers with exact rational exponents.

`mg_ref_sample_fp()` uses `exp2l()` only as a diagnostic Bernoulli oracle.
Ordinary `libm` and the finite-precision uniform conversion have no portable
certified error bound, and its proposal and rejection paths are variable-time.
The exact finite-proxy sampler from the paper is a later checkpoint.

## API and representation

The public API uses typed errors and a caller-owned `mg_rng` context. RNG source
failures and geometric-bound failures are sticky until the context is
reinitialized; a failed unary code is never clipped or restarted as a new
sample. Source bytes are decoded little-endian and consumed least-significant
bit first.

The scalar reference returns an integer `z` centered at `t/2`. Scheme-specific
representation is kept outside the core. For the HAWK research case,
`mg_ref_hawk_map()` performs the exact map `x = 2z - t` into `2Z + t`.
The future masked API will keep 32 lanes in Boolean-share bit planes through
the consuming scheme instead of unmasking into a scalar output buffer.

## HAWK positioning

HAWK is retained as the Maskaglia paper's research benchmark and integration
case, not as a deployment target. NIST marks HAWK as withdrawn from its
additional-signature process as of July 29, 2026.

The rational `s = 3/2` is only a mathematical fixture. It implies
`sigma = s/sqrt(2 ln 2) ~= 1.2739827`, while HAWK v1.1 specifies different
signing widths. No production parameter record will be added until the
paper's approximation, truncation, quantization, and Renyi-divergence choices
are independently regenerated.

## Repository layout

```text
include/                 public, reference, bitslice, and reserved masked APIs
src/core/                RNG and parameter validation
src/ref/                 readable scalar transform
src/portable/            architecture-neutral packed primitives
src/avx2/                planned x86 backend
src/cortex_m4/           planned embedded backend
src/rv32/                planned portability backend
adapters/hawk/           disabled research integration boundary
tests/                   deterministic unit and distribution tests
bench/                   local microbenchmarks
docs/                    design, security status, roadmap, and source ledger
tools/                   planned certified parameter generation
```

See [design](docs/design.md), [security status](docs/security.md), the
[roadmap](docs/roadmap.md), and the [source ledger](docs/sources.md) for the
implementation boundary and the remaining work.

## Primary references

- C. Abou Haidar, T. Espitau, C. Hoffmann, and M. Tibouchi,
  [*Maskaglia: A New, Efficient Approach to Masked Discrete Gaussian Sampling*](https://eprint.iacr.org/2026/988),
  CRYPTO 2026 / IACR ePrint 2026/988.
- HAWK team, [*HAWK v1.1 specification*](https://hawk-sign.info/hawk-spec.pdf).
- NIST,
  [Round 3 Additional Signatures](https://csrc.nist.gov/projects/pqc-dig-sig/round-3-additional-signatures).

Pinned source revisions and complete citation records are in
[`docs/sources.md`](docs/sources.md) and [`docs/refs.bib`](docs/refs.bib).
