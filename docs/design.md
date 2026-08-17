# Design

## Boundary

The sampler is generic. It knows fixed public Gaussian profiles, random-bit
streams, and Boolean shares; it does not know keys, signatures, or secret center
derivation.

```text
fixed profile + sampler coins
              │
       scalar reference
              │
       portable 32-lane sampler ── Boolean-shared samples
              │
       scheme adapter ── masked select / representation mapping
```

All global names use the `pqsamp_` namespace. The library has no allocator or
hidden random source. Public argument validation occurs at the API boundary;
internal helpers operate on already checked fixed-size values.

## Corrected finite sampler

The implementation follows Maskaglia Algorithms 5/6, with three corrections
required by the published text and supplement:

1. Proposal geometry is conditional, while acceptance geometry is saturated.
   They are different distributions. An all-zero proposal word is rejected; an
   all-zero `Ksat`-bit acceptance word maps to `Ksat`.
2. The stored boundary value is the acceptance count
   `A=floor(2^pU (2^(1-r/K0)-1))`; equality accepts on `U<A`.
3. The runtime decision is exactly `K<q` reject, `K>q` accept, and `K=q`
   boundary coin. It does not transcribe Algorithm 13's reversed expression.

The proposal tables fuse the public functions of the first-one position:
two's-complement `y`, quotient `q`, boundary count `A`, and support validity.
The implementation scans every public row and masked-selects its constants. It
never indexes a table with the reconstructed proposal or side bit.

For center `1/2`, both proposal sides have the same quotient and boundary for a
given geometric value `G`. The masked path therefore retains shared `G` and
`S`, derives the acceptance inputs from `G`, and rejects only the invalid
terminal pair with `SecAnd(indicator_13,S)`. It constructs `Y=-G` or `G+1` only
after final acceptance.

For center zero, the side coordinate uses a private per-call FIFO. Each raw
batch draws shared planes `U0,U1`, computes `bad=SecAnd(U0,U1)`, and refreshes
and recombines only `not bad`. The public validity mask stably selects shared
`U0` lanes. The pairs `00,01,10` retain values `0,0,1`; `11` is discarded, so
each retained bit is exactly Bernoulli `1/3`. Two shared words hold at most 63
bits. A complete 32-bit side plane is removed before `SecDLX` runs, and public
surplus remains only for the next request in the same sampling call. A request
is bounded at 64 raw batches.

## Masked words

A `pqsamp_word` is one bit plane across 32 lanes. Each plane has `d` Boolean
shares, `1 <= d <= 4`:

```text
value = share[0] xor ... xor share[d-1]
```

NOT changes only share zero. XOR and selection by a public lane mask are
sharewise. `pqsamp_sec_and()` retains the intermediate structure of CS20
Algorithm 2 and consumes `d(d-1)/2` fresh words. Compiler value barriers are
placed around the protected intermediates; the function is not inlined.

Equality OR-reduces bit planes with secure AND. Unsigned comparison uses the
full-subtractor borrow recurrence from least to most significant bit. Public
decisions are pairwise-refreshed before shares are recombined.

Raw validity is correlated with raw `U0`; no independence is claimed between
those two values. Rejection sampling removes that correlation: conditional on
the complete public validity/retry trace, every retained coordinate is still
an independent Bernoulli `1/3` bit, so the public retry trace is independent of
the eventual retained result. `SecAnd` copies its inputs, only refreshed
validity is recombined, and the retained `U0` sharing is never reconstructed.
Compaction applies the same public, stable permutation to every share, and the
coordinate's next nonlinear use receives fresh gadget randomness. This relies
on the project's CS20 PINI composition model and on independent sampler and
masking streams; it is not a mechanized proof of the exact C scheduler or its
compiled program.

Sampler coins create the underlying uniform values. A distinct masking stream
creates the input shares, secure-AND randomness, and refresh randomness. The API
rejects the same RNG object or the same callback/context pair; callers must also
guarantee cryptographic independence for distinct contexts.

## Scheduling

The masked candidate operation is split at Algorithm 13's public early
rejection boundary. Stage one computes `SecDLX`, saturated `K`, masked `K<q`,
and support validity. After refresh, only the support-validity and early-reject
masks are recombined. Their public survivor mask is `live=valid & ~(K<q)`.
The quotient `q`, boundary count, saturated `K`, and all individual shares
remain masked. Zero-center records retain masked `y`; half-center records retain
only masked `G` and `S`.

Survivors are appended in ascending lane order to one pending 32-lane batch.
This compaction is a public linear permutation: every Boolean share is moved
independently, and the identical lane map is applied to every retained field.
No field is reconstructed. Unused lanes are public zero sharings.
Memory addresses during this step depend on the declassified `live` mask.

Stage two runs only when the pending batch is full, or once on the final partial
batch at the public cap. It computes masked `K==q`, a fresh boundary uniform,
and masked `uniform<boundary`. Final acceptance is
`not_equal | boundary_accept`; only its refreshed result is recombined.
Zero-center samples are appended directly to the caller. Accepted half-center
`G,S` records enter a one-batch per-call FIFO. A full or final partial FIFO is
reconstructed with three secure ANDs, then copied to the caller; public surplus
is discarded only after the requested count is reached.

Zero-center accounting is `95*stage1 + raw_side + 52*stage2`. Half-center
accounting is `89*stage1 + 51*stage2 + 3*reconstruction`.

For `n` outputs the public cap is `4*ceil(n/32)` stage-one batches. The normal
path stops as soon as `n` accepted values have been returned. If the cap is
reached, the final partial survivor batch is processed once and the complete
output is cleared unless enough values were accepted. Pooling does not weaken
the old conservative bound: whenever every old 128-candidate output block
succeeds, their pooled candidates contain enough accepted values. Pooled
failure is therefore contained in the old union of block-failure events.

The slowest profile's fully formed candidate acceptance is about `0.72084`.
The implementation also treats the rare invalid 14-bit proposal encodings as
candidate rejection, giving a conservative block failure below `2^-94`. The
inherited failure bound for `m=ceil(n/32)` blocks is at most `m * 2^-94`; this
is deliberately not presented as a tighter pooled bound. A failure returns
`PQSAMP_ERR_BOUND` and zeroes the complete output.

Subject to the stated independent-randomness, declassification, and PINI
composition assumptions, successful calls preserve the compiled finite target
distribution. The finite scheduler and side-pool caps add only their explicit
bounded failure modes; they do not substitute a biased fallback value.

In the paper's model, the number and positions of rejected independent
candidates and the stopping time are public, as allowed by Remark 3. This
scheduler is invalid when a secret parameter selects the sampler execution. A
scheme with a secret center must execute both public-center samplers and select
their shared outputs without revealing the center. These are model assumptions,
not physical-leakage evidence or statements about compiler output.

## Adapter

The masked two-center research adapter demonstrates the intended consumer
boundary. It draws both public centers, packs both shared outputs, and selects
each plane with

```text
z = z0 xor (center & (z0 xor zhalf)).
```

It then computes `x=2z-center` with a masked borrow chain and returns Boolean
shares. It does not reconstruct the center or sample. The adapter is useful to
any scheme with the same two-center representation, but it is not a full masked
HAWK signing implementation.
