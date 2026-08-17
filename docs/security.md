# Security status

## Supported claims

- The runtime uses integer arithmetic and generated boundary counts; no
  floating-point function influences a sample.
- The built-in table rows and exact saturation bounds are independently
  recomputed with 512-bit MPFR directed rounding by `make oracle`.
- Scalar deterministic vectors are pinned for all four profile/center pairs.
- Gadget truth tables, 1--4 share reconstruction, distribution moments, RNG
  failures, and the share-aware adapter are tested.
- A public cap of four candidate batches per requested 32-output block retains
  the prior conservative union bound below `m * 2^-94` for `m` blocks.
- ASan, UBSan, Valgrind, a short fuzz target, GCC/Clang builds, freestanding
  Cortex-M4 and RV32 compilation, and a CBMC pack/unpack proof are available.
- The portable library performs no heap allocation and accepts no hidden RNG.

## Masking model

The source represents values in Boolean shares and implements the paper's
PINI-style composition: linear operations are sharewise, each secure AND gets
fresh pair randomness, and public rejection decisions are refreshed before
recombination. Samples remain shared across the public sampler API and adapter.

Stage one refreshes and declassifies only support validity and `K<q`; stage two
refreshes and declassifies only final acceptance. Half-center `G,S` values are
not reconstructed until after final acceptance, and then remain Boolean-shared.
Candidate values, `q`, `K`, the boundary, equality, and individual shares are
never recombined. The iteration count and lane maps depend on public rejection
data, following the
paper's Remark 3 model. This is not valid for secretly selected profiles or
parameters. A secret-center scheme must run both public-center samplers.

Compaction applies one public lane permutation independently to every share of
every candidate field. This preserves Boolean sharing, but its branches and
memory addresses depend on refreshed, declassified survivor and acceptance
masks. The permutation is stable and identical for every share.

Under the independent-randomness, declassification, and PINI composition
assumptions above, successful calls retain the compiled finite target
distribution. Finite retry caps create explicit bounded failures, never an
alternate output distribution. This statement is not a complete
machine-checked composition proof of the exact C scheduler.

This describes source structure. It is not a side-channel certification.
Compiler transformations, instruction transitions, spills, register
allocation, microarchitecture, and measurement noise are outside the abstract
probing proof.

The current optimized GCC and Clang host assembly spills shares and reuses
registers across protected intermediates. Those transitions are incompatible
with treating this build as proven secure in a Hamming-distance leakage model.
`make assembly` preserves the dumps for review; it is not a passing security
test.

On the current x86-64 GCC `-O2` build, `-fstack-usage` reports 2,352 bytes for
the half-center scheduler, 2,896 bytes for the zero-center scheduler, 512 bytes
for its stage-one wrapper, 192 bytes for a side-pool draw, 1,264 bytes for
saturated geometry, and 624 bytes for FIFO reconstruction/output. The
zero-center scheduler frame grew by 48 bytes for its 36-byte pool. Summing the
deepest reported library frames gives a conservative 5,192-byte zero-center
path, down from the previous 5,816-byte estimate because the large rational
Bernoulli retry frame is no longer on that path. Callback stack is not included.
The corresponding half-center path is 4,632 bytes, below its 5,584-byte limit.
The adapter has a separate bounded 2,000-byte frame.
`make stack` reproduces compiler frame reports; embedded limits still require
target linking and measurement.

## Explicit non-claims

- No generic “side-channel secure” or “constant power” claim.
- No physical fixed-versus-random, power, or EM result.
- No AVX2 or board-cycle result.
- No proof that GCC or Clang preserve PINI at any optimization level.
- No complete masked HAWK signer or Boolean-to-arithmetic conversion.
- No claim that the research widths equal HAWK v1.1 parameters.
- No approval beyond the four compiled research profiles.

Accepted-lane addresses depend on the refreshed and declassified acceptance
mask. Pending-batch addresses likewise depend on the declassified stage-one
survivor mask. This is public rejection information in the Maskaglia model; the
implementation does not claim constant-address execution with respect to that
data or physical side-channel security.

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
