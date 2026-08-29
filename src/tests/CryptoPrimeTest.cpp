/**
 * @file
 * @brief Primality and RSA key generation.
 *
 * The verdicts come from three places: numbers whose status is a matter of record
 * (strong pseudoprimes to base 2, Carmichael numbers, Mersenne primes), Python's
 * Miller-Rabin over random 256/512/1024-bit odd numbers and primes (PrimeVectors.h),
 * and the test key's own primes. Then the generator: its primes have the width, the
 * interval and the coprimality FIPS 186-4 asks for, and a generated pair loads,
 * signs and verifies, with every CRT relation holding.
 */

#include "TestHarness.h"
#include "PrimeVectors.h"
#include "RsaTestKey.h"

#include "Crypto/BigInt.h"
#include "Crypto/Fatal.h"
#include "Crypto/ModExp.h"
#include "Crypto/Prime.h"
#include "Crypto/Rsa.h"
#include "Crypto/SystemRandom.h"

#include <stdexcept>
#include <string>
#include <vector>

using namespace MaNGOS::Crypto;

namespace
{
    BigInt H(const char* hex)
    {
        BigInt v;
        v.FromHex(hex);
        return v;
    }
}

TEST(CryptoPrime_gcd)
{
    CHECK(Gcd(BigInt(12), BigInt(18)) == BigInt(6));
    CHECK(Gcd(BigInt(17), BigInt(31)) == BigInt(1));
    CHECK(Gcd(BigInt(0), BigInt(9)) == BigInt(9));
    CHECK(Gcd(BigInt(9), BigInt(0)) == BigInt(9));
    const BigInt p = H(testkey::kPrime1), q = H(testkey::kPrime2);
    CHECK(Gcd(p * q, p) == p);
    CHECK(Gcd(p - BigInt(1), BigInt(65537)) == BigInt(1));
}

TEST(CryptoPrime_small_prime_sieve_matches_python)
{
    // every odd number below 10000: the sieve passes exactly the primes (a composite
    // this small always has a factor below 2^16), and passes a small prime itself
    unsigned passed = 0;
    for (unsigned i = 0; i < 5000; ++i)
    {
        const unsigned n = 2 * i + 1;
        const bool expected = (vectors::kSievePassBits[i / 32] >> (i % 32)) & 1;
        const bool actual = PassesSmallPrimeSieve(BigInt(n));
        if (actual != expected)
        {
            testing::ReportFailure(__FILE__, __LINE__, "sieve verdict for " + std::to_string(n));
        }
        passed += actual;
    }
    CHECK(passed > 1200);   // 1229 primes below 10000
    CHECK(!PassesSmallPrimeSieve(BigInt(0)));
    CHECK(!PassesSmallPrimeSieve(BigInt(1)));
    CHECK(PassesSmallPrimeSieve(BigInt(2)));
    CHECK(PassesSmallPrimeSieve(BigInt(65521)));                   // the largest prime below 2^16
    CHECK(!PassesSmallPrimeSieve(BigInt(65521) * BigInt(65521)));
    CHECK(PassesSmallPrimeSieve(BigInt(65537)));                   // no factor below 2^16
    CHECK(PassesSmallPrimeSieve(BigInt(65537) * BigInt(65539)));   // nor here: the sieve is only a filter
    CHECK(!PassesSmallPrimeSieve(H(testkey::kPrime1) * BigInt(3)));
    CHECK(PassesSmallPrimeSieve(H(testkey::kPrime1)));
}

TEST(CryptoPrime_miller_rabin_matches_the_record_and_python)
{
    for (const vectors::PrimeVector& v : vectors::kPrimality)
    {
        const BigInt n = H(v.n);
        if (!n.IsOdd() || n <= BigInt(3))
        {
            continue;   // outside the function's contract; the sieve test covers them
        }
        const bool verdict = IsProbablePrime(n, 64, SystemRandom::Instance());
        if (verdict != v.prime)
        {
            testing::ReportFailure(__FILE__, __LINE__, std::string("primality of ") + v.n);
        }
    }
    // the test key's primes are prime, their product is not
    const BigInt p = H(testkey::kPrime1), q = H(testkey::kPrime2);
    CHECK(IsProbablePrime(p, 64, SystemRandom::Instance()));
    CHECK(IsProbablePrime(q, 64, SystemRandom::Instance()));
    CHECK(!IsProbablePrime(p * q, 64, SystemRandom::Instance()));
    CHECK(!IsProbablePrime(p * BigInt(7), 8, SystemRandom::Instance()));
    // one round is enough to reject most composites; none is enough to reject nothing
    CHECK(!IsProbablePrime(BigInt(2047) * BigInt(3), 4, SystemRandom::Instance()));
}

TEST(CryptoPrime_generated_primes_have_the_fips_shape)
{
    const BigInt e(65537);
    for (size_t bits : { size_t(128), size_t(256), size_t(512) })
    {
        for (int i = 0; i < 3; ++i)
        {
            const BigInt p = GeneratePrime(bits, e, SystemRandom::Instance(), 16);
            CHECK_EQ(p.BitLength(), bits);
            CHECK(p.IsOdd());
            CHECK_EQ((p * p).BitLength(), 2 * bits);          // above sqrt(2) * 2^(bits - 1)
            CHECK(Gcd(p - BigInt(1), e) == BigInt(1));
            CHECK(PassesSmallPrimeSieve(p));
            CHECK(IsProbablePrime(p, 16, SystemRandom::Instance()));
        }
    }
}

TEST(CryptoRsa_generated_keys_are_consistent_and_sign)
{
    const BigInt e(65537);
    for (size_t bits : { size_t(1024), size_t(2048) })
    {
        RsaKeyPair pair;
        REQUIRE(RsaGenerateKey(bits, e, SystemRandom::Instance(), pair, 16));
        CHECK_EQ(pair.n.BitLength(), bits);
        CHECK_EQ(pair.p.BitLength(), bits / 2);
        CHECK_EQ(pair.q.BitLength(), bits / 2);
        CHECK(pair.p > pair.q);
        CHECK(pair.p * pair.q == pair.n);
        CHECK((pair.p - pair.q).BitLength() > bits / 2 - 100);
        CHECK(pair.d.BitLength() > bits / 2);
        const BigInt pm1 = pair.p - BigInt(1), qm1 = pair.q - BigInt(1);
        const BigInt lambda = (pm1 * qm1) / Gcd(pm1, qm1);
        CHECK((e * pair.d) % lambda == BigInt(1));
        CHECK((e * pair.dP) % pm1 == BigInt(1));
        CHECK((e * pair.dQ) % qm1 == BigInt(1));
        CHECK((pair.q * pair.qInv) % pair.p == BigInt(1));
        CHECK(pair.dP == pair.d % pm1);
        CHECK(pair.dQ == pair.d % qm1);

        // it loads both ways, signs, verifies, and both paths agree
        RsaPrivateKey crt, plain;
        REQUIRE(crt.Load(pair.n, pair.e, pair.d, pair.p, pair.q, pair.dP, pair.dQ, pair.qInv));
        REQUIRE(plain.Load(pair.n, pair.e, pair.d));
        std::vector<uint8_t> message(bits / 8), s1, s2, recovered;
        for (int i = 0; i < 4; ++i)
        {
            SystemRandom::Instance().FillExact(message.data(), message.size());
            message[0] = 0x00;
            CHECK(crt.SignRaw(message.data(), message.size(), s1, SystemRandom::Instance()));
            CHECK(plain.SignRaw(message.data(), message.size(), s2, SystemRandom::Instance()));
            CHECK(s1 == s2);
            CHECK(RsaVerifyRaw(crt.Public(), s1.data(), s1.size(), recovered));
            CHECK(recovered == message);
        }
        // the parameters render as whole-byte hex of the expected widths
        CHECK_EQ(pair.n.ToHex().size(), bits / 4);
        CHECK_EQ(pair.p.ToHex().size(), bits / 8);
    }
    // the contract's edges
    RsaKeyPair bad;
    CHECK(!RsaGenerateKey(512, e, SystemRandom::Instance(), bad));        // below 1024
    CHECK(!RsaGenerateKey(1280, e, SystemRandom::Instance(), bad));       // not a multiple of 512
    CHECK(!RsaGenerateKey(1024, BigInt(4), SystemRandom::Instance(), bad));   // even e
}
