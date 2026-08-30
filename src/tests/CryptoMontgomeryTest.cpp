/**
 * @file
 * @brief The Montgomery kernel, the context constants, ModExp and ModInverse.
 *
 * The kernel is checked against its definition (z * R = x * y mod m, by plain
 * division), the constants against BigInt's division, the exponentiation against
 * every captured golden and every Python vector, and the two C++ tiers against each
 * other on the same inputs. The edge patterns are the ones a Montgomery reduction
 * gets wrong first: operands just below the modulus, a modulus whose top limb is
 * 0x8000..., all-ones operands, and results that need the final subtraction.
 */

#include "TestHarness.h"
#include "BigIntVectors.h"
#include "CryptoGoldenVectors.h"

#include "Crypto/BigInt.h"
#include "Crypto/Fatal.h"
#include "Crypto/ModExp.h"
#include "Crypto/Montgomery.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace MaNGOS::Crypto;

namespace
{
    BigInt FromHex(const char* hex)
    {
        BigInt v;
        if (!v.FromHex(hex))
        {
            testing::ReportFailure(__FILE__, __LINE__, std::string("bad hex in a vector: ") + hex);
        }
        return v;
    }

    std::mt19937_64 g_rng(0x4D414E474F53ull);

    BigInt RandomBits(size_t bits)
    {
        std::vector<uint8_t> bytes((bits + 7) / 8);
        for (auto& b : bytes) b = uint8_t(g_rng());
        const unsigned top = unsigned((bits - 1) % 8);
        bytes.back() &= uint8_t((2u << top) - 1);
        bytes.back() |= uint8_t(1u << top);
        return BigInt::FromBytesLE(bytes.data(), bytes.size());
    }

    BigInt OddModulus(size_t bits)
    {
        std::vector<uint8_t> bytes = RandomBits(bits).ToBytesLE();
        bytes[0] |= 1;
        return BigInt::FromBytesLE(bytes.data(), bytes.size());
    }

    LimbVector Padded(const BigInt& v, size_t k)
    {
        LimbVector out(k, 0);
        for (size_t j = 0; j < v.LimbCount(); ++j) out[j] = v.Limbs()[j];
        return out;
    }

    /// The definition of the product: z = x * y * R^-1 mod m  <=>  z * R = x * y (mod m).
    void CheckKernel(MontMulFn kernel, const MontgomeryContext& ctx, const BigInt& x, const BigInt& y)
    {
        const size_t k = ctx.Limbs();
        const BigInt m = ctx.ModulusValue();
        LimbVector xl = Padded(x, k), yl = Padded(y, k), zl(k);
        kernel(zl.data(), xl.data(), yl.data(), ctx.Modulus(), ctx.N0Inv(), k);
        const BigInt z = BigInt::FromLimbs(zl.data(), k);
        CHECK(z < m);
        const BigInt lhs = (z << (LimbBits * k)) % m;
        const BigInt rhs = (x * y) % m;
        CHECK_STR(lhs.ToHex(), rhs.ToHex());
        // aliasing: z may be x
        LimbVector alias = xl;
        kernel(alias.data(), alias.data(), yl.data(), ctx.Modulus(), ctx.N0Inv(), k);
        CHECK(std::memcmp(alias.data(), zl.data(), k * LimbBytes) == 0);
    }

    [[noreturn]] void Throw(const char* what) { throw std::runtime_error(what ? what : ""); }
    struct FatalScope
    {
        FatalHandler previous;
        FatalScope() : previous(SetFatalHandler(&Throw)) {}
        ~FatalScope() { SetFatalHandler(previous); }
    };
}

TEST(CryptoMontgomery_context_constants)
{
    for (const char* hex : { "894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7", golden::kRsaModulus })
    {
        const BigInt m = FromHex(hex);
        MontgomeryContext ctx(m);
        const size_t k = ctx.Limbs();
        CHECK_EQ(k, m.LimbCount());
        // n0inv * m[0] = -1 mod 2^64
        CHECK_EQ(ctx.N0Inv() * m.Limbs()[0] + 1, uint64_t(0));
        const BigInt R = BigInt(1) << (LimbBits * k);
        CHECK_STR(BigInt::FromLimbs(ctx.One(), k).ToHex(), (R % m).ToHex());
        CHECK_STR(BigInt::FromLimbs(ctx.R2(), k).ToHex(), ((R * R) % m).ToHex());
        CHECK_STR(ctx.ModulusValue().ToHex(), m.ToHex());
        // to and from Montgomery form round-trip, reducing on the way in
        LimbVector z(k);
        const BigInt wide = RandomBits(LimbBits * k * 2);
        ctx.ToMont(z.data(), wide);
        CHECK_STR(ctx.FromMont(z.data()).ToHex(), (wide % m).ToHex());
        ctx.ToMont(z.data(), BigInt(1));
        CHECK(std::memcmp(z.data(), ctx.One(), k * LimbBytes) == 0);
    }
}

TEST(CryptoMontgomery_even_or_zero_modulus_is_fatal)
{
    FatalScope scope;
    bool threw = false;
    try { MontgomeryContext ctx(BigInt(10)); (void)ctx; } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    threw = false;
    const BigInt zero;
    try { MontgomeryContext ctx(zero); (void)ctx; } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    threw = false;
    const BigInt one(1);
    try { MontgomeryContext ctx(one); (void)ctx; } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);   // R^2 mod 1 has no Montgomery form worth building
}

namespace
{
    // A context built during static initialisation, before main: the kernel it
    // captures must already be selected (a review found the dispatch pointer used to
    // be a dynamically-initialised global).
    const BigInt g_staticModulus = [] { BigInt n; n.FromHex("894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7"); return n; }();
    const MontgomeryContext g_staticContext(g_staticModulus);
}

TEST(CryptoMontgomery_squaring_agrees_with_multiplication_on_every_tier)
{
    // The assembly tier squares with its own listing at 16 and 32 limbs (the triangle
    // doubled, the diagonal, the in-place reduction); every tier's squaring must equal
    // the portable multiplication of x by itself, on random operands and at the edges.
    std::vector<MontSqrFn> squarings = { &MontSqrPortable };
    if (HasMulxTier()) squarings.push_back(&MontSqrMulx);
    if (HasAsmTier()) squarings.push_back(&MontSqrAsm);
    for (size_t k : { size_t(1), size_t(4), size_t(8), size_t(16), size_t(32), size_t(64) })
    {
        const size_t bits = LimbBits * k;
        for (int round = 0; round < 4; ++round)
        {
            const BigInt m = OddModulus(round == 0 ? bits : bits - size_t(g_rng() % 60));
            MontgomeryContext ctx(m);
            std::vector<BigInt> operands;
            for (int i = 0; i < 24; ++i)
            {
                operands.push_back(RandomBits(m.BitLength()) % m);
            }
            operands.push_back(m - BigInt(1));
            operands.push_back(BigInt(0));
            operands.push_back(BigInt(1));
            operands.push_back(BigInt(2));
            if (m > BigInt(0x10000))
            {
                operands.push_back(m - BigInt(0x1234));
                operands.push_back((m - BigInt(1)) >> 1);
            }
            for (MontSqrFn sqr : squarings)
            {
                if (sqr == &MontSqrAsm && k % 4 != 0)
                {
                    continue;
                }
                for (const BigInt& x : operands)
                {
                    LimbVector xl = Padded(x, k), a(k), b(k);
                    sqr(a.data(), xl.data(), ctx.Modulus(), ctx.N0Inv(), k);
                    MontMulPortable(b.data(), xl.data(), xl.data(), ctx.Modulus(), ctx.N0Inv(), k);
                    CHECK(std::memcmp(a.data(), b.data(), k * LimbBytes) == 0);
                    // and the definition: a * R = x * x mod m
                    const BigInt R = BigInt(1) << (LimbBits * k);
                    CHECK((BigInt::FromLimbs(a.data(), k) * R) % m == (x * x) % m);
                }
            }
            // the context squares with the tier it multiplies with
            LimbVector xl = Padded(operands[0], k), viaContext(k), viaKernel(k);
            ctx.Sqr(viaContext.data(), xl.data());
            MontSqrFor(k)(viaKernel.data(), xl.data(), ctx.Modulus(), ctx.N0Inv(), k);
            CHECK(std::memcmp(viaContext.data(), viaKernel.data(), k * LimbBytes) == 0);
        }
    }
    // the selector follows the multiplication's tier (which the environment may force down)
    if (ActiveMontMul() == &MontMulAsm)
    {
        CHECK(MontSqrFor(16) == &MontSqrAsm);
        CHECK(MontSqrFor(32) == &MontSqrAsm);
    }
    CHECK(MontSqrFor(3) != &MontSqrAsm);
    CHECK((MontSqrFor(16) == &MontSqrAsm) == (MontMulFor(16) == &MontMulAsm));
    CHECK((MontSqrFor(8) == &MontSqrMulx) == (MontMulFor(8) == &MontMulMulx));
}

TEST(CryptoMontgomery_a_context_built_before_main_works)
{
    CHECK_EQ(g_staticContext.Limbs(), size_t(4));
    CHECK_STR(ModExp(BigInt(7), BigInt(2), g_staticContext).ToHex(), "31");
    const BigInt R = BigInt(1) << 256;
    CHECK_STR(BigInt::FromLimbs(g_staticContext.One(), 4).ToHex(), (R % g_staticModulus).ToHex());
}

TEST(CryptoMontgomery_portable_kernel_matches_the_definition)
{
    for (size_t k : { size_t(1), size_t(2), size_t(4), size_t(8), size_t(16), size_t(32), size_t(64) })
    {
        const size_t bits = LimbBits * k;
        for (int round = 0; round < 6; ++round)
        {
            const BigInt m = OddModulus(round == 0 ? bits : bits - size_t(g_rng() % 60));
            MontgomeryContext ctx(m);
            for (int i = 0; i < 8; ++i)
            {
                CheckKernel(&MontMulPortable, ctx, RandomBits(m.BitLength()) % m, RandomBits(m.BitLength()) % m);
            }
            // the edge patterns
            const BigInt mMinusOne = m - BigInt(1);
            CheckKernel(&MontMulPortable, ctx, mMinusOne, mMinusOne);
            CheckKernel(&MontMulPortable, ctx, mMinusOne, BigInt(1));
            CheckKernel(&MontMulPortable, ctx, BigInt(0), mMinusOne);
            CheckKernel(&MontMulPortable, ctx, BigInt(1), BigInt(1));
            const BigInt same = RandomBits(m.BitLength() - 1);
            CheckKernel(&MontMulPortable, ctx, same, same);
            if (m > BigInt(0x10000))
            {
                CheckKernel(&MontMulPortable, ctx, m - BigInt(0x1234), m - BigInt(0x5678));
            }
        }
        // a modulus whose top limb is exactly 0x8000000000000000, and one whose top limb is 1
        LimbVector limbs(k);
        for (auto& l : limbs) l = g_rng();
        limbs[k - 1] = Limb(1) << 63;
        limbs[0] |= 1;
        {
            const BigInt m = BigInt::FromLimbs(limbs.data(), k);
            MontgomeryContext ctx(m);
            CheckKernel(&MontMulPortable, ctx, m - BigInt(1), m - BigInt(2));
            CheckKernel(&MontMulPortable, ctx, RandomBits(bits - 1), RandomBits(bits - 1));
        }
        if (k > 1)
        {
            limbs[k - 1] = 1;
            const BigInt m = BigInt::FromLimbs(limbs.data(), k);
            MontgomeryContext ctx(m);
            CheckKernel(&MontMulPortable, ctx, m - BigInt(1), m - BigInt(1));
            CheckKernel(&MontMulPortable, ctx, RandomBits(m.BitLength()) % m, RandomBits(m.BitLength()) % m);
        }
    }
}

TEST(CryptoMontgomery_assembly_tier_runs_where_it_was_built)
{
    // Which kernel this run exercised is part of the test output, so a log shows it;
    // and where the assembly tier was compiled in and the CPU can run it, it must be
    // the active one -- a silent fall-back to a C++ tier is a wiring bug, not a pass.
    const bool forced = std::getenv("MANGOS_CRYPTO_TIER") != nullptr;
    std::printf("    tier: %s (assembly compiled in: %s; cpu bmi2+adx: %s; forced by environment: %s)\n",
                ActiveTierName(), AssemblyTierCompiledIn() ? "yes" : "no", HasAsmTier() ? "yes" : "no", forced ? "yes" : "no");
    if (AssemblyTierCompiledIn() && !HasAsmTier())
    {
        std::printf("    the assembly tier was built but this CPU cannot run it: not exercised in this run\n");
    }
    if (AssemblyTierCompiledIn() && HasAsmTier() && !forced)
    {
        CHECK_STR(ActiveTierName(), "asm");
        CHECK(ActiveMontMul() == &MontMulAsm);
    }
    if (!AssemblyTierCompiledIn())
    {
        CHECK(!HasAsmTier());
        CHECK(ActiveMontMul() != &MontMulAsm);
    }
}

TEST(CryptoMontgomery_tiers_agree)
{
    const MontMulFn active = ActiveMontMul();
    CHECK(active == &MontMulPortable || active == &MontMulMulx || active == &MontMulAsm);
    const char* name = ActiveTierName();
    CHECK(std::strcmp(name, "portable") == 0 || std::strcmp(name, "mulx") == 0 || std::strcmp(name, "asm") == 0);
    CHECK((active == &MontMulAsm) == (std::strcmp(name, "asm") == 0));
    if (HasAsmTier())
    {
        // the assembly tier takes multiples of 4 limbs; other widths go down a tier
        CHECK(MontMulFor(16) == active);
        CHECK(MontMulFor(3) != &MontMulAsm);
        CHECK(MontMulFor(1) != &MontMulAsm);
    }
    std::vector<MontMulFn> others;
    if (HasMulxTier()) others.push_back(&MontMulMulx);
    if (HasAsmTier()) others.push_back(&MontMulAsm);
    if (others.empty())
    {
        return;
    }
    for (size_t k : { size_t(1), size_t(4), size_t(8), size_t(16), size_t(32), size_t(64) })
    {
        const size_t bits = LimbBits * k;
        for (int round = 0; round < 4; ++round)
        {
            const BigInt m = OddModulus(round == 0 ? bits : bits - size_t(g_rng() % 60));
            MontgomeryContext ctx(m);
            for (MontMulFn other : others)
            {
                if (other == &MontMulAsm && k % 4 != 0)
                {
                    continue;
                }
                for (int i = 0; i < 16; ++i)
                {
                    const BigInt x = RandomBits(m.BitLength()) % m, y = RandomBits(m.BitLength()) % m;
                    LimbVector xl = Padded(x, k), yl = Padded(y, k), a(k), b(k);
                    MontMulPortable(a.data(), xl.data(), yl.data(), ctx.Modulus(), ctx.N0Inv(), k);
                    other(b.data(), xl.data(), yl.data(), ctx.Modulus(), ctx.N0Inv(), k);
                    CHECK(std::memcmp(a.data(), b.data(), k * LimbBytes) == 0);
                    CheckKernel(other, ctx, x, y);
                }
                CheckKernel(other, ctx, m - BigInt(1), m - BigInt(1));
                CheckKernel(other, ctx, BigInt(0), BigInt(0));
                CheckKernel(other, ctx, BigInt(1), m - BigInt(1));
                if (m > BigInt(0x10000))
                {
                    CheckKernel(other, ctx, m - BigInt(0x1234), m - BigInt(0x5678));
                }
            }
        }
    }
}

TEST(CryptoModExp_reproduces_the_goldens)
{
    const size_t count = sizeof(golden::kBigInt) / sizeof(golden::kBigInt[0]);
    for (size_t i = 0; i < count; ++i)
    {
        const golden::BigIntCase& c = golden::kBigInt[i];
        const BigInt a = FromHex(c.a), b = FromHex(c.b), n = FromHex(c.n);
        CHECK_STR(ModExp(a, b, n).ToHex(), c.modexp);
    }
}

TEST(CryptoModExp_matches_python)
{
    for (const vectors::ModExpVector& v : vectors::kModExp)
    {
        const BigInt base = FromHex(v.base), e = FromHex(v.exp), m = FromHex(v.mod);
        CHECK_STR(ModExp(base, e, m).ToHex(), v.result);
        CHECK_STR(ModExp(base, e, m, ExponentKind::Public).ToHex(), v.result);
        if (m.IsOdd() && m.LimbCount() >= 2)
        {
            MontgomeryContext ctx(m);
            // the secret path with the field width declared (the vectors have exponents wider than the modulus)
            const size_t width = std::max(ctx.Limbs(), e.LimbCount());
            CHECK_STR(ModExp(base, e, ctx, ExponentKind::Secret, width).ToHex(), v.result);
            CHECK_STR(ModExp(base, e, ctx, ExponentKind::Secret, width + 3).ToHex(), v.result);   // a wider declared width changes nothing but the work
            CHECK_STR(ModExp(base, e, ctx, ExponentKind::Public).ToHex(), v.result);
            if (e.LimbCount() <= ctx.Limbs())
            {
                CHECK_STR(ModExp(base, e, ctx).ToHex(), v.result);   // the default width: the modulus width
            }
        }
    }
}

TEST(CryptoModExp_special_exponents_and_moduli)
{
    const BigInt N = FromHex("894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7");
    MontgomeryContext ctx(N);
    const BigInt g(7);
    CHECK_STR(ModExp(g, BigInt(0), ctx).ToHex(), "01");
    CHECK_STR(ModExp(g, BigInt(1), ctx).ToHex(), "07");
    CHECK_STR(ModExp(g, BigInt(2), ctx).ToHex(), "31");
    CHECK_STR(ModExp(BigInt(0), BigInt(5), ctx).ToHex(), "0");
    CHECK_STR(ModExp(N, BigInt(5), ctx).ToHex(), "0");          // base = modulus reduces to zero
    CHECK_STR(ModExp(N + BigInt(7), BigInt(3), ctx).ToHex(), ModExp(g, BigInt(3), ctx).ToHex());
    // an exponent wider than the modulus: through the general API, and through the
    // context with its width declared; without a width the secret path refuses it
    const BigInt wideExp = RandomBits(600);
    CHECK_STR(ModExp(g, wideExp, ctx, ExponentKind::Secret, 10).ToHex(), ModExp(g, wideExp, N).ToHex());
    CHECK_STR(ModExp(g, wideExp, ctx, ExponentKind::Public).ToHex(), ModExp(g, wideExp, N).ToHex());
    {
        FatalScope wide;
        bool threw = false;
        try { BigInt r = ModExp(g, wideExp, ctx); (void)r; } catch (const std::runtime_error&) { threw = true; }
        CHECK(threw);
    }
    // a two-limb, an even, a single-limb and the unit modulus through the general API
    CHECK_STR(ModExp(BigInt(3), BigInt(4), BigInt(100)).ToHex(), "51");        // 81 mod 100
    CHECK_STR(ModExp(BigInt(3), BigInt(100), BigInt(1)).ToHex(), "0");
    CHECK_STR(ModExp(BigInt(2), BigInt(10), BigInt(1000)).ToHex(), "18");      // 1024 mod 1000 = 24
    FatalScope scope;
    bool threw = false;
    try { BigInt r = ModExp(g, BigInt(3), BigInt()); (void)r; } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
}

TEST(CryptoModInverse_matches_python)
{
    for (const vectors::InvVector& v : vectors::kInv)
    {
        const BigInt a = FromHex(v.a), m = FromHex(v.m);
        const BigInt inv = ModInverse(a, m);
        CHECK_STR(inv.ToHex(), v.inv);
        if (!inv.IsZero())
        {
            CHECK_STR(((a * inv) % m).ToHex(), "01");
        }
    }
    CHECK(ModInverse(BigInt(3), BigInt(1)).IsZero());
    CHECK(ModInverse(BigInt(3), BigInt(0)).IsZero());
    CHECK(ModInverse(BigInt(0), BigInt(7)).IsZero());
}
