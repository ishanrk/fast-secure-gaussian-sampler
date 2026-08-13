# security status

## claims made by this checkpoint

- deterministic bit ordering and error propagation are unit-tested;
- before an explicit public bound fails, the two discrete-Laplace proposal
  paths follow their mathematical recipes without clipping;
- the rejection exponent is exact when its checked 64-bit calculation succeeds;
- the scalar rejection transform's kernel identity is tested;
- the bitslice pack/unpack representation is compared to a naive fixture and
  tested over the complete 32-lane by 16-plane one-hot basis;
- address and undefined-behavior sanitizers pass on the current test suite;
  leak scanning is disabled in the sandbox because `/proc` tracing is blocked,
  and the library itself performs no heap allocation.

## claims not made

- no constant-time claim;
- no probing, PINI, transition, power, or EM leakage claim;
- no claim that a C compiler preserves a masked circuit;
- no formally bounded `libm` or floating uniform-conversion error;
- no HAWK security claim;
- no production parameter or tail bound;
- no certified total failure bound for `gmax` or `tries`;
- no production random generator.

the floating oracle branches on proposals and acceptance. it exists for
differential and statistical testing only.

## masked-backend gate

`MG_HAVE_MASKED` is zero and `MG_ENABLE_MASKED` is a compile-time error. this is
intentional. a normal `&`, an ordinary unmask/re-mask sequence, or a generic
ISW gadget would not by itself establish the paper's composed PINI result in
compiled software. the exact paper gadget, fresh-randomness schedule, compiler
output, spill behavior, and target leakage model must all be evaluated.
