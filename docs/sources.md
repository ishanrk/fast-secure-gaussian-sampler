# Source ledger

Checked 2026-08-15. Links below point to primary publications or pinned source
snapshots. Each entry states what the project uses it for; none is a claim that
the C implementation inherits a paper proof or a repository's validation.

## Maskaglia construction

The sampling construction comes from Abou Haidar, Espitau, Hoffmann, and
Tibouchi, [Maskaglia, ePrint 2026/988](https://eprint.iacr.org/2026/988).
Sections 3--5 define the discrete-Laplace proposal, integer rejection test, and
masked gadgets. For public rationals `s=p/q` and `c=p'/q'`, this implementation
precomputes

```text
g   = gcd(2 q^2, p^2 q')
K0  = (2 p q q' / g)^2
Ly  = (2 q^2 |y q' - p'| - p^2 q') / g
Ly^2 = qy K0 + ry, 0 <= ry < K0.
```

The finite runtime rejects for `K<qy`, accepts for `K>qy`, and, when `K=qy`,
accepts with probability `2^(1-ry/K0)-1`. The stored boundary is the integer
count

```text
A = floor(2^pU (2^(1-ry/K0)-1)),
```

so the comparison is strictly `U<A`.

The published pseudocode is not transcribed literally. The proposal and
acceptance geometric variables need different finite handling: an all-zero
proposal encoding is an invalid candidate, while an all-zero acceptance word
maps to the saturated value `Ksat`. The implementation also uses the decision
ordering above instead of Algorithm 13's displayed comparison. These choices
are exercised by the scalar/masked differential tests and the MPFR table
oracle; they are implementation corrections, not a revision of the paper.

The authors' [supplementary repository, pinned at
`1342642aae6e3588e8fa1d18869803d394473c70`](https://github.com/shhun/unmaskaglia/tree/1342642aae6e3588e8fa1d18869803d394473c70)
is used only as a parameter and differential reference. That snapshot contains
an unmasked C demo using `rand()` and data-dependent control flow. Its `K=40`
cut does not preserve the far-tail output law, so it is not used here. The
snapshot has no license file; no source is copied from it.

## Built-in profiles

The two research widths originate in the supplement. Both public centers are
instantiated over `[-13,13]`:

| `s` | center | implied `sigma=s/sqrt(2 ln 2)` | `Ksat` | `pU` |
|---:|---:|---:|---:|---:|
| `3/2` | `0` | `1.27398270043203` | 63 | 45 |
| `3/2` | `1/2` | `1.27398270043203` | 69 | 43 |
| `1521/1000` | `0` | `1.29181845823808` | 61 | 45 |
| `1521/1000` | `1/2` | `1.29181845823808` | 66 | 43 |

`make oracle` independently recomputes the stored quotients and
directed-rounding boundary counts. That verifies these four records; no
caller-defined profile API is exposed.

The widths are close to, but not equal to, the HAWK v1.1 signing widths `1.278`
and `1.299`. Consequently, the built-ins are research fixtures and are not
advertised as HAWK known-answer-test or signature-compatible profiles.

## Masking gadgets

- Cassiers and Standaert's [PINI paper](https://doi.org/10.1109/TIFS.2020.2971153),
  in particular Algorithm 2, is the source for the protected intermediate
  structure of `pqsamp_sec_and()` and for the abstract composition model.
- Azouaoui et al., [Protecting Dilithium against Leakage
  Revisited](https://doi.org/10.46586/tches.v2023.i4.58-79), Algorithm 3, is the
  source for pairwise refresh before recombination in `SecUnmask`. Its
  comparison gadget is also cited by Maskaglia.
- Bronchain and Cassiers,
  [bitsliced arithmetic/Boolean masking conversions](https://eprint.iacr.org/2022/158),
  supplies the full-adder lineage used by the masked comparison and adapter.
  Its Boolean-to-arithmetic conversions are a future scheme reference;
  this library does not implement or claim B2A conversion.

The C source follows those gadget structures with fresh pair randomness and
Boolean shares. PINI is an abstract probing-model statement. No claim is made
that a particular compiler, ABI, processor, or physical device preserves it.

## HAWK boundary

The [HAWK v1.1 specification](https://hawk-sign.info/hawk-spec.pdf), especially
Section 3.5.1 and Algorithm 14, defines samples in `2Z+t`. The adapter samples
both centers, masked-selects the chosen one, and returns Boolean shares of
`x=2y-t`.

The [official C99 implementation, pinned at
`1b9fef52559273fe7b40fe3e22968eaedd3a4c2a`](https://github.com/hawk-sign/dev/tree/1b9fef52559273fe7b40fe3e22968eaedd3a4c2a)
is the interoperability reference. The adapter is not patched into that signer
and does not implement its Boolean-to-arithmetic conversion or masked
downstream arithmetic. It is therefore a masked two-center research adapter,
not a complete masked HAWK signer.

NIST [lists HAWK as withdrawn](https://csrc.nist.gov/projects/pqc-dig-sig/round-3-additional-signatures)
as of 2026-07-29. HAWK remains here only as the motivating two-center research
consumer.

## C style and evaluation

[mlkem-native, pinned at
`69d24e37b8a04c6050ec55bc84a4228d7051bb4b`](https://github.com/pq-code-package/mlkem-native/tree/69d24e37b8a04c6050ec55bc84a4228d7051bb4b)
is the style and build-organization reference: a small namespaced public API,
strict warning flags, explicit portability targets, and no placeholder native
backend. It is not an algorithmic dependency.

Guerreau and Rossi's [HAWK power-analysis study, ePrint
2024/1248](https://eprint.iacr.org/2024/1248) motivates keeping samples masked
at the scheme boundary. It does not validate this implementation. Compiler and
physical leakage evaluation remain separate work described in `security.md` and
`roadmap.md`.
