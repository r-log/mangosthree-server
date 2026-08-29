/**
 * @file
 * @brief SystemRandom: the FillExact contract, the SetRand shape, and the fatal paths.
 *
 * The backend is injected so the cases the operating system will not stage on demand
 * -- short reads, end of data, a hard error -- are exercised here. CryptoFatal is
 * redirected to a throwing handler for the duration of each such case.
 */

#include "TestHarness.h"

#include "Crypto/Fatal.h"
#include "Crypto/SystemRandom.h"

#include <cstring>
#include <set>
#include <stdexcept>
#include <vector>

using namespace MaNGOS::Crypto;

namespace
{
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

    // Delivers one byte per call, the byte being its position -- the short-read backend.
    size_t g_position = 0;
    bool OneByteAtATime(uint8_t* out, size_t want, size_t& got)
    {
        (void)want;
        out[0] = uint8_t(g_position++);
        got = 1;
        return true;
    }

    bool EndOfData(uint8_t*, size_t, size_t& got)
    {
        got = 0;
        return true;
    }

    bool HardError(uint8_t*, size_t, size_t& got)
    {
        got = 0;
        return false;
    }

    bool TooMuch(uint8_t* out, size_t want, size_t& got)
    {
        std::memset(out, 0, want);
        got = want + 1;
        return true;
    }
}

TEST(CryptoRandom_fill_exact_loops_over_short_reads)
{
    g_position = 0;
    SystemRandom random(&OneByteAtATime);
    uint8_t buf[64];
    std::memset(buf, 0xFF, sizeof buf);
    random.FillExact(buf, sizeof buf);
    bool ok = true;
    for (size_t i = 0; i < sizeof buf; ++i) ok = ok && buf[i] == uint8_t(i);
    CHECK(ok);
    random.FillExact(buf, 0);   // nothing to do, no call
}

TEST(CryptoRandom_end_of_data_is_fatal)
{
    FatalScope scope;
    SystemRandom random(&EndOfData);
    uint8_t buf[8];
    bool threw = false;
    try { random.FillExact(buf, sizeof buf); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
}

TEST(CryptoRandom_hard_error_is_fatal)
{
    FatalScope scope;
    SystemRandom random(&HardError);
    uint8_t buf[8];
    bool threw = false;
    try { random.FillExact(buf, sizeof buf); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
}

TEST(CryptoRandom_overdelivery_is_fatal)
{
    FatalScope scope;
    SystemRandom random(&TooMuch);
    uint8_t buf[8];
    bool threw = false;
    try { random.FillExact(buf, sizeof buf); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
}

TEST(CryptoRandom_os_backend_delivers_entropy)
{
    std::vector<uint8_t> buf(1024, 0);
    SystemRandom::Instance().FillExact(buf.data(), buf.size());
    std::set<uint8_t> distinct(buf.begin(), buf.end());
    CHECK(distinct.size() > 100);   // 1024 random bytes hit far more than 100 of the 256 values
}

TEST(CryptoRandom_bits_has_the_setrand_shape)
{
    // Exactly `bits` bits, the top one set, odd: what BN_rand(top = 0, bottom = 1) gave
    // the SRP6 ephemerals and salts. 200 draws per width.
    for (size_t bits : { size_t(2), size_t(32), size_t(152), size_t(256), size_t(257) })
    {
        for (int i = 0; i < 200; ++i)
        {
            BigInt v = SystemRandom::Instance().Bits(bits);
            CHECK_EQ(v.BitLength(), bits);
            CHECK(v.IsOdd());
        }
    }
}

TEST(CryptoRandom_below_is_uniform_enough_and_in_range)
{
    const BigInt ten(10);
    std::set<uint64_t> seen;
    for (int i = 0; i < 2000; ++i)
    {
        BigInt v = SystemRandom::Instance().Below(ten);
        CHECK(v < ten);
        seen.insert(v.Low64());
    }
    CHECK_EQ(seen.size(), size_t(10));   // every residue turns up in 2,000 draws
    // a wide bound: every draw is below it, and their widths vary
    BigInt bound;
    bound.FromHex("894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7");
    for (int i = 0; i < 200; ++i)
    {
        CHECK(SystemRandom::Instance().Below(bound) < bound);
    }
    // a bound that is a power of two: the mask alone suffices, still below
    BigInt two64 = BigInt(1) << 64;
    for (int i = 0; i < 200; ++i)
    {
        CHECK(SystemRandom::Instance().Below(two64) < two64);
    }
}
