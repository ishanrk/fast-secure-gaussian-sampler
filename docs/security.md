# Security status

## Supported claims

- The runtime uses integer arithmetic and generated boundary counts; no
  floating-point function influences a sample.
- The built-in table rows and exact saturation bounds are independently
  recomputed with 512-bit MPFR directed rounding by `make oracle`.
- Scalar deterministic vectors are pinned for all four profile/center pairs.
- Gadget truth tables, 1--4 share reconstruction, distribution moments, RNG
  failures, and the share-aware adapter are tested.
- Four fixed candidate batches give a per-32-output public failure probability
  below `2^-94` for the slowest built-in profile.
- ASan, UBSan, Valgrind, a short fuzz target, GCC/Clang builds, freestanding
  Cortex-M4 and RV32 compilation, and a CBMC pack/unpack proof are available.
- The portable library performs no heap allocation and accepts no hidden RNG.

## Masking model

The source represents values in Boolean shares and implements the paper's
PINI-style composition: linear operations are sharewise, each secure AND gets
fresh pair randomness, and public rejection decisions are refreshed before
recombination. Samples remain shared across the public sampler API and adapter.

This describes source structure. It is not a side-channel certification.
Compiler transformations, instruction transitions, spills, register
allocation, microarchitecture, and measurement noise are outside the abstract
probing proof.

The current optimized GCC and Clang host assembly spills shares and reuses
registers across protected intermediates. Those transitions are incompatible
with treating this build as proven secure in a Hamming-distance leakage model.
`make assembly` preserves the dumps for review; it is not a passing security
test.

On the current x86-64 GCC `-O2` build, `-fstack-usage` reports 3,488 bytes for
the candidate batch, 1,008 bytes for the masked generator, and a bounded
2,000-byte adapter frame. The nested adapter path is therefore roughly 7 KiB
before the caller's RNG frame. `make stack` reproduces compiler frame reports;
embedded limits still require target linking and measurement.

## Explicit non-claims

- No generic “side-channel secure” or “constant power” claim.
- No physical fixed-versus-random, power, or EM result.
- No AVX2 or board-cycle result.
- No proof that GCC or Clang preserve PINI at any optimization level.
- No complete masked HAWK signer or Boolean-to-arithmetic conversion.
- No claim that the research widths equal HAWK v1.1 parameters.
- No production approval for arbitrary user-created `pqsamp_params` records.

The compiler assembly dumps produced by `make assembly` are inspection inputs,
not automatic evidence. A physical claim must name the compiler, flags, target,
masking order, probe/leakage model, trace method, and raw data.

## Error handling

The callback either fills every requested byte or fails. RNG failure is sticky.
On a sampling error, a non-empty output is fully zeroed; no partially valid
prefix is returned. `PQSAMP_ERR_BOUND` is a public scheduling failure and must
not be silently converted into output.

Invalid pointer arguments are API precondition failures. They do not authorize
dereferencing an output solely to clear it.
