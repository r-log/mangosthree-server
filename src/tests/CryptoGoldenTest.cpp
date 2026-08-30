/**
 * @file
 * @brief The golden vectors captured before the switch, replayed through the wrappers.
 *
 * `CryptoGoldenVectors.h` holds what this repository's own wrappers computed, on the
 * library they used to sit on, for fixed inputs: big-integer arithmetic at every shape the server uses
 * (SRP6's 256-bit modulus, the redirect's 2048-bit one, the division edge cases a
 * replacement gets wrong first), the packet cipher's keystreams, an HMAC over a key
 * that must be padded, and raw RSA signatures. These tests passed on the old build by
 * construction; their job is to keep passing now that the wrappers are re-implemented.
 */

#include "TestHarness.h"
#include "CryptoGoldenVectors.h"

#include "Auth/AuthCrypt.h"
#include "Auth/BigNumber.h"
#include "Auth/HMACSHA1.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    std::string Hex(BigNumber& v)
    {
        return std::string(v.AsHexStr());
    }

    // CHECK_HEX renders bytes in lowercase; the captured table is uppercase (AsHexStr's case).
    std::string Lower(const char* hex)
    {
        std::string s(hex);
        for (auto& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    BigNumber FromHex(const char* hex)
    {
        BigNumber v;
        v.SetHexStr(hex);
        return v;
    }

    std::vector<uint8> BytesFromHex(const char* hex)
    {
        std::vector<uint8> out;
        for (const char* p = hex; p[0] && p[1]; p += 2)
        {
            auto nibble = [](char c) -> uint8 {
                if (c >= '0' && c <= '9') return uint8(c - '0');
                if (c >= 'A' && c <= 'F') return uint8(c - 'A' + 10);
                return uint8(c - 'a' + 10);
            };
            out.push_back(uint8((nibble(p[0]) << 4) | nibble(p[1])));
        }
        return out;
    }

    // BigNumber::SetBinary reads little-endian; the RSA quantities are big-endian.
    void SetBigEndian(BigNumber& number, const uint8* data, size_t length)
    {
        std::vector<uint8> reversed(data, data + length);
        std::reverse(reversed.begin(), reversed.end());
        number.SetBinary(reversed.data(), int(reversed.size()));
    }
}

TEST(Golden_bigint_cases_match_the_capture)
{
    const size_t count = sizeof(golden::kBigInt) / sizeof(golden::kBigInt[0]);
    CHECK(count > 150);
    for (size_t i = 0; i < count; ++i)
    {
        const golden::BigIntCase& c = golden::kBigInt[i];
        BigNumber a = FromHex(c.a), b = FromHex(c.b), n = FromHex(c.n);
        // hex round trip: minimal, uppercase
        CHECK_STR(Hex(a).c_str(), c.a);
        CHECK_STR(Hex(b).c_str(), c.b);
        CHECK_STR(Hex(n).c_str(), c.n);
        BigNumber sum = a + b;
        CHECK_STR(Hex(sum).c_str(), c.sum);
        BigNumber product = a * b;
        CHECK_STR(Hex(product).c_str(), c.product);
        BigNumber quotient = a / n;
        CHECK_STR(Hex(quotient).c_str(), c.quotient);
        BigNumber remainder = a % n;
        CHECK_STR(Hex(remainder).c_str(), c.remainder);
        BigNumber modexp = a.ModExp(b, n);
        CHECK_STR(Hex(modexp).c_str(), c.modexp);
        // the invariants a = q*n + r and r < n, checked in code
        BigNumber rebuilt = quotient * n + remainder;
        CHECK_STR(Hex(rebuilt).c_str(), c.a);
        BigNumber rAgain = remainder % n;
        CHECK_STR(Hex(rAgain).c_str(), c.remainder);
    }
}

TEST(Golden_authcrypt_keystreams_match_the_capture)
{
    BigNumber K = FromHex(golden::kAuthCrypt.K);
    uint8 send[64], recv[64];
    std::memset(send, 0, sizeof send);
    std::memset(recv, 0, sizeof recv);
    AuthCrypt enc;
    enc.Init(&K);
    enc.EncryptSend(send, 64);
    CHECK_HEX(send, 64, Lower(golden::kAuthCrypt.serverEncrypt));
    AuthCrypt dec;
    dec.Init(&K);
    dec.DecryptRecv(recv, 64);
    CHECK_HEX(recv, 64, Lower(golden::kAuthCrypt.clientDecrypt));
}

TEST(Golden_hmac_over_a_short_key_at_its_full_width)
{
    BigNumber K = FromHex(golden::kHmacShortKey.K);
    CHECK_EQ(K.GetNumBytes(), 39);
    std::vector<uint8> data = BytesFromHex(golden::kHmacShortKey.data);
    HMACSHA1 h(40, K.AsByteArray(40));
    h.UpdateData(data.data(), int(data.size()));
    h.Finalize();
    CHECK_HEX(h.GetDigest(), 20, Lower(golden::kHmacShortKey.hmac));
}

TEST(Golden_raw_rsa_signatures_match_the_capture)
{
    BigNumber n = FromHex(golden::kRsaModulus), d = FromHex(golden::kRsaPrivateExponent), e(65537u);
    CHECK_EQ(n.GetNumBytes(), 256);
    const size_t count = sizeof(golden::kRsa) / sizeof(golden::kRsa[0]);
    for (size_t i = 0; i < count; ++i)
    {
        std::vector<uint8> m = BytesFromHex(golden::kRsa[i].message);
        REQUIRE(m.size() == 256);
        BigNumber message;
        SetBigEndian(message, m.data(), m.size());
        BigNumber signature = message.ModExp(d, n);
        CHECK_HEX(signature.AsByteArray(256, false), 256, Lower(golden::kRsa[i].signature));
        BigNumber recovered = signature.ModExp(e, n);
        CHECK_HEX(recovered.AsByteArray(256, false), 256, Lower(golden::kRsa[i].message));
    }
}

TEST(Golden_setrand_has_exactly_the_requested_bits_with_both_ends_set)
{
    // The most significant bit set, odd -- the shape the SRP6 ephemerals and salts have
    // relied on. 200 draws per width.
    for (int bits : { 32, 128, 152, 256 })
    {
        for (int i = 0; i < 200; ++i)
        {
            BigNumber r;
            r.SetRand(bits);
            CHECK_EQ(r.GetNumBytes(), bits / 8);
            uint8* be = r.AsByteArray(bits / 8, false);
            CHECK((be[0] & 0x80) != 0);
            CHECK((be[bits / 8 - 1] & 0x01) != 0);
        }
    }
}
