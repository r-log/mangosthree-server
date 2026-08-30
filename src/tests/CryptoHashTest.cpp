/**
 * @file
 * @brief The in-house hashes against their published vectors and the captured goldens.
 *
 * SHA-1 and SHA-256: FIPS 180 examples and the NIST million-'a' samples. MD5: the
 * RFC 1321 test suite. HMAC-SHA1: RFC 2202 cases 1-7 (short key, block-size key,
 * larger-than-block key). Then the properties a streaming implementation gets wrong:
 * the digest must not depend on how the message was split into Update calls, and the
 * padding must be right at every length around the 64-byte block boundary. Finally
 * the values the old wrappers produced before the switch (CryptoGoldenVectors.h) must
 * come out of the new code byte for byte.
 */

#include "TestHarness.h"
#include "CryptoGoldenVectors.h"

#include "Crypto/Hex.h"
#include "Crypto/HmacSha1.h"
#include "Crypto/Md5.h"
#include "Crypto/SecureZero.h"
#include "Crypto/Sha1.h"
#include "Crypto/Sha256.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

using namespace MaNGOS::Crypto;

namespace
{
    std::string Lower(const char* hex)
    {
        std::string s(hex);
        for (auto& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    const uint8_t* Bytes(const char* text) { return reinterpret_cast<const uint8_t*>(text); }

    std::vector<uint8_t> Pattern(size_t length)
    {
        std::vector<uint8_t> v(length);
        uint32_t x = 0x12345678u;
        for (auto& b : v) { x = x * 1103515245u + 12345u; b = uint8_t(x >> 16); }
        return v;
    }
}

TEST(CryptoSha1_fips_180_examples)
{
    CHECK_HEX(Sha1(Bytes("abc"), 3).data(), 20, "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK_HEX(Sha1(Bytes(""), 0).data(), 20, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    const char* two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK_HEX(Sha1(Bytes(two), std::strlen(two)).data(), 20, "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    std::vector<uint8_t> million(1000000, uint8_t('a'));
    Sha1Core core;
    for (size_t off = 0; off < million.size(); off += 4096)
    {
        core.Update(million.data() + off, std::min<size_t>(4096, million.size() - off));
    }
    uint8_t out[20];
    core.Finish(out);
    CHECK_HEX(out, 20, "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
}

TEST(CryptoSha256_fips_180_examples)
{
    CHECK_HEX(Sha256(Bytes("abc"), 3).data(), 32, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK_HEX(Sha256(Bytes(""), 0).data(), 32, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const char* two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK_HEX(Sha256(Bytes(two), std::strlen(two)).data(), 32, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    std::vector<uint8_t> million(1000000, uint8_t('a'));
    CHECK_HEX(Sha256(million.data(), million.size()).data(), 32, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(CryptoMd5_rfc_1321_suite)
{
    struct Case { const char* text; const char* digest; };
    const Case cases[] =
    {
        { "", "d41d8cd98f00b204e9800998ecf8427e" },
        { "a", "0cc175b9c0f1b6a831c399e269772661" },
        { "abc", "900150983cd24fb0d6963f7d28e17f72" },
        { "message digest", "f96b697d7cb7938d525a2f31aaf161d0" },
        { "abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b" },
        { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", "d174ab98d277d9f5a5611c2c9f419d9f" },
        { "12345678901234567890123456789012345678901234567890123456789012345678901234567890", "57edf4a22be3c955ac49da2e2107b67a" },
    };
    for (const Case& c : cases)
    {
        CHECK_HEX(Md5(Bytes(c.text), std::strlen(c.text)).data(), 16, c.digest);
    }
}

TEST(CryptoHmacSha1_rfc_2202_cases)
{
    uint8_t out[20];
    {
        std::vector<uint8_t> key(20, 0x0b);
        HmacSha1 h(key.data(), key.size());
        h.Update(Bytes("Hi There"), 8);
        h.Finish(out);
        CHECK_HEX(out, 20, "b617318655057264e28bc0b6fb378c8ef146be00");
    }
    {
        HmacSha1 h(Bytes("Jefe"), 4);
        const char* msg = "what do ya want for nothing?";
        h.Update(Bytes(msg), std::strlen(msg));
        h.Finish(out);
        CHECK_HEX(out, 20, "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");
    }
    {
        std::vector<uint8_t> key(20, 0xaa), data(50, 0xdd);
        HmacSha1 h(key.data(), key.size());
        h.Update(data.data(), data.size());
        h.Finish(out);
        CHECK_HEX(out, 20, "125d7342b9ac11cd91a39af48aa17b4f63f175d3");
    }
    {
        std::vector<uint8_t> key(25), data(50, 0xcd);
        for (size_t i = 0; i < key.size(); ++i) key[i] = uint8_t(i + 1);
        HmacSha1 h(key.data(), key.size());
        h.Update(data.data(), data.size());
        h.Finish(out);
        CHECK_HEX(out, 20, "4c9007f4026250c6bc8414f9bf50c86c2d7235da");
    }
    {
        std::vector<uint8_t> key(20, 0x0c);
        HmacSha1 h(key.data(), key.size());
        const char* msg = "Test With Truncation";
        h.Update(Bytes(msg), std::strlen(msg));
        h.Finish(out);
        CHECK_HEX(out, 20, "4c1a03424b55e07fe7f27be1d58bb9324a9a5a04");
    }
    {
        std::vector<uint8_t> key(80, 0xaa);
        HmacSha1 h(key.data(), key.size());
        const char* msg = "Test Using Larger Than Block-Size Key - Hash Key First";
        h.Update(Bytes(msg), std::strlen(msg));
        h.Finish(out);
        CHECK_HEX(out, 20, "aa4ae5e15272d00e95705637ce8a3b55ed402112");
    }
    {
        std::vector<uint8_t> key(80, 0xaa);
        HmacSha1 h(key.data(), key.size());
        const char* msg = "Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data";
        h.Update(Bytes(msg), std::strlen(msg));
        h.Finish(out);
        CHECK_HEX(out, 20, "e8e99d0f45237d786d6bbaa7965c7808bbff1a91");
    }
}

TEST(CryptoHash_incremental_equals_one_shot_at_every_split)
{
    const std::vector<uint8_t> msg = Pattern(1000);
    const auto whole1 = Sha1(msg.data(), msg.size());
    const auto whole256 = Sha256(msg.data(), msg.size());
    const auto wholeMd5 = Md5(msg.data(), msg.size());
    for (size_t split = 0; split <= msg.size(); split += 7)
    {
        Sha1Core s1; s1.Update(msg.data(), split); s1.Update(msg.data() + split, msg.size() - split);
        uint8_t o1[20]; s1.Finish(o1);
        CHECK(std::memcmp(o1, whole1.data(), 20) == 0);
        Sha256Core s2; s2.Update(msg.data(), split); s2.Update(msg.data() + split, msg.size() - split);
        uint8_t o2[32]; s2.Finish(o2);
        CHECK(std::memcmp(o2, whole256.data(), 32) == 0);
        Md5Core s3; s3.Update(msg.data(), split); s3.Update(msg.data() + split, msg.size() - split);
        uint8_t o3[16]; s3.Finish(o3);
        CHECK(std::memcmp(o3, wholeMd5.data(), 16) == 0);
    }
    // byte by byte, and with empty updates in between
    Sha1Core one;
    for (size_t i = 0; i < msg.size(); ++i)
    {
        one.Update(msg.data() + i, 1);
        one.Update(msg.data(), 0);
    }
    uint8_t out[20];
    one.Finish(out);
    CHECK(std::memcmp(out, whole1.data(), 20) == 0);
}

TEST(CryptoHash_padding_at_the_block_boundary)
{
    // Lengths 55/56 decide whether the bit count fits the current block; 63/64/65
    // straddle the block. The reference is the one-shot of a fresh core, and every
    // core must agree with itself after a Finish (the state is reset).
    for (size_t length : { size_t(0), size_t(1), size_t(55), size_t(56), size_t(57), size_t(63), size_t(64), size_t(65), size_t(119), size_t(120), size_t(128) })
    {
        const std::vector<uint8_t> msg = Pattern(length);
        Sha1Core core;
        uint8_t first[20], second[20];
        core.Update(msg.data(), msg.size()); core.Finish(first);
        core.Update(msg.data(), msg.size()); core.Finish(second);
        CHECK(std::memcmp(first, second, 20) == 0);
        CHECK(std::memcmp(first, Sha1(msg.data(), msg.size()).data(), 20) == 0);
    }
}

TEST(CryptoHash_reproduces_the_goldens)
{
    // The SRP6 password hash of the golden handshake: SHA-1("GOLDEN:VECTOR") as captured.
    CHECK_HEX(Sha1(Bytes("GOLDEN:VECTOR"), 13).data(), 20, Lower(golden::kSrp6.passwordHashHex));

    // The HMAC keyed with a 39-byte session key taken at its 40-byte width: a zero byte
    // in front (the big-endian view AsByteArray(40) pads at the high end; the key is
    // little-endian on the wire, so the zero lands at the end of the byte array).
    std::vector<uint8_t> keyBE, data;
    REQUIRE(FromHex(golden::kHmacShortKey.K, keyBE));
    REQUIRE(FromHex(golden::kHmacShortKey.data, data));
    CHECK_EQ(keyBE.size(), size_t(39));
    std::vector<uint8_t> keyLE(keyBE.rbegin(), keyBE.rend());   // the little-endian byte array the old HMACSHA1 was fed
    keyLE.push_back(0x00);                                       // AsByteArray(40) pads the high end
    HmacSha1 h(keyLE.data(), keyLE.size());
    h.Update(data.data(), data.size());
    uint8_t out[20];
    h.Finish(out);
    CHECK_HEX(out, 20, Lower(golden::kHmacShortKey.hmac));
}

TEST(CryptoHex_round_trips_and_rejects_garbage)
{
    const uint8_t bytes[] = { 0x00, 0x7F, 0x80, 0xFF, 0x1A };
    CHECK_STR(ToHex(bytes, 5), "007F80FF1A");
    CHECK_STR(ToHex(bytes, 5, false), "007f80ff1a");
    std::vector<uint8_t> back;
    CHECK(FromHex("007f80FF1a", back));
    CHECK(back.size() == 5 && std::memcmp(back.data(), bytes, 5) == 0);
    CHECK(FromHex(" 00 7f\n80ff1a ", back));                 // whitespace ignored
    CHECK(back.size() == 5);
    std::vector<uint8_t> untouched = { 1, 2, 3 };
    CHECK(!FromHex("abc", untouched));                        // odd digit count
    CHECK(!FromHex("zz", untouched));                         // not hex
    CHECK(untouched.size() == 3);                             // left alone on failure
    CHECK(FromHex("", back) && back.empty());
}

TEST(CryptoSecureZero_erases)
{
    uint8_t buf[64];
    std::memset(buf, 0xA5, sizeof buf);
    SecureZero(buf, sizeof buf);
    bool allZero = true;
    for (uint8_t b : buf) allZero = allZero && b == 0;
    CHECK(allZero);
    SecureZero(nullptr, 16);   // tolerated
    SecureZero(buf, 0);
}
