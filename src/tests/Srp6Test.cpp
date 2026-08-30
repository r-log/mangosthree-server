/**
 * @file
 * @brief The SRP6 calculation realmd runs, as a unit.
 *
 * `Srp6` is what `AuthSocket` calls, so what these tests pin is the production
 * calculation and not a copy of its formulas. The golden handshakes were captured on
 * the old build (`CryptoGoldenVectors.h`); the in-house primitives that replace
 * it must reproduce every intermediate byte for byte. The short-width handshakes are
 * the ones that matter most: every quantity is hashed at its protocol width, and a
 * minimal encoding passes the full-width handshake and fails all of them.
 */

#include "TestHarness.h"
#include "CryptoGoldenVectors.h"

#include "Auth/BigNumber.h"
#include "Auth/Srp6.h"

#include <cctype>
#include <cstring>
#include <string>

namespace
{
    std::string Hex(BigNumber& v)
    {
        return std::string(v.AsHexStr());
    }

    BigNumber FromHex(const char* hex)
    {
        BigNumber v;
        v.SetHexStr(hex);
        return v;
    }

    // CHECK_HEX renders bytes in lowercase; the captured table is uppercase (AsHexStr's case).
    std::string Lower(const char* hex)
    {
        std::string s(hex);
        for (auto& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    /// Replay one captured handshake through Srp6 and compare every intermediate.
    void Replay(const golden::Srp6Vector& gv)
    {
        BigNumber s = FromHex(gv.s), b = FromHex(gv.b), A = FromHex(gv.A), challenge = FromHex(gv.challenge);

        BigNumber v = Srp6::Verifier(s, gv.passwordHashHex);
        CHECK_STR(Hex(v).c_str(), gv.v);

        BigNumber B = Srp6::ServerEphemeral(v, b);
        CHECK_STR(Hex(B).c_str(), gv.B);

        BigNumber u = Srp6::Scrambler(A, B);
        CHECK_STR(Hex(u).c_str(), gv.u);

        BigNumber K = Srp6::SessionKey(A, v, u, b);
        CHECK_STR(Hex(K).c_str(), gv.K);
        CHECK(K.GetNumBytes() <= Srp6::SessionKeyWidth);

        uint8 M1[Srp6::DigestWidth], M2[Srp6::DigestWidth], R2[Srp6::DigestWidth];
        Srp6::ClientProof(gv.login, s, A, B, K, M1);
        CHECK_HEX(M1, Srp6::DigestWidth, Lower(gv.M1));
        Srp6::ServerProof(A, M1, K, M2);
        CHECK_HEX(M2, Srp6::DigestWidth, Lower(gv.M2));

        BigNumber R1 = FromHex(gv.R1);
        Srp6::ReconnectProof(gv.login, R1.AsByteArray(Srp6::ReconnectWidth, false), challenge, K, R2);
        CHECK_HEX(R2, Srp6::DigestWidth, Lower(gv.R2));
    }
}

TEST(Srp6_modulus_and_generator_are_the_protocol_constants)
{
    BigNumber N = Srp6::Modulus();
    CHECK_STR(Hex(N).c_str(), "894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7");
    CHECK_EQ(N.GetNumBytes(), Srp6::EphemeralWidth);
    BigNumber g = Srp6::Generator();
    CHECK_EQ(g.AsDword(), 7u);
    CHECK_EQ(g.GetNumBytes(), Srp6::GeneratorWidth);
}

TEST(Srp6_golden_handshake_reproduces_every_intermediate)
{
    const golden::Srp6Vector& gv = golden::kSrp6;
    CHECK_STR(gv.name, "full width");
    BigNumber s = FromHex(gv.s), A = FromHex(gv.A), B = FromHex(gv.B), K = FromHex(gv.K);
    CHECK_EQ(s.GetNumBytes(), Srp6::SaltWidth);
    CHECK_EQ(A.GetNumBytes(), Srp6::EphemeralWidth);
    CHECK_EQ(B.GetNumBytes(), Srp6::EphemeralWidth);
    CHECK_EQ(K.GetNumBytes(), Srp6::SessionKeyWidth);
    Replay(gv);
}

TEST(Srp6_short_width_handshakes_reproduce_every_intermediate)
{
    // One handshake per quantity that can come up short on the wire or in the derivation:
    // the client's A, the salt, the reconnect challenge, the server's B, the premaster S
    // (only visible through K here) and the session key K. Each is hashed at its protocol
    // width; these are the vectors a minimal encoding fails.
    const size_t count = sizeof(golden::kSrp6Short) / sizeof(golden::kSrp6Short[0]);
    CHECK_EQ(count, size_t(6));
    bool shortA = false, shortSalt = false, shortChallenge = false, shortB = false, shortS = false, shortK = false;
    for (size_t i = 0; i < count; ++i)
    {
        const golden::Srp6Vector& gv = golden::kSrp6Short[i];
        Replay(gv);
        BigNumber s = FromHex(gv.s), A = FromHex(gv.A), B = FromHex(gv.B), K = FromHex(gv.K), challenge = FromHex(gv.challenge);
        if (std::strcmp(gv.name, "short A") == 0)          { CHECK(A.GetNumBytes() < Srp6::EphemeralWidth); shortA = true; }
        if (std::strcmp(gv.name, "short s") == 0)          { CHECK(s.GetNumBytes() < Srp6::SaltWidth); shortSalt = true; }
        if (std::strcmp(gv.name, "short challenge") == 0)  { CHECK(challenge.GetNumBytes() < Srp6::ReconnectWidth); shortChallenge = true; }
        if (std::strcmp(gv.name, "short B") == 0)          { CHECK(B.GetNumBytes() < Srp6::EphemeralWidth); shortB = true; }
        if (std::strcmp(gv.name, "short S") == 0)          { shortS = true; }
        if (std::strcmp(gv.name, "short K") == 0)          { CHECK(K.GetNumBytes() < Srp6::SessionKeyWidth); shortK = true; }
    }
    CHECK(shortA && shortSalt && shortChallenge && shortB && shortS && shortK);
}

TEST(Srp6_verifier_hashes_the_salt_and_the_password_hash_at_their_widths)
{
    // A salt whose top byte is zero and a password hash whose top byte is zero: the
    // client hashes 32 and 20 bytes regardless, so the server must too. A minimal
    // encoding here would produce a verifier the client can never match.
    const golden::VerifierVector& gv = golden::kVerifierShort;
    BigNumber s = FromHex(gv.s);
    CHECK_EQ(s.GetNumBytes(), Srp6::SaltWidth - 1);
    BigNumber v = Srp6::Verifier(s, gv.passwordHashHex);
    CHECK_STR(Hex(v).c_str(), gv.v);
}

TEST(Srp6_verifier_accepts_the_password_hash_in_either_case)
{
    BigNumber s = FromHex(golden::kSrp6.s);
    std::string upper = golden::kSrp6.passwordHashHex, lower = upper;
    for (auto& c : lower) c = char(std::tolower(static_cast<unsigned char>(c)));
    BigNumber vUpper = Srp6::Verifier(s, upper), vLower = Srp6::Verifier(s, lower);
    CHECK_STR(Hex(vUpper).c_str(), Hex(vLower).c_str());
}

TEST(Srp6_rejects_a_client_ephemeral_that_is_a_multiple_of_N)
{
    BigNumber N = Srp6::Modulus();
    CHECK(!Srp6::IsAcceptableClientEphemeral(BigNumber(0u)));
    CHECK(!Srp6::IsAcceptableClientEphemeral(N));
    BigNumber twoN = N * BigNumber(2u);
    CHECK(!Srp6::IsAcceptableClientEphemeral(twoN));
    BigNumber NPlusOne = N + BigNumber(1u);
    CHECK(Srp6::IsAcceptableClientEphemeral(NPlusOne));
    CHECK(Srp6::IsAcceptableClientEphemeral(BigNumber(1u)));
}

TEST(Srp6_server_ephemeral_is_reduced_below_N)
{
    BigNumber v = FromHex(golden::kSrp6.v), b = FromHex(golden::kSrp6.b);
    BigNumber B = Srp6::ServerEphemeral(v, b);
    BigNumber reduced = B % Srp6::Modulus();
    CHECK_STR(Hex(reduced).c_str(), Hex(B).c_str());
    CHECK(B.GetNumBytes() <= Srp6::EphemeralWidth);
}

TEST(Srp6_proofs_change_with_every_input)
{
    const golden::Srp6Vector& gv = golden::kSrp6;
    BigNumber s = FromHex(gv.s), A = FromHex(gv.A), B = FromHex(gv.B), K = FromHex(gv.K);
    uint8 M1[20], M1b[20], M2[20], M2b[20];
    Srp6::ClientProof(gv.login, s, A, B, K, M1);
    Srp6::ClientProof("golden", s, A, B, K, M1b);          // the login is hashed as sent: case matters
    CHECK(std::memcmp(M1, M1b, 20) != 0);
    Srp6::ServerProof(A, M1, K, M2);
    uint8 flipped[20]; std::memcpy(flipped, M1, 20); flipped[7] ^= 0x01;
    Srp6::ServerProof(A, flipped, K, M2b);
    CHECK(std::memcmp(M2, M2b, 20) != 0);
}
