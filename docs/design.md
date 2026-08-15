# Design

## Boundary

The sampler is generic. It knows fixed public Gaussian profiles, random-bit
streams, and Boolean shares; it does not know keys, signatures, or secret center
derivation.

```text
generated profile + sampler coins
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

Sampler coins create the underlying uniform values. A distinct masking stream
creates the input shares, secure-AND randomness, and refresh randomness. The API
rejects the same RNG object or the same callback/context pair; callers must also
guarantee cryptographic independence for distinct contexts.

## Scheduling

One internal invocation produces 32 candidates. For every output block of at
most 32 values, the public API always executes four invocations (128
candidates). Every candidate scans every destination slot and is selected with
a word mask. The loop counts and memory addresses therefore do not depend on
the refreshed public acceptance masks or proposal values.

The slowest profile's fully formed candidate acceptance is about `0.72084`.
The implementation also treats the rare invalid 14-bit proposal encodings as
candidate rejection, giving a conservative block failure below `2^-94`. The
failure bound for `m` blocks is at most `m * 2^-94`. A failure returns
`PQSAMP_ERR_BOUND` and zeroes the complete output.

The running public rank still depends on declassified rejection data. In the
paper's model, the number and positions of rejected independent candidates do
not reveal the eventual accepted sample. This is a model assumption, not
physical-leakage evidence or a statement about compiler output.

## Adapter

The HAWK-shaped adapter demonstrates the intended consumer boundary. It draws
both public centers, packs both shared outputs, and selects each plane with

```text
z = z0 xor (center & (z0 xor zhalf)).
```

It then computes `x=2z-center` with a masked borrow chain and returns Boolean
shares. It does not reconstruct the center or sample. The adapter is useful to
any scheme with the same two-center representation, but it is not a full masked
HAWK signing implementation.
