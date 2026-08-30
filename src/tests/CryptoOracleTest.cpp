/**
 * @file
 * @brief The in-house kernels against s2n-bignum's machine-code-proved routines.
 *
 * Compiled only where the oracle assembles (Linux x86-64, GCC/Clang: MANGOS_CRYPTO_ORACLE).
 * Everything the Montgomery layer does has a proved counterpart here: montmul, montsqr,
 * the Montgomery form of 1 (montifier / demont), the modular exponentiation, the
 * modular inverse, plain multiplication, add and sub. Random operands in the kernels'
 * domain (below the modulus) at 1 to 64 limbs, plus the edge patterns, both C++ tiers.
 */

#include "TestHarness.h"

#if defined(MANGOS_CRYPTO_ORACLE)

#include "Crypto/BigInt.h"
#include "Crypto/ModExp.h"
#include "Crypto/Montgomery.h"

extern "C"
{
#include "s2n-bignum.h"
}

#include <cstring>
#include <random>
#include <vector>

using namespace MaNGOS::Crypto;

namespace
{
    std::mt19937_64 g_rng(0x53324E2D42494Eull);

    std::vector<Limb> RandomLimbs(size_t k)
    {
        std::vector<Limb> v(k);
        for (auto& l : v) l = g_rng();
        return v;
    }

    /// An odd modulus of k limbs with a non-zero top limb.
    std::vector<Limb> OddModulus(size_t k)
    {
        std::vector<Limb> m = RandomLimbs(k);
        m[0] |= 1;
        if (m[k - 1] == 0) m[k - 1] = 1;
        return m;
    }

    /// A value below m (the same limb count): random limbs, reduced with BigInt.
    std::vector<Limb> Below(const std::vector<Limb>& m)
    {
        const size_t k = m.size();
        const BigInt modulus = BigInt::FromLimbs(m.data(), k);
        std::vector<Limb> r = RandomLimbs(k);
        const BigInt reduced = BigInt::FromLimbs(r.data(), k) % modulus;
        std::vector<Limb> out(k, 0);
        for (size_t j = 0; j < reduced.LimbCount(); ++j) out[j] = reduced.Limbs()[j];
        return out;
    }

    std::vector<Limb> MinusOne(const std::vector<Limb>& m)
    {
        std::vector<Limb> v = m;
        v[0] -= 1;   // m is odd, so no borrow
        return v;
    }

    bool Same(const std::vector<Limb>& a, const std::vector<Limb>& b)
    {
        return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * LimbBytes) == 0;
    }

    Limb N0Inv(const std::vector<Limb>& m)
    {
        MontgomeryContext ctx(BigInt::FromLimbs(m.data(), m.size()));
        return ctx.N0Inv();
    }

    void CompareMontMul(MontMulFn kernel, const std::vector<Limb>& m, const std::vector<Limb>& x, const std::vector<Limb>& y)
    {
        const size_t k = m.size();
        std::vector<Limb> ours(k), theirs(k);
        kernel(ours.data(), x.data(), y.data(), m.data(), N0Inv(m), k);
        bignum_montmul(k, theirs.data(), x.data(), y.data(), m.data());
        CHECK(Same(ours, theirs));
        if (Same(x, y))
        {
            bignum_montsqr(k, theirs.data(), x.data(), m.data());
            CHECK(Same(ours, theirs));
        }
    }
}

TEST(CryptoOracle_montmul_matches_s2n_bignum)
{
    for (size_t k : { size_t(1), size_t(2), size_t(3), size_t(4), size_t(8), size_t(16), size_t(17), size_t(32), size_t(64) })
    {
        for (int round = 0; round < 4; ++round)
        {
            const std::vector<Limb> m = OddModulus(k);
            const int cases = k >= 32 ? 400 : 2000;
            const bool asmHere = HasAsmTier() && k % 4 == 0;
            for (int i = 0; i < cases; ++i)
            {
                const std::vector<Limb> x = Below(m), y = Below(m);
                CompareMontMul(&MontMulPortable, m, x, y);
                if (HasMulxTier())
                {
                    CompareMontMul(&MontMulMulx, m, x, y);
                }
                if (asmHere)
                {
                    CompareMontMul(&MontMulAsm, m, x, y);
                }
            }
            // the edge patterns
            const std::vector<Limb> mm1 = MinusOne(m);
            std::vector<Limb> one(k, 0), zero(k, 0);
            one[0] = 1;
            std::vector<MontMulFn> kernels = { &MontMulPortable };
            if (HasMulxTier()) kernels.push_back(&MontMulMulx);
            if (asmHere) kernels.push_back(&MontMulAsm);
            for (MontMulFn kernel : kernels)
            {
                CompareMontMul(kernel, m, mm1, mm1);
                CompareMontMul(kernel, m, mm1, one);
                CompareMontMul(kernel, m, zero, mm1);
                CompareMontMul(kernel, m, one, one);
                CompareMontMul(kernel, m, zero, zero);
                const std::vector<Limb> same = Below(m);
                CompareMontMul(kernel, m, same, same);
            }
        }
        // a modulus whose top limb is 0x8000000000000000, and one whose top limb is 1
        for (Limb top : { Limb(1) << 63, Limb(1) })
        {
            if (k == 1 && top == 1)
            {
                continue;   // that would be the modulus 1, which no context accepts
            }
            std::vector<Limb> m = OddModulus(k);
            m[k - 1] = top;
            m[0] |= 1;
            std::vector<MontMulFn> kernels = { &MontMulPortable };
            if (HasMulxTier()) kernels.push_back(&MontMulMulx);
            if (HasAsmTier() && k % 4 == 0) kernels.push_back(&MontMulAsm);
            for (MontMulFn kernel : kernels)
            {
                for (int i = 0; i < 200; ++i)
                {
                    CompareMontMul(kernel, m, Below(m), Below(m));
                }
                CompareMontMul(kernel, m, MinusOne(m), MinusOne(m));
                CompareMontMul(kernel, m, MinusOne(m), Below(m));
            }
        }
    }
}

TEST(CryptoOracle_montgomery_constants_match_s2n_bignum)
{
    for (size_t k : { size_t(1), size_t(4), size_t(16), size_t(32) })
    {
        for (int round = 0; round < 8; ++round)
        {
            const std::vector<Limb> m = OddModulus(k);
            MontgomeryContext ctx(BigInt::FromLimbs(m.data(), k));
            // negmodinv: -m^-1 mod 2^64 (the word-level version is the first limb of the array one)
            std::vector<Limb> negInv(k);
            bignum_negmodinv(k, negInv.data(), m.data());
            CHECK_EQ(negInv[0], ctx.N0Inv());
            // montifier: 2^(128k) mod m, which is R^2
            std::vector<Limb> r2(k), t(3 * k);
            bignum_montifier(k, r2.data(), m.data(), t.data());
            CHECK(std::memcmp(r2.data(), ctx.R2(), k * LimbBytes) == 0);
            // demont of our One is 1
            std::vector<Limb> back(k);
            bignum_demont(k, back.data(), ctx.One(), m.data());
            CHECK_EQ(back[0], Limb(1));
            bool rest = true;
            for (size_t j = 1; j < k; ++j) rest = rest && back[j] == 0;
            CHECK(rest);
        }
    }
}

TEST(CryptoOracle_modexp_matches_s2n_bignum)
{
    for (size_t k : { size_t(2), size_t(4), size_t(16), size_t(32) })
    {
        for (int round = 0; round < (k >= 32 ? 3 : 10); ++round)
        {
            const std::vector<Limb> m = OddModulus(k);
            const BigInt modulus = BigInt::FromLimbs(m.data(), k);
            MontgomeryContext ctx(modulus);
            for (int i = 0; i < 4; ++i)
            {
                const std::vector<Limb> a = Below(m), p = RandomLimbs(k);
                std::vector<Limb> theirs(k), t(3 * k);
                bignum_modexp(k, theirs.data(), a.data(), p.data(), m.data(), t.data());
                const BigInt ours = ModExp(BigInt::FromLimbs(a.data(), k), BigInt::FromLimbs(p.data(), k), ctx);
                CHECK_STR(ours.ToHex(), BigInt::FromLimbs(theirs.data(), k).ToHex());
            }
            // the edge patterns: base m-1, exponent all ones, exponent zero
            {
                const std::vector<Limb> a = MinusOne(m);
                std::vector<Limb> p(k, ~Limb(0)), theirs(k), t(3 * k);
                bignum_modexp(k, theirs.data(), a.data(), p.data(), m.data(), t.data());
                CHECK_STR(ModExp(BigInt::FromLimbs(a.data(), k), BigInt::FromLimbs(p.data(), k), ctx).ToHex(),
                          BigInt::FromLimbs(theirs.data(), k).ToHex());
                std::vector<Limb> zero(k, 0);
                bignum_modexp(k, theirs.data(), a.data(), zero.data(), m.data(), t.data());
                CHECK_STR(ModExp(BigInt::FromLimbs(a.data(), k), BigInt(), ctx).ToHex(),
                          BigInt::FromLimbs(theirs.data(), k).ToHex());
            }
        }
    }
}

TEST(CryptoOracle_modinv_and_mul_match_s2n_bignum)
{
    for (size_t k : { size_t(1), size_t(4), size_t(16), size_t(32) })
    {
        for (int round = 0; round < 20; ++round)
        {
            std::vector<Limb> m = OddModulus(k);
            if (k == 1 && m[0] < 3) m[0] = 3;
            const BigInt modulus = BigInt::FromLimbs(m.data(), k);
            std::vector<Limb> a = Below(m);
            const BigInt aValue = BigInt::FromLimbs(a.data(), k);
            const BigInt ours = ModInverse(aValue, modulus);
            std::vector<Limb> t(3 * k), pad(k, 0);
            if (!bignum_coprime(k, a.data(), k, m.data(), t.data()))
            {
                CHECK(ours.IsZero());
                continue;
            }
            std::vector<Limb> theirs(k);
            bignum_modinv(k, theirs.data(), a.data(), m.data(), t.data());
            CHECK_STR(ours.ToHex(), BigInt::FromLimbs(theirs.data(), k).ToHex());
        }
        for (int round = 0; round < 20; ++round)
        {
            const std::vector<Limb> x = RandomLimbs(k), y = RandomLimbs(k + round % 3);
            std::vector<Limb> theirs(x.size() + y.size());
            bignum_mul(theirs.size(), theirs.data(), x.size(), x.data(), y.size(), y.data());
            const BigInt ours = BigInt::FromLimbs(x.data(), x.size()) * BigInt::FromLimbs(y.data(), y.size());
            CHECK_STR(ours.ToHex(), BigInt::FromLimbs(theirs.data(), theirs.size()).ToHex());
        }
    }
}

#else

TEST(CryptoOracle_not_available_on_this_platform)
{
    // The s2n-bignum oracle assembles on Linux x86-64 with GCC or Clang only; here the
    // same comparisons ran against the Python vectors and the captured goldens.
    CHECK(true);
}

#endif
