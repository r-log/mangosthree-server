# s2n-bignum, test-only

A subset of [awslabs/s2n-bignum](https://github.com/awslabs/s2n-bignum) at upstream commit
`d779d06` ("Fix x86 Curve25519 Mach-O table references (#446)"), licensed Apache-2.0 OR ISC OR
MIT-0 (see `LICENSE`, `NOTICE`). Nothing here is shipped: these routines are assembled on the
Linux x86-64 test jobs only and linked into `mangos_tests`, where the in-house Montgomery kernels
(`src/shared/Crypto`) are compared with them limb for limb (`CryptoOracleTest.cpp`).

Why these: each routine is proved correct at the machine-code level in HOL Light. The proof of
every file vendored here exists upstream under `x86/proofs/<name>.ml` (`bignum_montmul.ml` proves
`BIGNUM_MONTMUL_SUBROUTINE_CORRECT`, `bignum_modexp.ml` proves `BIGNUM_MODEXP_SUBROUTINE_CORRECT`,
and so on for `montsqr`, `montredc`, `amontmul`, `demont`, `montifier`, `negmodinv`, `modinv`,
`coprime`, `mul`, `sqr`, `add`, `sub`, `cmul`, `eq`, `lt`, `copy`, `modadd`, `modsub`,
`shl_small`, `shr_small`); the proofs themselves are not vendored.

Contents: `include/s2n-bignum.h` (the declarations), `include/_internal_s2n_bignum*.h` (the
assembler macros the sources include), `x86/generic/*.S` (22 generic-size routines, GAS syntax,
System V and Windows ABIs selected by `WINDOWS_ABI`). Unmodified.

To refresh: copy the same files from a newer upstream commit, update the commit here, and check
that every `.S` still has its `.ml` proof upstream.
