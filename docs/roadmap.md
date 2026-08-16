# Roadmap

## Portable software release

- [x] one C99 `pqsamp_` API with separate core and adapter archives
- [x] exact-integer scalar sampler
- [x] corrected finite profile tables and directed-rounding MPFR check
- [x] 32-lane Boolean masking with 1--4 shares
- [x] secure AND, equality, comparison, Bernoulli, geometry, and fused proposal
- [x] two-stage early rejection and pooled public lane compaction
- [x] postponed three-gate half-center reconstruction after final acceptance
- [x] masked two-center research adapter and minimal demo
- [x] compact deterministic/statistical/adapter tests
- [x] sanitizer, Valgrind, fuzz, CBMC, GCC/Clang, M4, and RV32 build targets

## Evidence and native optimization

- [ ] interval certificate covering the full Gaussian normalizer, tail,
  boundary quantization, width approximation, and multi-call composition
- [ ] an AVX2 sampler instantiated over vector words and measured against the
  portable backend
- [ ] Cortex-M4 and RV32 board cycle, flash, stack, and randomness measurements
- [ ] compiler/flag assembly corpus with reviewed branch, address, and spill
  classifications
- [ ] dudect-style timing experiments and reproducible raw records
- [ ] physical leakage experiments on a named capture board
- [ ] retuned profiles with a new independently checked error certificate
- [ ] reconciliation of local gate counters with the paper's profile and
  accounting conventions
- [ ] complete formal composition of the implemented C scheduler, including
  refreshed declassification, public compaction, caps, and output wiping

## Scheme work

- [x] masked selection and `x=2y-t` adapter boundary
- [ ] PINI-compatible Boolean-to-arithmetic conversion for a chosen consumer
- [ ] masked downstream arithmetic and complete sign/verify path
- [ ] scheme-specific rational-width certificate and official interoperability
  vectors

HAWK remains a useful research case, but the general sampler API is the project
boundary. A new adapter should be added only for a fixed-public-profile PQC
consumer whose downstream representation and leakage boundary are understood.
