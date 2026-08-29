# =============================================================================
# The big-integer oracle: s2n-bignum's generic x86-64 routines, machine-code-proved
# (HOL Light, one proof per routine in upstream x86/proofs), assembled here so the
# in-house Montgomery kernels can be compared with them limb for limb.
#
# Test-only. Built where its GAS sources assemble -- Linux, x86-64, GCC or Clang --
# and nowhere else; nothing in it is shipped or linked into a server binary. On the
# other platforms the same comparisons run against the Python vectors instead.
# =============================================================================
set(S2N_BIGNUM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/s2n-bignum)

set(S2N_BIGNUM_SOURCES
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_add.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_amontmul.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_cmul.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_copy.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_coprime.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_demont.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_eq.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_lt.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_modadd.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_modexp.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_modinv.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_modsub.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_montifier.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_montmul.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_montredc.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_montsqr.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_mul.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_negmodinv.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_shl_small.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_shr_small.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_sqr.S
    ${S2N_BIGNUM_DIR}/x86/generic/bignum_sub.S
)

enable_language(ASM)

add_library(s2n_bignum_oracle STATIC ${S2N_BIGNUM_SOURCES})
target_include_directories(s2n_bignum_oracle PUBLIC ${S2N_BIGNUM_DIR}/include)
set_target_properties(s2n_bignum_oracle PROPERTIES FOLDER "tests" LINKER_LANGUAGE C)
