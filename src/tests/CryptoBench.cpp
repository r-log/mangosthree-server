/**
 * @file
 * @brief `mangos_tests --bench crypto`: the in-house primitives timed the way the
 * pre-switch baseline was (medians of N runs, same shapes), printed as a markdown table.
 *
 * Not a test: nothing is asserted. The numbers go into the pull request next to the
 * baseline captured on the old build, and decide nothing by themselves -- the
 * assembly tier is written regardless; this is what it is compared with.
 */

#include "RsaTestKey.h"

#include "Crypto/BigInt.h"
#include "Crypto/HmacSha1.h"
#include "Crypto/ModExp.h"
#include "Crypto/Montgomery.h"
#include "Crypto/Rsa.h"
#include "Crypto/Sha1.h"
#include "Crypto/SystemRandom.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <vector>

using namespace MaNGOS::Crypto;

namespace
{
    using Clock = std::chrono::steady_clock;

    double MedianMicros(int runs, const std::function<void()>& fn)
    {
        std::vector<double> samples;
        samples.reserve(runs);
        for (int i = 0; i < runs; ++i)
        {
            const auto t0 = Clock::now();
            fn();
            const auto t1 = Clock::now();
            samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        std::sort(samples.begin(), samples.end());
        return samples[samples.size() / 2];
    }

    BigInt H(const char* hex)
    {
        BigInt v;
        v.FromHex(hex);
        return v;
    }
}

void RunCryptoBench()
{
    std::printf("\n## In-house crypto (tier: %s)\n\n", ActiveTierName());
    std::printf("| operation | median | runs |\n|---|---|---|\n");

    RsaPrivateKey plain, crt;
    plain.Load(H(testkey::kModulus), H(testkey::kPublicExponent), H(testkey::kPrivateExponent));
    crt.Load(H(testkey::kModulus), H(testkey::kPublicExponent), H(testkey::kPrivateExponent),
             H(testkey::kPrime1), H(testkey::kPrime2), H(testkey::kExponent1), H(testkey::kExponent2), H(testkey::kCoefficient));
    std::vector<uint8_t> msg(256, 0x5A), sig, out;
    msg[0] = 0;
    SystemRandom& random = SystemRandom::Instance();

    double t = MedianMicros(200, [&] { crt.SignRaw(msg.data(), msg.size(), sig, random); });
    std::printf("| RSA-2048 private op, CRT + blinding (`RsaPrivateKey::SignRaw`) | %.0f us | 200 |\n", t);
    t = MedianMicros(50, [&] { plain.SignRaw(msg.data(), msg.size(), sig, random); });
    std::printf("| RSA-2048 private op, no CRT, blinding (`SignRaw` on n, d only) | %.0f us | 50 |\n", t);
    t = MedianMicros(200, [&] { RsaVerifyRaw(crt.Public(), sig.data(), sig.size(), out); });
    std::printf("| RSA-2048 public op, e = 65537 (`RsaVerifyRaw`) | %.1f us | 200 |\n", t);
    {
        const BigInt m = H(testkey::kModulus), d = H(testkey::kPrivateExponent);
        MontgomeryContext ctx(m);
        const BigInt base = BigInt::FromBytesBE(msg.data(), msg.size());
        t = MedianMicros(50, [&] { BigInt r = ModExp(base, d, ctx); (void)r; });
        std::printf("| RSA-2048 private op, no CRT, no blinding (`ModExp` over the context) | %.0f us | 50 |\n", t);
        const BigInt e(65537);
        t = MedianMicros(200, [&] { BigInt r = ModExp(base, e, ctx, ExponentKind::Public); (void)r; });
        std::printf("| `ModExp` with the public exponent (17 bits, walked at its own length) | %.1f us | 200 |\n", t);
        BigInt rr = SystemRandom::Instance().Below(m);
        t = MedianMicros(50, [&] { BigInt inv = ModInverse(rr, m); (void)inv; });
        std::printf("| `ModInverse` of a 2048-bit blinding factor (extended Euclid) | %.0f us | 50 |\n", t);
        MontgomeryContext pctx(H(testkey::kPrime1));
        const BigInt dP = H(testkey::kExponent1);
        t = MedianMicros(100, [&] { BigInt r = ModExp(base, dP, pctx); (void)r; });
        std::printf("| one CRT half: 1024-bit `ModExp` (16 limbs) | %.0f us | 100 |\n", t);
        const size_t k = ctx.Limbs();
        LimbVector a(k), b(k), z(k);
        ctx.ToMont(a.data(), base);
        ctx.ToMont(b.data(), d);
        t = MedianMicros(2000, [&] { for (int i = 0; i < 100; ++i) ctx.Mul(z.data(), a.data(), b.data()); });
        std::printf("| `montmul`, 32 limbs (2048-bit), per call | %.3f us | 2000 x 100 |\n", t / 100.0);
        MontgomeryContext half(H(testkey::kPrime1));
        const size_t kh = half.Limbs();
        LimbVector ah(kh), bh(kh), zh(kh);
        half.ToMont(ah.data(), base);
        half.ToMont(bh.data(), d);
        t = MedianMicros(2000, [&] { for (int i = 0; i < 100; ++i) half.Mul(zh.data(), ah.data(), bh.data()); });
        std::printf("| `montmul`, 16 limbs (1024-bit), per call | %.3f us | 2000 x 100 |\n", t / 100.0);
        t = MedianMicros(2000, [&] { for (int i = 0; i < 100; ++i) ctx.Sqr(z.data(), a.data()); });
        std::printf("| `montsqr`, 32 limbs (2048-bit), per call | %.3f us | 2000 x 100 |\n", t / 100.0);
        t = MedianMicros(2000, [&] { for (int i = 0; i < 100; ++i) half.Sqr(zh.data(), ah.data()); });
        std::printf("| `montsqr`, 16 limbs (1024-bit), per call | %.3f us | 2000 x 100 |\n", t / 100.0);
    }

    // SRP6: v = g^x, B = g^b, S = (A * v^u)^b under N -- three 256-bit exponentiations
    {
        const BigInt N = H("894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7");
        MontgomeryContext ctx(N);
        const BigInt g(7);
        const BigInt x = H("A5E1D9C4B3F2011E8D7C6B5A49382716F5E4D3C2B1A0");
        const BigInt b = H("3C9F1E2D4B5A69788796A5B4C3D2E1F00112");
        const BigInt A = H("12F3E4D5C6B7A8990A1B2C3D4E5F60718293A4B5C6D7E8F90A1B2C3D4E5F6071");
        const BigInt u = H("0F1E2D3C4B5A69788796A5B4C3D2E1F0A1B2C3D4");
        t = MedianMicros(500, [&] {
            BigInt v = ModExp(g, x, ctx);
            BigInt B = (ModExp(g, b, ctx) + v * BigInt(3)) % N;
            BigInt S = ModExp(A * ModExp(v, u, ctx), b, ctx);
            (void)B; (void)S;
        });
        std::printf("| SRP6 login arithmetic (three 256-bit `ModExp`, one context) | %.1f us | 500 |\n", t);
        t = MedianMicros(500, [&] {
            BigInt v = ModExp(g, x, N);
            BigInt S = ModExp(A * ModExp(v, u, N), b, N);
            (void)S;
        });
        std::printf("| the same through `ModExp(base, exp, modulus)` (a context per call) | %.1f us | 500 |\n", t);
    }

    // key generation (offline, once per realm)
    {
        RsaKeyPair pair;
        t = MedianMicros(3, [&] { RsaGenerateKey(2048, BigInt(65537), random, pair); });
        std::printf("| RSA-2048 key generation (`RsaGenerateKey`, 64 Miller-Rabin rounds) | %.0f ms | 3 |\n", t / 1000.0);
    }

    // hashes
    {
        std::vector<uint8_t> buf(1 << 16, 0xA5);
        const int chunks = 1024;   // 64 MiB
        t = MedianMicros(3, [&] { Sha1Core sh; for (int i = 0; i < chunks; ++i) sh.Update(buf.data(), buf.size()); uint8_t o[20]; sh.Finish(o); });
        std::printf("| SHA-1 (`Sha1Core`, 64 MiB in 64 KiB updates) | %.0f MB/s | 3 |\n", 64.0 * 1048576.0 / t);
        std::vector<uint8_t> key(40, 0x3C);
        t = MedianMicros(3, [&] { HmacSha1 hm(key.data(), key.size()); for (int i = 0; i < chunks; ++i) hm.Update(buf.data(), buf.size()); uint8_t o[20]; hm.Finish(o); });
        std::printf("| HMAC-SHA1 (`HmacSha1`, 64 MiB in 64 KiB updates) | %.0f MB/s | 3 |\n", 64.0 * 1048576.0 / t);
    }
    std::printf("\n");
}
