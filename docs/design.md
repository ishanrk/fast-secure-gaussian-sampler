# design

## boundary

the core library owns sampling. scheme adapters depend on the core, never the
other way around.

```text
rng + params -> scalar ref
             -> portable bitslice core -> architecture backend
                                      -> scheme adapter
```

the scalar API may return ordinary integers because it is explicitly unmasked.
the future masked API must keep outputs as bit planes of boolean shares. it must
not unmask into `int16_t *` before the scheme consumes the sample.

## rng contract

`mg_read_fn` either fills the full requested buffer or fails. the library:

- decodes source bytes little-endian;
- consumes the least-significant bit first;
- caches 64 bits at a time;
- propagates source failure through a sticky error;
- counts consumed bits;
- returns a sticky `MG_EBOUND` instead of clipping a long geometric value.

after `MG_ERNG` or geometric `MG_EBOUND`, the context is terminal and must be
reinitialized. this avoids treating the remainder of a failed unary code as a
fresh sample.

`gmax` and `tries` are public failure bounds, not a certified truncation policy.
an over-bound call produces no sample. callers must not silently reinitialize
and retry while describing the resulting conditional distribution as exact;
the total failure and tail analysis is checkpoint 3 work.

the geometric hot path scans one word with `ctz`, not one branch per zero.

sampler coins and fresh masking randomness will use separate contexts in the
masked API. domain separation and freshness rules belong in that API contract.

## rational exponent

`mg_ref_exp()` avoids signed absolute-value traps and checks every potentially
overflowing multiplication. its reduced fraction makes differential tests easy
and gives the parameter generator an exact target.

## bitslice layout

`mg_pack16()` maps bit `b` of lane `i` to bit `i` of word `b`. the implementation
uses a 32 by 32 SWAR transpose, with the unused upper 16 input bits zero. packing
and unpacking stay outside the future hot sampling loop.

## next implementation slice

1. reproduce the paper's public parameter-generation script and hashes;
2. add a certified MPFR interval oracle under `tools/`;
3. transcribe and test the exact finite proxy distribution;
4. implement one paper-faithful `SecAND`, then `SecEq` and `SecGeom`;
5. differential-test every bitsliced lane against the scalar proposal;
6. design a fixed public batch schedule for rejected lanes;
7. specialize the share count at compile time;
8. inspect optimized assembly before adding AVX2 or Cortex-M4 code;
9. integrate behind a share-aware scheme adapter.

the HAWK adapter remains useful for reproducing the Maskaglia paper, but HAWK's
2026 withdrawal means it is no longer the only product-level destination.
