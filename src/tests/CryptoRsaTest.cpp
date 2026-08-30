/**
 * @file
 * @brief Raw RSA: the captured goldens through both private paths, and the key's rules.
 *
 * The four signatures captured under the test pair (CryptoGoldenVectors.h)
 * must come out of the plain path and of the CRT path byte for byte; the CRT
 * parameters are the ones recovered from (n, e, d) in RsaTestKey.h. Then the
 * properties: verify inverts sign, blinding does not change the result, the widths
 * and the bound (message below n) are enforced, and a CRT set that is not consistent
 * with n does not load.
 */

#include "TestHarness.h"
#include "CryptoGoldenVectors.h"
#include "RsaTestKey.h"

#include "Crypto/BigInt.h"
#include "Crypto/Hex.h"
#include "Crypto/Rsa.h"
#include "Crypto/SystemRandom.h"

#include <cctype>
#include <cstring>
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

    std::string Lower(const char* hex)
    {
        std::string s(hex);
        for (auto& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    bool LoadPlain(RsaPrivateKey& key)
    {
        return key.Load(H(testkey::kModulus), H(testkey::kPublicExponent), H(testkey::kPrivateExponent));
    }

    bool LoadCrt(RsaPrivateKey& key)
    {
        return key.Load(H(testkey::kModulus), H(testkey::kPublicExponent), H(testkey::kPrivateExponent),
                        H(testkey::kPrime1), H(testkey::kPrime2), H(testkey::kExponent1), H(testkey::kExponent2), H(testkey::kCoefficient));
    }

    void CheckGoldens(const RsaPrivateKey& key)
    {
        const size_t count = sizeof(golden::kRsa) / sizeof(golden::kRsa[0]);
        for (size_t i = 0; i < count; ++i)
        {
            std::vector<uint8_t> message, signature, recovered;
            REQUIRE(FromHex(golden::kRsa[i].message, message));
            REQUIRE(message.size() == 256);
            CHECK(key.SignRaw(message.data(), message.size(), signature, SystemRandom::Instance()));
            CHECK_HEX(signature.data(), signature.size(), Lower(golden::kRsa[i].signature));
            CHECK(RsaVerifyRaw(key.Public(), signature.data(), signature.size(), recovered));
            CHECK(recovered == message);
        }
    }
}

TEST(CryptoRsa_test_key_is_the_golden_pair_with_consistent_crt_parameters)
{
    const BigInt n = H(testkey::kModulus), e = H(testkey::kPublicExponent), d = H(testkey::kPrivateExponent);
    const BigInt p = H(testkey::kPrime1), q = H(testkey::kPrime2), dP = H(testkey::kExponent1), dQ = H(testkey::kExponent2), qInv = H(testkey::kCoefficient);
    CHECK_STR(n.ToHex(), golden::kRsaModulus);
    CHECK_STR(d.ToHex(), golden::kRsaPrivateExponent);
    CHECK_EQ(e.Low64(), uint64_t(65537));
    CHECK(p * q == n);
    CHECK(p > q);
    CHECK((e * dP) % (p - BigInt(1)) == BigInt(1));
    CHECK((e * dQ) % (q - BigInt(1)) == BigInt(1));
    CHECK((q * qInv) % p == BigInt(1));
    CHECK_EQ(p.BitLength(), size_t(1024));
    CHECK_EQ(q.BitLength(), size_t(1024));
}

TEST(CryptoRsa_plain_signatures_match_the_goldens)
{
    RsaPrivateKey key;
    REQUIRE(LoadPlain(key));
    CHECK(!key.HasCrt());
    CHECK_EQ(key.Bytes(), size_t(256));
    CheckGoldens(key);
}

TEST(CryptoRsa_crt_signatures_match_the_goldens)
{
    RsaPrivateKey key;
    REQUIRE(LoadCrt(key));
    CHECK(key.HasCrt());
    CheckGoldens(key);
}

TEST(CryptoRsa_random_messages_round_trip_and_both_paths_agree)
{
    RsaPrivateKey plain, crt;
    REQUIRE(LoadPlain(plain));
    REQUIRE(LoadCrt(crt));
    std::vector<uint8_t> message(256), sigPlain, sigCrt, recovered;
    for (int i = 0; i < 40; ++i)
    {
        SystemRandom::Instance().FillExact(message.data(), message.size());
        message[0] = 0x00;   // below the modulus, as the redirect guarantees
        CHECK(crt.SignRaw(message.data(), message.size(), sigCrt, SystemRandom::Instance()));
        CHECK_EQ(sigCrt.size(), size_t(256));
        CHECK(RsaVerifyRaw(crt.Public(), sigCrt.data(), sigCrt.size(), recovered));
        CHECK(recovered == message);
        if (i < 8)
        {
            CHECK(plain.SignRaw(message.data(), message.size(), sigPlain, SystemRandom::Instance()));
            CHECK(sigPlain == sigCrt);
        }
    }
    // the blinding factor changes every time; the signature never does
    std::vector<uint8_t> again;
    CHECK(crt.SignRaw(message.data(), message.size(), again, SystemRandom::Instance()));
    CHECK(again == sigCrt);
    // the extreme messages: 1 and the largest 255-byte value
    std::vector<uint8_t> one(256, 0x00);
    one[255] = 0x01;
    CHECK(crt.SignRaw(one.data(), one.size(), sigCrt, SystemRandom::Instance()));
    std::vector<uint8_t> expectOne(256, 0x00);
    expectOne[255] = 0x01;
    CHECK(sigCrt == expectOne);   // 1^d = 1
    std::vector<uint8_t> big(256, 0xFF);
    big[0] = 0x00;
    CHECK(crt.SignRaw(big.data(), big.size(), sigCrt, SystemRandom::Instance()));
    CHECK(RsaVerifyRaw(crt.Public(), sigCrt.data(), sigCrt.size(), recovered));
    CHECK(recovered == big);
}

TEST(CryptoRsa_blinding_pair_stays_consistent_across_refreshes)
{
    // The pair (r^e, r^-1) is squared after every use and drawn afresh every 32
    // signatures: across 70 signatures of the same message -- two refreshes and every
    // squaring in between -- the result must be the golden signature every time.
    RsaPrivateKey key;
    REQUIRE(LoadCrt(key));
    std::vector<uint8_t> message, signature;
    REQUIRE(FromHex(golden::kRsa[0].message, message));
    for (int i = 0; i < 70; ++i)
    {
        CHECK(key.SignRaw(message.data(), message.size(), signature, SystemRandom::Instance()));
        CHECK_HEX(signature.data(), signature.size(), Lower(golden::kRsa[0].signature));
    }
    // reloading resets the pair; the signature is unchanged
    REQUIRE(LoadPlain(key));
    CHECK(key.SignRaw(message.data(), message.size(), signature, SystemRandom::Instance()));
    CHECK_HEX(signature.data(), signature.size(), Lower(golden::kRsa[0].signature));
}

TEST(CryptoRsa_refuses_wrong_widths_and_messages_not_below_n)
{
    RsaPrivateKey key;
    REQUIRE(LoadCrt(key));
    std::vector<uint8_t> signature, recovered;
    std::vector<uint8_t> n = H(testkey::kModulus).ToBytesBE(256);
    CHECK(!key.SignRaw(n.data(), n.size(), signature, SystemRandom::Instance()));             // m = n
    std::vector<uint8_t> above(256, 0xFF);
    CHECK(!key.SignRaw(above.data(), above.size(), signature, SystemRandom::Instance()));     // m > n
    std::vector<uint8_t> shortMessage(255, 0x00);
    CHECK(!key.SignRaw(shortMessage.data(), shortMessage.size(), signature, SystemRandom::Instance()));
    std::vector<uint8_t> longMessage(257, 0x00);
    CHECK(!key.SignRaw(longMessage.data(), longMessage.size(), signature, SystemRandom::Instance()));
    CHECK(!RsaVerifyRaw(key.Public(), n.data(), n.size(), recovered));                        // s = n
    CHECK(!RsaVerifyRaw(key.Public(), shortMessage.data(), shortMessage.size(), recovered));
    RsaPrivateKey unloaded;
    CHECK(!unloaded.Loaded());
    std::vector<uint8_t> zero(256, 0x00);
    CHECK(!unloaded.SignRaw(zero.data(), zero.size(), signature, SystemRandom::Instance()));
    CHECK(!RsaVerifyRaw(unloaded.Public(), zero.data(), zero.size(), recovered));
}

TEST(CryptoRsa_load_rejects_what_is_not_a_key)
{
    RsaPrivateKey key;
    const BigInt n = H(testkey::kModulus), e = H(testkey::kPublicExponent), d = H(testkey::kPrivateExponent);
    const BigInt p = H(testkey::kPrime1), q = H(testkey::kPrime2), dP = H(testkey::kExponent1), dQ = H(testkey::kExponent2), qInv = H(testkey::kCoefficient);
    CHECK(!key.Load(n + BigInt(1), e, d));                                  // even modulus
    CHECK(!key.Load(n, BigInt(2), d));                                      // even exponent
    CHECK(!key.Load(n, BigInt(1), d));                                      // e = 1
    CHECK(!key.Load(n, e, BigInt()));                                       // d = 0
    CHECK(!key.Load(n, e, n));                                              // d >= n
    CHECK(!key.Load(BigInt(0xFFFFFFFFFFFFFFFFull), e, BigInt(3)));          // a single-limb modulus
    CHECK(!key.Load(n, e, d, p, q, dP, dQ, qInv + BigInt(1)));              // wrong coefficient
    CHECK(!key.Load(n, e, d, p, p, dP, dQ, qInv));                          // p * p != n
    CHECK(!key.Load(n, e, d, q, p, dQ, dP, qInv));                          // swapped primes: p > q is required (the recombination relies on m2 < q < p)
    CHECK(!key.Load(n, e, d, p, q, p, dQ, qInv));                           // dP >= p
    CHECK(!key.Load(n, e, BigInt(1), p, q, dP, dQ, qInv));                  // d is not the key's: dP, dQ do not derive from it
    CHECK(!key.Load(n, e, d, p, q, dP + BigInt(2), dQ, qInv));              // dP not d mod (p - 1)
    CHECK(key.Load(n, e, d, p, q, dP, dQ, qInv));                           // and the real set loads
    CHECK(key.HasCrt());
    // reloading without the primes drops the CRT path
    CHECK(key.Load(n, e, d));
    CHECK(!key.HasCrt());
}

TEST(CryptoRsa_a_failed_load_leaves_nothing_loaded)
{
    // A load that returns false must not leave a half-installed key: the review found
    // the public half committed before the private exponent was checked.
    RsaPrivateKey key;
    REQUIRE(LoadCrt(key));
    const BigInt n = H(testkey::kModulus), e = H(testkey::kPublicExponent), d = H(testkey::kPrivateExponent);
    const BigInt p = H(testkey::kPrime1), q = H(testkey::kPrime2), dP = H(testkey::kExponent1), dQ = H(testkey::kExponent2), qInv = H(testkey::kCoefficient);
    std::vector<uint8_t> message(256, 0x5A), signature;
    message[0] = 0;

    CHECK(!key.Load(n, e, BigInt()));                                       // d = 0 after a good key
    CHECK(!key.Loaded());
    CHECK(!key.HasCrt());
    CHECK(!key.SignRaw(message.data(), message.size(), signature, SystemRandom::Instance()));

    REQUIRE(LoadCrt(key));
    CHECK(!key.Load(n, e, d, p, q, dP, dQ, qInv + BigInt(1)));              // a bad CRT set after a good key
    CHECK(!key.Loaded());
    CHECK(!key.SignRaw(message.data(), message.size(), signature, SystemRandom::Instance()));

    REQUIRE(LoadCrt(key));                                                  // and it loads again afterwards
    CHECK(key.SignRaw(message.data(), message.size(), signature, SystemRandom::Instance()));
    key.Unload();
    CHECK(!key.Loaded());
}
