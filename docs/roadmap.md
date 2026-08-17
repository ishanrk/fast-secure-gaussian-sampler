# Roadmap

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

- [ ] PINI-compatible Boolean-to-arithmetic conversion for a chosen consumer
- [ ] masked downstream arithmetic and complete sign/verify path
- [ ] scheme-specific rational-width certificate and official interoperability
  vectors
