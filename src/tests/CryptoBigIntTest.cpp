/**
 * @file
 * @brief BigInt against the captured goldens, Python's vectors, and its own invariants.
 *
 * Three independent references: what the old wrappers computed before the switch
 * (CryptoGoldenVectors.h), what Python's integers compute (BigIntVectors.h), and the
 * algebra a = q * d + r, r < d that any division must satisfy. The directed cases in
 * both tables are the ones Knuth's Algorithm D gets wrong first: a quotient estimate
 * one or two too large, the add-back, a divisor whose top limb is small or all ones.
 */

#include "TestHarness.h"
#include "BigIntVectors.h"
#include "CryptoGoldenVectors.h"

#include "Crypto/BigInt.h"
#include "Crypto/Fatal.h"

#include <cstring>
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

    [[noreturn]] void Throw(const char* what)
    {
        throw std::runtime_error(what ? what : "");
    }

    struct FatalScope
    {
        FatalHandler previous;
        FatalScope() : previous(SetFatalHandler(&Throw)) {}
        ~FatalScope() { SetFatalHandler(previous); }
    };

    void CheckDivision(const BigInt& a, const BigInt& d, const char* expectedQ, const char* expectedR)
    {
        BigInt q, r;
        BigInt::DivMod(a, d, q, r);
        CHECK_STR(q.ToHex(), expectedQ);
        CHECK_STR(r.ToHex(), expectedR);
        CHECK(r < d);
        CHECK(q * d + r == a);
        CHECK_STR((a / d).ToHex(), expectedQ);
        CHECK_STR((a % d).ToHex(), expectedR);
    }
}

TEST(CryptoBigInt_reproduces_the_goldens)
{
    const size_t count = sizeof(golden::kBigInt) / sizeof(golden::kBigInt[0]);
    CHECK(count > 200);
    for (size_t i = 0; i < count; ++i)
    {
        const golden::BigIntCase& c = golden::kBigInt[i];
        BigInt a = FromHex(c.a), b = FromHex(c.b), n = FromHex(c.n);
        CHECK_STR(a.ToHex(), c.a);
        CHECK_STR(b.ToHex(), c.b);
        CHECK_STR(n.ToHex(), c.n);
        CHECK_STR((a + b).ToHex(), c.sum);
        CHECK_STR((a * b).ToHex(), c.product);
        CheckDivision(a, n, c.quotient, c.remainder);
    }
}

TEST(CryptoBigInt_matches_python_multiplication_and_division)
{
    for (const vectors::MulVector& v : vectors::kMul)
    {
        BigInt a = FromHex(v.a), b = FromHex(v.b);
        CHECK_STR((a * b).ToHex(), v.product);
        CHECK_STR((b * a).ToHex(), v.product);
        BigInt c(a);
        c *= b;
        CHECK_STR(c.ToHex(), v.product);
    }
    for (const vectors::DivVector& v : vectors::kDiv)
    {
        CheckDivision(FromHex(v.a), FromHex(v.d), v.q, v.r);
    }
}

TEST(CryptoBigInt_addition_and_subtraction_invert)
{
    for (const vectors::MulVector& v : vectors::kMul)
    {
        BigInt a = FromHex(v.a), b = FromHex(v.b);
        BigInt sum = a + b;
        CHECK(sum - b == a);
        CHECK(sum - a == b);
        CHECK(sum >= a && sum >= b);
        BigInt zero = a - a;
        CHECK(zero.IsZero());
        CHECK_STR(zero.ToHex(), "0");
    }
    // carries across every limb
    BigInt ones;
    ones.FromHex("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
    BigInt next = ones + BigInt(1);
    CHECK_STR(next.ToHex(), "010000000000000000000000000000000000000000000000000000000000000000");
    CHECK_EQ(next.LimbCount(), size_t(5));
    CHECK(next - BigInt(1) == ones);
}

TEST(CryptoBigInt_subtraction_below_zero_is_fatal)
{
    FatalScope scope;
    BigInt a(5), b(6);
    bool threw = false;
    try { BigInt c = a - b; (void)c; } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
}

TEST(CryptoBigInt_division_by_zero_is_fatal)
{
    FatalScope scope;
    BigInt a(5), zero;
    bool threw = false;
    try { BigInt c = a / zero; (void)c; } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { BigInt c = a % zero; (void)c; } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
}

TEST(CryptoBigInt_hex_parsing_and_rendering)
{
    BigInt v;
    CHECK(v.FromHex("0"));
    CHECK(v.IsZero());
    CHECK_STR(v.ToHex(), "0");
    CHECK(v.FromHex("00ff"));
    CHECK_STR(v.ToHex(), "FF");
    CHECK(v.FromHex("abcdef0123456789ABCDEF"));
    CHECK_STR(v.ToHex(), "ABCDEF0123456789ABCDEF");
    CHECK(v.FromHex("1"));                    // rendered as a whole byte, as the old wrappers did
    CHECK_STR(v.ToHex(), "01");
    CHECK(v.FromHex("ABC"));
    CHECK_STR(v.ToHex(), "0ABC");
    CHECK(v.FromHex("10000000000000000"));   // 2^64: two limbs
    CHECK_EQ(v.LimbCount(), size_t(2));
    CHECK_STR(v.ToHex(), "010000000000000000");
    CHECK(v.Bit(64) && !v.Bit(63) && !v.Bit(0));
    CHECK(v.FromHex(""));                     // no digits: zero
    CHECK(v.IsZero());
    BigInt keep(7);
    CHECK(!keep.FromHex("-1"));               // a sign is not a digit
    CHECK(keep.IsZero());                     // and the value is zero after a rejection
    CHECK(!keep.FromHex("0x10"));
    CHECK(!keep.FromHex("12 34"));
    CHECK(!keep.FromHex("G"));
}

TEST(CryptoBigInt_decimal_rendering)
{
    for (const vectors::DecVector& v : vectors::kDecimal)
    {
        CHECK_STR(FromHex(v.hex).ToDecimal(), v.decimal);
    }
}

TEST(CryptoBigInt_byte_conversions_and_padding)
{
    const uint8_t le[] = { 0x02, 0x01 };            // 0x0102 little-endian
    BigInt v = BigInt::FromBytesLE(le, 2);
    CHECK_STR(v.ToHex(), "0102");
    const uint8_t be[] = { 0x01, 0x02 };
    CHECK(BigInt::FromBytesBE(be, 2) == v);

    std::vector<uint8_t> out = v.ToBytesLE();
    CHECK(out.size() == 2 && out[0] == 0x02 && out[1] == 0x01);
    out = v.ToBytesLE(4);                           // padded at the high end
    CHECK(out.size() == 4 && out[0] == 0x02 && out[1] == 0x01 && out[2] == 0 && out[3] == 0);
    out = v.ToBytesLE(1);                           // minSize below the length: the length wins
    CHECK(out.size() == 2);
    out = v.ToBytesBE(4);                           // left-padded
    CHECK(out.size() == 4 && out[0] == 0 && out[1] == 0 && out[2] == 0x01 && out[3] == 0x02);
    out = v.ToBytesBE(0);                           // minimal
    CHECK(out.size() == 2 && out[0] == 0x01 && out[1] == 0x02);
    out = v.ToBytesBE(1);                           // does not fit
    CHECK(out.empty());

    BigInt zero;
    CHECK(zero.ToBytesLE().empty());
    CHECK(zero.ToBytesLE(3).size() == 3);
    CHECK(zero.ToBytesBE(0).empty());
    CHECK(zero.ToBytesBE(2).size() == 2);
    CHECK_EQ(zero.ByteLength(), size_t(0));
    CHECK_EQ(zero.BitLength(), size_t(0));

    // leading zero bytes on input are not significant
    const uint8_t padded[] = { 0x00, 0x00, 0x01, 0x02 };
    CHECK(BigInt::FromBytesBE(padded, 4) == v);
    const uint8_t paddedLE[] = { 0x02, 0x01, 0x00, 0x00 };
    CHECK(BigInt::FromBytesLE(paddedLE, 4) == v);
    CHECK_EQ(v.ByteLength(), size_t(2));
    CHECK_EQ(v.BitLength(), size_t(9));

    // a 40-byte value: the session-key shape, through both byte orders
    std::vector<uint8_t> key(40);
    for (size_t i = 0; i < key.size(); ++i) key[i] = uint8_t(i * 7 + 1);
    BigInt k = BigInt::FromBytesLE(key.data(), key.size());
    std::vector<uint8_t> backLE = k.ToBytesLE(40), backBE = k.ToBytesBE(40);
    CHECK(backLE == key);
    std::vector<uint8_t> reversed(key.rbegin(), key.rend());
    CHECK(backBE == reversed);
}

TEST(CryptoBigInt_shifts_and_comparison)
{
    BigInt one(1);
    BigInt big = one << 200;
    CHECK_EQ(big.BitLength(), size_t(201));
    CHECK(big.Bit(200) && !big.Bit(199));
    CHECK((big >> 200) == one);
    CHECK((big >> 201).IsZero());
    CHECK((big >> 1000).IsZero());
    CHECK((big << 0) == big);
    CHECK((big >> 0) == big);
    BigInt x;
    x.FromHex("123456789ABCDEF0123456789ABCDEF0");
    CHECK_STR((x << 4).ToHex(), "0123456789ABCDEF0123456789ABCDEF00");
    CHECK_STR((x >> 4).ToHex(), "0123456789ABCDEF0123456789ABCDEF");
    CHECK_STR((x << 64).ToHex(), "123456789ABCDEF0123456789ABCDEF00000000000000000");
    CHECK_STR((x >> 64).ToHex(), "123456789ABCDEF0");
    CHECK_STR(((x << 67) >> 67).ToHex(), x.ToHex());

    CHECK(BigInt(0) < BigInt(1));
    CHECK(BigInt(5) == BigInt(5));
    CHECK(BigInt(5) != BigInt(6));
    CHECK(big > x);
    CHECK(x <= x && x >= x);
    CHECK_EQ(x.Compare(big), -1);
    CHECK_EQ(big.Compare(x), 1);
    CHECK_EQ(x.Low64(), uint64_t(0x123456789ABCDEF0ull));
    CHECK(BigInt(0).IsZero() && !BigInt(1).IsZero() && BigInt(1).IsOne() && BigInt(3).IsOdd() && !BigInt(4).IsOdd());
}

TEST(CryptoBigInt_secure_clear_leaves_zero)
{
    BigInt v;
    v.FromHex("DEADBEEFDEADBEEFDEADBEEFDEADBEEF");
    v.SecureClear();
    CHECK(v.IsZero());
    CHECK_EQ(v.LimbCount(), size_t(0));
}
