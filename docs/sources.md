# source ledger

checked 2026-08-09. links below are primary sources. snapshots are pinned where
code can move.

## maskaglia

the sampler definition comes from Abou Haidar, Espitau, Hoffmann, and
Tibouchi, [Maskaglia, ePrint 2026/988](https://eprint.iacr.org/2026/988).
sections 3--5 are the algorithm source. section 6 and table 2 report the masked
cost study; they are not a substitute for the earlier construction.

for `c=t/2`, checkpoint 1 implements only these unmasked recipe and arithmetic
pieces, with `MG_EBOUND` rather than an output when a public bound is exceeded:

- `G` is geometric with `P[G=k]=2^(-k-1)`;
- `c=0` removes the second encoding of zero;
- `c=1/2` maps the two signs to `-G` and `G+1`;
- acceptance has exponent `(|z-c|/s-s/2)^2`.

the paper's integer rejection path is the next source-controlled slice. with
`s=p/q`, `c=p'/q'`, and

```text
g   = gcd(2q^2, p^2 q')
N   = (2pqq')^2 / g^2
L_y = (2q^2 |yq' - p'| - p^2 q') / g
L_y^2 = N q_y + r_y, 0 <= r_y < N
```

the boundary coin is `T_y=2^(1-r_y/N)-1`. truncation and quantization from the
paper must be included before making a finite-proxy or Renyi claim.

the authors' [supplementary repository, pinned at
`1342642`](https://github.com/shhun/unmaskaglia/tree/1342642aae6e3588e8fa1d18869803d394473c70)
is a differential reference, not vendored code. the inspected snapshot's C
demo is unmasked, uses `rand()`, and has data-dependent control flow. the
snapshot has no license file, so this project reimplements from the paper and
copies nothing from it.

## parameter records

the supplement's `parameters.py` includes these research fixtures:

| case | rational `s` | implied `sigma=s/sqrt(2 ln 2)` | HAWK v1.1 `sigma_sign` |
|---|---:|---:|---:|
| HAWK-512 | `3/2` | `1.27398270043203` | `1.278` |
| HAWK-1024 | `1521/1000` | `1.29181845823808` | `1.299` |

they are close, not equal. using them in a HAWK signing path would change the
distribution and deterministic vectors. checkpoint 1 uses `3/2` only as a math
fixture.

## hawk

the normative format comes from the [HAWK v1.1
specification](https://hawk-sign.info/hawk-spec.pdf), especially equations
(3)--(4), table 4, and section 3.5.1 / algorithm 14. it asks for samples in
`2Z+t`; this project samples around `t/2` then maps with `x=2z-t`.

the [official C99 source, pinned at
`1b9fef5`](https://github.com/hawk-sign/dev/tree/1b9fef52559273fe7b40fe3e22968eaedd3a4c2a)
is the future interoperability reference. a secret-center masked integration
must compute both centers and masked-select; it must not branch or index on
`t`.

NIST [marks HAWK withdrawn as of July 29,
2026](https://csrc.nist.gov/projects/pqc-dig-sig/round-3-additional-signatures).
the HAWK team discusses the key-recovery result and withdrawal in the
[NIST PQC forum thread](https://groups.google.com/a/list.nist.gov/g/pqc-forum/c/2r2u6SbHun4).
HAWK is kept here only as the paper's research benchmark.

## leakage motivation

Guerreau and Rossi's [power-analysis study, ePrint
2024/1248](https://eprint.iacr.org/2024/1248) motivates protecting HAWK's
Gaussian outputs. it does not validate this implementation. leakage claims
require compiler, assembly, statistical, and physical evaluation listed in
`security.md` and `roadmap.md`.
