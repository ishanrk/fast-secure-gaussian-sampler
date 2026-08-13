# roadmap

## checkpoint 1: correctness base

- [x] C99 layout and strict build
- [x] injected buffered rng
- [x] fast bounded geometric decoder with explicit failure
- [x] `c=0` and `c=1/2` Laplace proposal recipes without clipping
- [x] checked rational exponent
- [x] approximate scalar diagnostic sampler
- [x] SWAR 32-lane pack/unpack
- [x] deterministic, kernel, mapping, sanitizer, and statistical tests

## checkpoint 2: paper-faithful masked core

- [ ] vendor or regenerate the paper's parameter data
- [ ] certified MPFR oracle
- [ ] exact finite proxy and Bernoulli path
- [ ] paper-faithful `SecAND`, `SecEq`, `SecGeom`, and `SecDLX`
- [ ] gate/randomness counters
- [ ] differential lane tests for two, three, and four shares

## checkpoint 3: production scheduling

- [ ] fixed public batching policy
- [ ] oblivious accepted-lane movement
- [ ] bounded failure analysis
- [ ] zero heap embedded build
- [ ] compile-time share-count specialization
- [ ] assembly branch, address, and spill checks

## checkpoint 4: systems evaluation

- [ ] portable u32 benchmark
- [ ] AVX2 backend
- [ ] Cortex-M4 backend
- [ ] RV32 portability backend
- [ ] compiler and flag matrix
- [ ] dudect and physical leakage experiments
- [ ] reproducible static results explorer

## checkpoint 5: integrations

- [ ] HAWK research adapter for paper reproduction
- [ ] identify a still-relevant fixed-parameter consumer
- [ ] upstream-quality patch where technically justified
