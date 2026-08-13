# hawk research adapter

disabled in checkpoint 1.

the future boundary takes the secret coset bit `t` in boolean shares, samples
around `t/2`, maps with `x = 2z - t`, and keeps `x` shared through the HAWK
selection/arithmetic boundary. the scalar floating reference must never be
plugged into signing and described as protected.

HAWK was withdrawn from NIST's additional-signature process on July 29, 2026.
this adapter is retained to reproduce and evaluate the Maskaglia paper, not to
claim a deployment-ready HAWK implementation.
