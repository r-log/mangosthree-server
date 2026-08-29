// The golden-vector generator. Run on the OpenSSL build, its stdout becomes
// src/tests/CryptoGoldenVectors.h and its stderr the timing baseline the in-house kernel
// is measured against (see src/tests/CMakeLists.txt for the command). Built on demand
// only; it captures what OpenSSL computes, so it leaves the tree together with OpenSSL.
#include "Auth/BigNumber.h"
#include "Auth/Sha1.h"
#include "Auth/HMACSHA1.h"
#include "Auth/AuthCrypt.h"
#include "Auth/Srp6.h"

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace
{
    std::mt19937_64 rng(0x4D414E474F53ULL);   // fixed seed: the table is reproducible

    std::string HexOf(const uint8* p, int n)
    {
        static const char* digits = "0123456789ABCDEF";
        std::string s;
        for (int i = 0; i < n; ++i) { s += digits[p[i] >> 4]; s += digits[p[i] & 15]; }
        return s;
    }

    std::string HexOf(BigNumber& v)
    {
        const char* h = v.AsHexStr();
        std::string s(h);
        OPENSSL_free(const_cast<char*>(h));
        return s;
    }

    // A random value of exactly `bits` bits (top bit set), as uppercase hex.
    BigNumber RandomBits(int bits)
    {
        std::vector<uint8> bytes((bits + 7) / 8);
        for (auto& b : bytes) b = uint8(rng());
        int top = (bits - 1) % 8;
        bytes.back() &= uint8((2u << top) - 1);
        bytes.back() |= uint8(1u << top);
        BigNumber v; v.SetBinary(bytes.data(), int(bytes.size()));   // little-endian
        return v;
    }

    // A random odd modulus of exactly `bits` bits.
    BigNumber OddModulus(int bits)
    {
        BigNumber n = RandomBits(bits);
        int nbytes = (bits + 7) / 8;
        const uint8* le = n.AsByteArray(nbytes);   // one call: the buffer is object-owned and replaced by the next call
        std::vector<uint8> nb(le, le + nbytes);
        nb[0] |= 1;
        BigNumber odd; odd.SetBinary(nb.data(), nbytes);
        return odd;
    }

    BigNumber FromLimbs(const std::vector<uint32>& limbs)     // little-endian 32-bit limbs
    {
        std::vector<uint8> bytes;
        for (uint32 l : limbs) for (int i = 0; i < 4; ++i) bytes.push_back(uint8(l >> (8 * i)));
        BigNumber v; v.SetBinary(bytes.data(), int(bytes.size()));
        return v;
    }

    BigNumber FromLimbs64(const std::vector<uint64>& limbs)   // little-endian 64-bit limbs
    {
        std::vector<uint8> bytes;
        for (uint64 l : limbs) for (int i = 0; i < 8; ++i) bytes.push_back(uint8(l >> (8 * i)));
        BigNumber v; v.SetBinary(bytes.data(), int(bytes.size()));
        return v;
    }

    // BigNumber::SetBinary reads little-endian; the RSA quantities are big-endian.
    void SetBigEndian(BigNumber& number, const uint8* data, size_t length)
    {
        std::vector<uint8> reversed(data, data + length);
        std::reverse(reversed.begin(), reversed.end());
        number.SetBinary(reversed.data(), int(reversed.size()));
    }

    /// One handshake through the Srp6 unit, printed as a Srp6Vector initialiser.
    void EmitHandshake(const char* name, const std::string& login, const std::string& passwordHashHex,
                       BigNumber s, BigNumber b, BigNumber A, const uint8* R1, BigNumber challenge, const char* terminator)
    {
        BigNumber v = Srp6::Verifier(s, passwordHashHex);
        BigNumber B = Srp6::ServerEphemeral(v, b);
        BigNumber u = Srp6::Scrambler(A, B);
        BigNumber K = Srp6::SessionKey(A, v, u, b);
        uint8 M1[20], M2[20], R2[20];
        Srp6::ClientProof(login, s, A, B, K, M1);
        Srp6::ServerProof(A, M1, K, M2);
        Srp6::ReconnectProof(login, R1, challenge, K, R2);
        std::printf("    { \"%s\", \"%s\", \"%s\",\n", name, login.c_str(), passwordHashHex.c_str());
        std::printf("      \"%s\",\n      \"%s\",\n      \"%s\",\n", HexOf(s).c_str(), HexOf(b).c_str(), HexOf(A).c_str());
        std::printf("      \"%s\", \"%s\",\n", HexOf(R1, 16).c_str(), HexOf(challenge).c_str());
        std::printf("      \"%s\",\n      \"%s\",\n      \"%s\",\n      \"%s\",\n",
                    HexOf(v).c_str(), HexOf(B).c_str(), HexOf(u).c_str(), HexOf(K).c_str());
        std::printf("      \"%s\", \"%s\", \"%s\" }%s\n", HexOf(M1, 20).c_str(), HexOf(M2, 20).c_str(), HexOf(R2, 20).c_str(), terminator);
    }

    /// b stepped from a fixed start until the derived quantity the caller asks for has a zero
    /// top byte. Deterministic: no RNG, so the rest of the table does not move.
    BigNumber SearchB(const std::string& passwordHashHex, BigNumber s, BigNumber A,
                      const std::function<bool(BigNumber&, BigNumber&, BigNumber&)>& want)
    {
        BigNumber v = Srp6::Verifier(s, passwordHashHex);
        BigNumber N = Srp6::Modulus();
        BigNumber b; b.SetHexStr("3C9F1E2D4B5A69788796A5B4C3D2E1F00113");
        for (int i = 0; i < 100000; ++i)
        {
            BigNumber B = Srp6::ServerEphemeral(v, b);
            BigNumber u = Srp6::Scrambler(A, B);
            BigNumber vv(v);
            BigNumber S = (BigNumber(A) * vv.ModExp(u, N)).ModExp(b, N);   // the premaster the session key is derived from
            BigNumber K = Srp6::SessionKey(A, v, u, b);
            if (want(B, S, K)) return b;
            b += BigNumber(1u);
        }
        std::fprintf(stderr, "search for a short SRP6 quantity failed\n");
        std::exit(1);
    }

    void EmitCase(BigNumber a, BigNumber b, BigNumber n)
    {
        BigNumber sum = a + b, product = a * b, quotient = a / n, remainder = a % n, modexp = a.ModExp(b, n);
        std::printf("    {\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"},\n",
                    HexOf(a).c_str(), HexOf(b).c_str(), HexOf(n).c_str(), HexOf(sum).c_str(),
                    HexOf(product).c_str(), HexOf(quotient).c_str(), HexOf(remainder).c_str(), HexOf(modexp).c_str());
    }

    // ------------------------------------------------------------------ timing
    using Clock = std::chrono::steady_clock;

    double MedianMicros(int runs, const std::function<void()>& fn)
    {
        std::vector<double> samples;
        for (int i = 0; i < runs; ++i)
        {
            auto t0 = Clock::now();
            fn();
            auto t1 = Clock::now();
            samples.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
        std::sort(samples.begin(), samples.end());
        return samples[samples.size() / 2];
    }

    const char* TEST_MODULUS =
        "91744218A5AC0134DC2BE63E95AA3F2F3130C1714DB7084E3AC31A170C4FA02B"
        "F9D62E5CB1E444258D852B100395CF7D2DC0088BB7B96DF2B1EDC3BFF0D968F8"
        "D1003D4726DBC79417B001AB37653922BC10FF538B43B186E0071E41653382C5"
        "2FE58D701948AAA8F20D15B62EC4147AE1142AC3A287461D8F8C9D4021369889"
        "FED17806C3662CEC078092D90CBE881343A160C0CD8984094C7D6F18481DE73E"
        "14EB6766D7C9EF88A575B5910C67D62F4071EFB3639EF5A1E7892CF16F6D6D43"
        "F56495769D0683580030F514CB21208360116717C16CCC4B233A11F8CCB3CEBC"
        "D9873C33CFA81ACD619F567403D8839B88CA37FA08392F02439054B88E70E233";
    const char* TEST_PRIVATE_EXPONENT =
        "0B8B0F67C756142E6EBEA922145C93711A554534C9B719D8A37F3245DBFB41B9"
        "DBB4ECAEFC8B22015CEED1910EC7C7D4A659D413CA7BD3C6EBE9F39BFAF0360D"
        "7100B4DC3DB039717E43C08E26F2488B8223532FFD205D295804189995FF7584"
        "529DC410BE60EEF2436B586AC1E15BC2B8B41204BE943FB33EDE28E89AFA2B36"
        "C145CA75174805FA0278EC54B545CDFF1D744C63BDC2F568692480D04933E295"
        "E0AFE18D886721843D41B51F491900EB58771749BD032F5B9453484516B26642"
        "A1355D38C1CD98F2E079FC3B66303DB17EEAE5B8A2DAFE605093031EE242C2C2"
        "C84BD028B6E94F3B87C2800FD79D7EFCD5DC40C53853A448DB17393EFDFEFEB9";
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);   // a crash must not swallow the lines before it
    std::printf("// Generated by the golden-vector capture harness on the OpenSSL build; do not edit by hand.\n");
    std::printf("// Every value here was produced by OpenSSL 3 through this repository's own wrappers, on\n");
    std::printf("// the same formulas the server runs. The in-house primitives must reproduce them exactly.\n");
    std::printf("// Generator: src/tests/tools/CryptoGoldenCapture.cpp (target crypto_golden_capture), mt19937_64 seed\n");
    std::printf("// 0x4D414E474F53; the searches for short SRP6 quantities are deterministic. Regenerate: build the target\n");
    std::printf("// on the OpenSSL build and run it with stdout into this file (src/tests/CMakeLists.txt has the command).\n");
    std::printf("#ifndef MANGOS_H_TESTS_CRYPTO_GOLDEN_VECTORS\n#define MANGOS_H_TESTS_CRYPTO_GOLDEN_VECTORS\n\n");
    std::printf("#include \"Platform/Define.h\"\n\nnamespace golden\n{\n");

    // ---------------------------------------------------------------- big integers
    std::printf("    /// a, b, n and OpenSSL's a+b, a*b, a/n, a%%n, a^b mod n (uppercase minimal hex).\n");
    std::printf("    struct BigIntCase { const char* a; const char* b; const char* n; const char* sum; const char* product; const char* quotient; const char* remainder; const char* modexp; };\n");
    std::printf("    inline const BigIntCase kBigInt[] = {\n");
    std::printf("    // single-limb divisors\n");
    for (uint32 d : { 1u, 2u, 3u, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFFu, 0x10000u, 0xB504F333u })
    {
        EmitCase(RandomBits(256 + int(rng() % 3) * 32), RandomBits(64), BigNumber(d));
    }
    std::printf("    // exact multiples\n");
    for (int i = 0; i < 8; ++i)
    {
        BigNumber n = RandomBits(64 + int(rng() % 4) * 64), q = RandomBits(32 + int(rng() % 8) * 32);
        EmitCase(n * q, RandomBits(96), n);
    }
    std::printf("    // dividend smaller than the divisor, and zero\n");
    for (int i = 0; i < 4; ++i) EmitCase(RandomBits(128), RandomBits(40), RandomBits(256));
    EmitCase(BigNumber(0u), RandomBits(64), RandomBits(256));
    std::printf("    // normalisation shifts 0..31 (the divisor's top limb has k leading zero bits)\n");
    for (int k = 0; k < 32; ++k)
    {
        EmitCase(RandomBits(512), RandomBits(64), RandomBits(256 - k));
    }
    std::printf("    // special limbs: quotient-estimate corrections and add-back live here\n");
    const uint32 special[] = { 0u, 1u, 2u, 0x7FFFFFFFu, 0x80000000u, 0x80000001u, 0xFFFFFFFEu, 0xFFFFFFFFu };
    for (int i = 0; i < 40; ++i)
    {
        std::vector<uint32> la, ln;
        int na = 2 + int(rng() % 7), nn = 1 + int(rng() % 4);
        for (int j = 0; j < na; ++j) la.push_back(special[rng() % 8]);
        for (int j = 0; j < nn; ++j) ln.push_back(special[rng() % 8]);
        if (la.back() == 0) la.back() = 0x80000000u;
        if (ln.back() == 0) ln.back() = 0x80000001u;
        EmitCase(FromLimbs(la), RandomBits(48), FromLimbs(ln));
    }
    std::printf("    // classic Knuth D add-back triggers\n");
    EmitCase(FromLimbs({ 0u, 0u, 0x80000000u }), RandomBits(40), FromLimbs({ 1u, 0x80000000u }));
    EmitCase(FromLimbs({ 0xFFFFFFFFu, 0xFFFFFFFFu, 0x7FFFFFFFu }), RandomBits(40), FromLimbs({ 1u, 0x80000000u }));
    EmitCase(FromLimbs({ 0u, 0xFFFFFFFEu, 0x80000000u }), RandomBits(40), FromLimbs({ 0xFFFFFFFFu, 0x80000000u }));
    EmitCase(FromLimbs({ 0x00000003u, 0u, 0x80000000u }), RandomBits(40), FromLimbs({ 0x00000001u, 0x20000000u }));
    std::printf("    // random 256/320/512-bit operands with a 256-bit odd modulus (the SRP6 shape)\n");
    for (int i = 0; i < 64; ++i)
    {
        int bits = (i % 3 == 0) ? 256 : (i % 3 == 1) ? 320 : 512;
        EmitCase(RandomBits(bits), RandomBits(i % 2 ? 256 : 152), OddModulus(256));
    }
    std::printf("    // 64-bit-limb shapes: single-limb, exact multiples and add-back triggers at 64-bit limb boundaries\n");
    for (uint64 d : { uint64(0xFFFFFFFFFFFFFFFFull), uint64(0x8000000000000000ull), uint64(0x8000000000000001ull), uint64(0x100000000ull) })
    {
        EmitCase(RandomBits(320), RandomBits(64), FromLimbs64({ d }));
    }
    EmitCase(FromLimbs64({ 0ull, 0ull, 0x8000000000000000ull }), RandomBits(40), FromLimbs64({ 1ull, 0x8000000000000000ull }));
    EmitCase(FromLimbs64({ 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull, 0x7FFFFFFFFFFFFFFFull }), RandomBits(40), FromLimbs64({ 1ull, 0x8000000000000000ull }));
    EmitCase(FromLimbs64({ 0ull, 0xFFFFFFFFFFFFFFFEull, 0x8000000000000000ull }), RandomBits(40), FromLimbs64({ 0xFFFFFFFFFFFFFFFFull, 0x8000000000000000ull }));
    std::printf("    // wide operands: 1024/2048/4096-bit bases, 2048-bit exponents, odd 1024/2048-bit moduli (the RSA shape)\n");
    for (int i = 0; i < 12; ++i)
    {
        int baseBits = (i % 3 == 0) ? 1024 : (i % 3 == 1) ? 2048 : 4096;
        int modBits  = (i % 2 == 0) ? 2048 : 1024;
        EmitCase(RandomBits(baseBits), RandomBits(2048), OddModulus(modBits));
    }
    std::printf("    // Montgomery patterns at 256/1024/2048 bits: x = n-1, all-ones operands, a modulus whose top limb is 0x8000..., x = y, operands just below n\n");
    for (int bits : { 256, 1024, 2048 })
    {
        BigNumber n = OddModulus(bits);
        BigNumber nMinusOne = n - BigNumber(1u);
        EmitCase(nMinusOne, nMinusOne, n);                                   // (n-1)^2 mod n = 1; (n-1)^(n-1) mod n = 1
        EmitCase(nMinusOne, RandomBits(bits), n);
        std::vector<uint64> ones(bits / 64, 0xFFFFFFFFFFFFFFFFull);
        EmitCase(FromLimbs64(ones), FromLimbs64(ones), n);                   // all-ones operands (>= n: the API reduces first)
        std::vector<uint64> top(bits / 64, 0);
        for (auto& l : top) l = rng();
        top[0] |= 1; top.back() = 0x8000000000000000ull;
        BigNumber nTop = FromLimbs64(top);
        EmitCase(RandomBits(bits), RandomBits(bits), nTop);
        EmitCase(nTop - BigNumber(1u), nTop - BigNumber(2u), nTop);
        BigNumber same = RandomBits(bits - 1);
        EmitCase(same, same, n);                                             // x = y
        EmitCase(n - BigNumber(0x1234u), n - BigNumber(0x5678u), n);         // just below n: the final subtraction is needed
    }
    std::printf("    };\n\n");

    // ---------------------------------------------------------------- SRP6
    const std::string login = "GOLDEN";
    Sha1Hash pw; pw.UpdateData(std::string("GOLDEN:VECTOR")); pw.Finalize();
    const std::string passwordHashHex = HexOf(pw.GetDigest(), 20);
    BigNumber s; s.SetHexStr("A5E1D9C4B3F2011E8D7C6B5A49382716F5E4D3C2B1A09F8E7D6C5B4A39281706");
    BigNumber b; b.SetHexStr("3C9F1E2D4B5A69788796A5B4C3D2E1F00112");
    BigNumber A; A.SetHexStr("12F3E4D5C6B7A8990A1B2C3D4E5F60718293A4B5C6D7E8F90A1B2C3D4E5F6071");
    const uint8 R1[16] = { 0xDE,0xAD,0xBE,0xEF,0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,0xFE,0xDC,0xBA,0x98 };
    BigNumber challenge; challenge.SetHexStr("0F1E2D3C4B5A69788796A5B4C3D2E1F0");
    std::printf("    /// A full SRP6 handshake with fixed inputs, through the Srp6 unit realmd calls.\n");
    std::printf("    struct Srp6Vector { const char* name; const char* login; const char* passwordHashHex; const char* s; const char* b; const char* A; const char* R1; const char* challenge; const char* v; const char* B; const char* u; const char* K; const char* M1; const char* M2; const char* R2; };\n");
    std::printf("    inline const Srp6Vector kSrp6 =\n");
    EmitHandshake("full width", login, passwordHashHex, s, b, A, R1, challenge, ";");
    std::printf("\n");

    // Handshakes in which one quantity has a zero top byte. Each is hashed at its protocol
    // width (32, 40 or 16 bytes); a minimal encoding would pass the full-width handshake
    // above and fail every one of these.
    BigNumber sShort; sShort.SetHexStr("00E1D9C4B3F2011E8D7C6B5A49382716F5E4D3C2B1A09F8E7D6C5B4A39281706");          // 31 bytes
    BigNumber AShort; AShort.SetHexStr("00F3E4D5C6B7A8990A1B2C3D4E5F60718293A4B5C6D7E8F90A1B2C3D4E5F6071");          // 31 bytes
    BigNumber challengeShort; challengeShort.SetHexStr("000E2D3C4B5A69788796A5B4C3D2E1F0");                          // 15 bytes
    std::printf("    /// Handshakes in which one quantity has a zero top byte: the widths, not the values, decide.\n");
    std::printf("    inline const Srp6Vector kSrp6Short[] = {\n");
    EmitHandshake("short A", login, passwordHashHex, s, b, AShort, R1, challenge, ",");
    EmitHandshake("short s", login, passwordHashHex, sShort, b, A, R1, challenge, ",");
    EmitHandshake("short challenge", login, passwordHashHex, s, b, A, R1, challengeShort, ",");
    EmitHandshake("short B", login, passwordHashHex, s,
                  SearchB(passwordHashHex, s, A, [](BigNumber& B, BigNumber&, BigNumber&) { return B.GetNumBytes() < Srp6::EphemeralWidth; }),
                  A, R1, challenge, ",");
    EmitHandshake("short S", login, passwordHashHex, s,
                  SearchB(passwordHashHex, s, A, [](BigNumber&, BigNumber& S, BigNumber&) { return S.GetNumBytes() < Srp6::EphemeralWidth; }),
                  A, R1, challenge, ",");
    EmitHandshake("short K", login, passwordHashHex, s,
                  SearchB(passwordHashHex, s, A, [](BigNumber&, BigNumber&, BigNumber& K) { return K.GetNumBytes() < Srp6::SessionKeyWidth; }),
                  A, R1, challenge, ",");
    std::printf("    };\n\n");

    // A salt whose top byte is zero (31 significant bytes) and a password hash whose top
    // byte is zero (19 significant bytes): both are hashed at their protocol widths.
    const std::string shortHash = "00A1B2C3D4E5F60718293A4B5C6D7E8F90A1B2C3";
    BigNumber vShort = Srp6::Verifier(sShort, shortHash);
    std::printf("    /// Verifier for a salt and a password hash whose top bytes are zero: the widths, not the values, decide.\n");
    std::printf("    struct VerifierVector { const char* s; const char* passwordHashHex; const char* v; };\n");
    std::printf("    inline const VerifierVector kVerifierShort = { \"%s\", \"%s\", \"%s\" };\n\n",
                HexOf(sShort).c_str(), shortHash.c_str(), HexOf(vShort).c_str());

    // ---------------------------------------------------------------- AuthCrypt keystreams
    BigNumber Kc; Kc.SetHexStr("C4D1B0A9F8E7D6C5B4A3928170F1E2D3C4B5A6978897A6B5C4D3E2F10102030405060708090A0B0C");
    uint8 zeros[64]; std::memset(zeros, 0, sizeof zeros);
    AuthCrypt enc; enc.Init(&Kc); uint8 sendStream[64]; std::memcpy(sendStream, zeros, 64); enc.EncryptSend(sendStream, 64);
    AuthCrypt dec; dec.Init(&Kc); uint8 recvStream[64]; std::memcpy(recvStream, zeros, 64); dec.DecryptRecv(recvStream, 64);
    std::printf("    /// AuthCrypt keystreams for a fixed session key: the first 64 bytes in each direction.\n");
    std::printf("    struct AuthCryptVector { const char* K; const char* serverEncrypt; const char* clientDecrypt; };\n");
    std::printf("    inline const AuthCryptVector kAuthCrypt = {\n        \"%s\",\n        \"%s\",\n        \"%s\"\n    };\n\n",
                HexOf(Kc).c_str(), HexOf(sendStream, 64).c_str(), HexOf(recvStream, 64).c_str());

    // ---------------------------------------------------------------- HMAC over a short key at its full width
    BigNumber Ks; Ks.SetHexStr("00A1B2C3D4E5F60718293A4B5C6D7E8F90A1B2C3D4E5F60718293A4B5C6D7E8F90A1B2C3D4E5F607");
    const uint8 data[6] = { 0x7F, 0x00, 0x00, 0x01, 0x95, 0x1F };
    HMACSHA1 h(40, Ks.AsByteArray(40));
    h.UpdateData(data, 6); h.Finalize();
    std::printf("    /// HMAC-SHA1 keyed with a 40-byte session key whose top byte is zero (39 significant bytes),\n");
    std::printf("    /// taken at its 40-byte protocol width (AsByteArray(40) pads at the high end), over six bytes.\n");
    std::printf("    struct HmacVector { const char* K; const char* data; const char* hmac; };\n");
    std::printf("    inline const HmacVector kHmacShortKey = { \"%s\", \"%s\", \"%s\" };\n\n",
                HexOf(Ks).c_str(), HexOf(data, 6).c_str(), HexOf(h.GetDigest(), 20).c_str());

    // ---------------------------------------------------------------- RSA: the redirect's raw signature
    BigNumber n, d, e(65537u);
    n.SetHexStr(TEST_MODULUS);
    d.SetHexStr(TEST_PRIVATE_EXPONENT);
    std::vector<std::vector<uint8>> messages;
    {
        std::vector<uint8> probe(256, 0x5A); probe[0] = 0x00;           // RedirectSigner::Load's probe
        messages.push_back(probe);
        std::vector<uint8> block(256); for (auto& x : block) x = uint8(rng()); block[0] = 0x00;   // a redirect-shaped block
        messages.push_back(block);
        std::vector<uint8> one(256, 0x00); one[255] = 0x01;             // 1^d = 1
        messages.push_back(one);
        std::vector<uint8> big(256, 0xFF); big[0] = 0x00;                // largest 255-byte value
        messages.push_back(big);
    }
    std::printf("    /// Raw RSA-2048 (no padding) under the throwaway pair ConnectToTest uses: signature = m^d mod n,\n");
    std::printf("    /// big-endian at 256 bytes; the public operation with e = 65537 recovers the message.\n");
    std::printf("    inline const char* kRsaModulus = \"%s\";\n", TEST_MODULUS);
    std::printf("    inline const char* kRsaPrivateExponent = \"%s\";\n", TEST_PRIVATE_EXPONENT);
    std::printf("    struct RsaCase { const char* message; const char* signature; };\n");
    std::printf("    inline const RsaCase kRsa[] = {\n");
    for (auto& m : messages)
    {
        BigNumber message; SetBigEndian(message, m.data(), m.size());
        BigNumber signature = message.ModExp(d, n);
        BigNumber recovered = signature.ModExp(e, n);
        if (std::memcmp(recovered.AsByteArray(256, false), m.data(), 256) != 0) { std::fprintf(stderr, "RSA recovery failed\n"); return 1; }
        std::printf("    {\"%s\",\n     \"%s\"},\n", HexOf(m.data(), 256).c_str(), HexOf(signature.AsByteArray(256, false), 256).c_str());
    }
    std::printf("    };\n");
    std::printf("}\n\n#endif\n");

    // ================================================================ timing baseline (stderr)
    std::fprintf(stderr, "\n## OpenSSL baseline (%s, %s)\n\n", std::getenv("COMPUTERNAME") ? std::getenv("COMPUTERNAME") : "?",
                 std::getenv("PROCESSOR_IDENTIFIER") ? std::getenv("PROCESSOR_IDENTIFIER") : "?");
    std::fprintf(stderr, "| operation | median | runs |\n|---|---|---|\n");

    // keygen
    EVP_PKEY* generated = nullptr;
    double keygen = MedianMicros(5, [&] { if (generated) EVP_PKEY_free(generated); generated = EVP_RSA_gen(2048); });
    std::fprintf(stderr, "| `EVP_RSA_gen(2048)` | %.1f ms | 5 |\n", keygen / 1000.0);

    // raw private op with CRT + blinding on the generated key (EVP_PKEY_sign, no padding)
    {
        EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(generated, nullptr);
        EVP_PKEY_sign_init(ctx);
        EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_NO_PADDING);
        std::vector<uint8> msg(256, 0x5A); msg[0] = 0;
        std::vector<uint8> sig(256); size_t siglen = 256;
        double t = MedianMicros(200, [&] { siglen = 256; EVP_PKEY_sign(ctx, sig.data(), &siglen, msg.data(), msg.size()); });
        std::fprintf(stderr, "| RSA-2048 private op, CRT + blinding (`EVP_PKEY_sign`, `RSA_NO_PADDING`) | %.0f us | 200 |\n", t);
        EVP_PKEY_CTX_free(ctx);

        EVP_PKEY_CTX* vctx = EVP_PKEY_CTX_new(generated, nullptr);
        EVP_PKEY_encrypt_init(vctx);
        EVP_PKEY_CTX_set_rsa_padding(vctx, RSA_NO_PADDING);
        std::vector<uint8> out(256); size_t outlen = 256;
        double tv = MedianMicros(200, [&] { outlen = 256; EVP_PKEY_encrypt(vctx, out.data(), &outlen, msg.data(), msg.size()); });
        std::fprintf(stderr, "| RSA-2048 public op, e = 65537 (`EVP_PKEY_encrypt`, `RSA_NO_PADDING`) | %.1f us | 200 |\n", tv);
        EVP_PKEY_CTX_free(vctx);
    }
    EVP_PKEY_free(generated);

    // the server's actual path today: BigNumber::ModExp (BN_mod_exp) with the 2048-bit private exponent, no CRT
    {
        BigNumber message; std::vector<uint8> m(256, 0x5A); m[0] = 0; SetBigEndian(message, m.data(), 256);
        double t = MedianMicros(50, [&] { BigNumber sgn = message.ModExp(d, n); (void)sgn; });
        std::fprintf(stderr, "| RSA-2048 private op, no CRT, `BigNumber::ModExp` (= `BN_mod_exp`) — the redirect today | %.0f us | 50 |\n", t);
        double tp = MedianMicros(200, [&] { BigNumber r = message.ModExp(e, n); (void)r; });
        std::fprintf(stderr, "| RSA-2048 public op, `BigNumber::ModExp` | %.1f us | 200 |\n", tp);

        BN_CTX* bnctx = BN_CTX_new();
        BN_MONT_CTX* mont = BN_MONT_CTX_new();
        BN_MONT_CTX_set(mont, n.BN(), bnctx);
        BIGNUM* r = BN_new();
        double tc = MedianMicros(50, [&] { BN_mod_exp_mont_consttime(r, message.BN(), d.BN(), n.BN(), bnctx, mont); });
        std::fprintf(stderr, "| RSA-2048 private op, no CRT, `BN_mod_exp_mont_consttime` | %.0f us | 50 |\n", tc);
        BN_free(r); BN_MONT_CTX_free(mont); BN_CTX_free(bnctx);
    }

    // SRP6: one login's server-side arithmetic
    {
        double t = MedianMicros(200, [&] {
            BigNumber vv = Srp6::Verifier(s, passwordHashHex);
            BigNumber BB = Srp6::ServerEphemeral(vv, b);
            BigNumber uu = Srp6::Scrambler(A, BB);
            BigNumber KK = Srp6::SessionKey(A, vv, uu, b);
            (void)KK;
        });
        std::fprintf(stderr, "| SRP6 login (Verifier + ServerEphemeral + Scrambler + SessionKey: three 256-bit `ModExp`) | %.1f us | 200 |\n", t);
    }

    // hashes
    {
        std::vector<uint8> buf(1 << 16, 0xA5);
        const int chunks = 1024;   // 64 MiB
        double t = MedianMicros(3, [&] { Sha1Hash sh; for (int i = 0; i < chunks; ++i) sh.UpdateData(buf.data(), int(buf.size())); sh.Finalize(); });
        std::fprintf(stderr, "| SHA-1 (`Sha1Hash`, 64 MiB in 64 KiB updates) | %.0f MB/s | 3 |\n", 64.0 * 1048576.0 / t);
        double th = MedianMicros(3, [&] { HMACSHA1 hm(40, Ks.AsByteArray(40)); for (int i = 0; i < chunks; ++i) hm.UpdateData(buf.data(), int(buf.size())); hm.Finalize(); });
        std::fprintf(stderr, "| HMAC-SHA1 (`HMACSHA1`, 64 MiB in 64 KiB updates) | %.0f MB/s | 3 |\n", 64.0 * 1048576.0 / th);
    }
    return 0;
}
