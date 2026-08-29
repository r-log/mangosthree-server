/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "ConnectTo.h"

#include "Auth/HMACSHA1.h"
#include "Log/Log.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <vector>

namespace proto
{
    namespace
    {
        /// HMAC key the client keeps in its image and uses for both tags.
        const uint8 SEED64[64] =
        {
            0x2C, 0x1F, 0x1D, 0x80, 0xC3, 0x8C, 0x23, 0x64, 0xDA, 0x90, 0xCA, 0x8E, 0x2C, 0xFC, 0x0C, 0xCE,
            0x09, 0xD3, 0x62, 0xF9, 0xF3, 0x8B, 0xBE, 0x9F, 0x19, 0xEF, 0x58, 0xA1, 0x1C, 0x34, 0x14, 0x41,
            0x3F, 0x23, 0xFD, 0xD3, 0xE8, 0x14, 0xEC, 0x2A, 0xFD, 0x4F, 0x95, 0xBA, 0x30, 0x7E, 0x56, 0x5D,
            0x83, 0x95, 0x81, 0x69, 0xB0, 0x5A, 0xB4, 0x9D, 0xA8, 0x55, 0xFF, 0xFC, 0xEE, 0x58, 0x0A, 0x2F
        };

        /**
         * @brief Where each plaintext byte lands in the struct the client builds.
         *
         * The client does not read the 256-byte plaintext as a struct. It walks it
         * one byte at a time and scatters each byte to a fixed offset, so the
         * fields are interleaved beyond recognition on the wire and contiguous
         * only once reassembled. Byte 255 of the plaintext is never read.
         *
         * Index 212 targets offset 16, which the client's deserialiser writes as a
         * dword -- so offsets 17 through 19 are zeroed by that store no matter what
         * we put in them. Anything signed there is lost, which is why the block is
         * round-tripped through this permutation before the tag is computed.
         */
        const uint16 KPERM[255] =
        {
            126,  54, 117, 122,  26, 174,  39,  78,  77, 164, 155, 211,  88, 253,  48, 128,
            197,  60,  43,  52, 184,  32,  86,  99, 118,  37, 134, 142,   1, 190, 192, 233,
             65,  44, 109, 252, 159, 112,  28, 154,  90,  76, 175,  14,   9, 221,  47,  29,
            236,  31, 208, 229, 170, 231, 146, 110, 181, 257, 148, 255, 232, 167,   6, 183,
            124, 201,  55, 171,  69, 101, 200,  40, 160,  45,   3,  21,  46,  96,   5, 206,
            205, 189,  33,  20,  49,  36, 186, 168, 102, 150,   0, 119, 196,  62, 182, 178,
             79, 191, 256, 209,  61, 246, 188, 138, 216, 151, 220,  53, 100,  41, 219,   8,
            157, 218,  42, 149,   2, 249, 127, 132,  25,  80,  63, 166,  57, 228, 116, 203,
            237, 193, 240,  83, 198, 212, 145, 133, 144, 114,  92, 250, 161, 241,  50, 214,
            140,  73,  82, 177, 147,   4, 108, 123,  11, 136, 235,  74,  87, 207, 227,  24,
             72, 137,  51, 173, 172, 104,  12,  85, 115, 113, 120,  95, 251,  84, 125, 111,
              7, 217, 165,  13,  58, 199, 135, 243,  27, 130, 152,  22, 248, 176, 226,  35,
            230, 129, 121, 169, 234, 105, 162,  97,  68, 222, 163,  56, 106, 242, 247,  67,
            156, 194, 187, 239,  16, 107, 179, 238,  75,  66, 143,  34, 225,  23, 153, 103,
            245,  38,  81,  89, 223,  64,  10,  94, 195, 213, 158, 185,  93, 244, 210, 202,
            204,  71,  30,  98, 139,  59, 141, 215,  70,  91, 254, 224,  15, 131, 180
        };

        // Field offsets inside the reassembled struct.
        const size_t D_ADDRESS    = 0x00;  ///< 16 bytes: IPv4 in the first four, or IPv6
        const size_t D_FAMILY     = 0x10;  ///< 4 bytes, but only the first survives
        const size_t D_CONTROL    = 0x14;  ///< 1 byte, must be 0x2A
        const size_t D_PORT       = 0x15;  ///< 2 bytes, low byte first
        const size_t D_AUTH       = 0x17;  ///< 73 bytes
        const size_t D_PI         = 0x60;  ///< 142 bytes
        const size_t D_TAG        = 0xEE;  ///< 20 bytes

        const size_t D_ADDRESS_LEN = 16;
        const size_t D_FAMILY_LEN  = 4;
        const size_t D_PORT_LEN    = 2;
        const size_t D_AUTH_LEN    = 73;
        const size_t D_PI_LEN      = 142;
        const size_t D_TAG_LEN     = 20;

        const uint8 CONTROL_VALUE = 0x2A;

        /// The RSA field, and the wrapper that carries it.
        const size_t RSA_FIELD_LEN = 256;

        /**
         * @brief The 142 bytes of pi in BCD the client expects to find.
         *
         * The client does not carry these as data. It regenerates them with a
         * spigot -- a digit-extraction algorithm with no input but its own length
         * -- and rejects the block if a single byte differs. This is a
         * transcription of that routine; the point is not the digits but that both
         * sides produce the same 142 bytes without exchanging them.
         */
        const std::array<uint8, D_PI_LEN>& PiBcdBlock()
        {
            static const std::array<uint8, D_PI_LEN> block = []
            {
                std::array<uint8, D_PI_LEN> out{};
                size_t written = 0;

                const int64 columns = 14 * (0x11C / 4);
                std::vector<int64> work(size_t(columns) + 1, 2000);

                int64 carry = 0;
                int64 column = columns;
                int64 divisor = 2 * columns - 1;

                while (column > 0)
                {
                    int64 index = column;
                    int64 accumulator = 0;

                    for (int64 d = divisor; index > 0; --index, d -= 2)
                    {
                        const int64 value = 10000 * work[size_t(index)] + accumulator * index;
                        work[size_t(index)] = value % d;
                        accumulator = value / d;
                    }

                    const int64 digits = accumulator / 10000 + carry;

                    out[written++] = uint8((16 * (digits / 1000)) | ((digits % 1000) / 100));
                    out[written++] = uint8((16 * ((digits % 100) / 10)) | (digits % 10));

                    divisor -= 28;
                    carry = accumulator % 10000;
                    column -= 14;
                }

                return out;
            }();

            return block;
        }

        std::array<uint8, 20> Tag(const uint8* message, size_t length)
        {
            uint8 key[sizeof(SEED64)];
            std::memcpy(key, SEED64, sizeof(key));

            HMACSHA1 hmac(uint32(sizeof(key)), key);
            hmac.UpdateData(message, int(length));
            hmac.Finalize();

            std::array<uint8, 20> digest{};
            std::memcpy(digest.data(), hmac.GetDigest(), digest.size());
            return digest;
        }

        bool DecodeHex(const std::string& text, std::vector<uint8>& out)
        {
            // An odd digit count is a number written without its leading zero,
            // not a malformed value: a key generator that prints the private
            // exponent as a plain integer drops it about one time in sixteen.
            // Refusing that would be a keypair the operator cannot tell from a
            // typo, so pad instead. Fields that really are byte strings are
            // caught by their own length checks below.
            const std::string padded = (text.size() % 2) != 0 ? "0" + text : text;

            out.clear();
            out.reserve(padded.size() / 2);

            for (size_t i = 0; i < padded.size(); i += 2)
            {
                uint8 byte = 0;
                for (size_t half = 0; half < 2; ++half)
                {
                    const char c = padded[i + half];
                    uint8 nibble;

                    if (c >= '0' && c <= '9')      { nibble = uint8(c - '0'); }
                    else if (c >= 'a' && c <= 'f') { nibble = uint8(c - 'a' + 10); }
                    else if (c >= 'A' && c <= 'F') { nibble = uint8(c - 'A' + 10); }
                    else                           { return false; }

                    byte = uint8((byte << 4) | nibble);
                }
                out.push_back(byte);
            }

            return true;
        }

        std::string EncodeHex(const uint8* data, size_t length)
        {
            static const char digits[] = "0123456789ABCDEF";

            std::string text;
            text.reserve(length * 2);
            for (size_t i = 0; i < length; ++i)
            {
                text.push_back(digits[data[i] >> 4]);
                text.push_back(digits[data[i] & 0x0F]);
            }
            return text;
        }

        std::string Trim(const std::string& text)
        {
            const char* space = " \t\r\n\"";

            const size_t first = text.find_first_not_of(space);
            if (first == std::string::npos)
            {
                return std::string();
            }
            return text.substr(first, text.find_last_not_of(space) - first + 1);
        }

        /// BigNumber::SetBinary reads little-endian; every quantity here is big.
        void SetBigEndian(BigNumber& number, const uint8* data, size_t length)
        {
            std::vector<uint8> reversed(data, data + length);
            std::reverse(reversed.begin(), reversed.end());
            number.SetBinary(reversed.data(), int(reversed.size()));
        }
    }

    RedirectSigner::RedirectSigner()
        : m_authBlob{},
          m_loaded(false)
    {
    }

    bool RedirectSigner::LoadFromFile(const std::string& path)
    {
        m_loaded = false;

        std::ifstream file(path.c_str());
        if (!file.good())
        {
            sLog.outError("proto: cannot read the redirect secret at '%s'. "
                          "Generate one with secret-gen and point "
                          "Redirect.SecretFile at it.", path.c_str());
            return false;
        }

        std::string modulus;
        std::string exponent;
        std::string authBlob;

        std::string line;
        while (std::getline(file, line))
        {
            const size_t comment = line.find('#');
            if (comment != std::string::npos)
            {
                line.erase(comment);
            }

            const size_t equals = line.find('=');
            if (equals == std::string::npos)
            {
                continue;
            }

            const std::string key   = Trim(line.substr(0, equals));
            const std::string value = Trim(line.substr(equals + 1));

            if (key == "Modulus")              { modulus  = value; }
            else if (key == "PrivateExponent") { exponent = value; }
            else if (key == "AuthBlob")        { authBlob = value; }
        }

        if (modulus.empty() || exponent.empty())
        {
            sLog.outError("proto: '%s' is missing Modulus or PrivateExponent. "
                          "It is a server.secret this file wants, not a client.secret.",
                          path.c_str());
            return false;
        }

        return Load(modulus, exponent, authBlob);
    }

    bool RedirectSigner::Load(const std::string& modulusHex,
                              const std::string& privateExponentHex,
                              const std::string& authBlobHex)
    {
        m_loaded = false;

        if (modulusHex.empty() || privateExponentHex.empty())
        {
            return false;
        }

        std::vector<uint8> modulus;
        std::vector<uint8> exponent;

        if (!DecodeHex(modulusHex, modulus) || !DecodeHex(privateExponentHex, exponent))
        {
            sLog.outError("proto: redirect key is not valid hex");
            return false;
        }

        if (modulus.size() != 256)
        {
            sLog.outError("proto: redirect modulus is %zu bytes, must be 256 (RSA-2048)",
                          modulus.size());
            return false;
        }

        m_authBlob.fill(0);

        if (!authBlobHex.empty())
        {
            std::vector<uint8> blob;
            if (!DecodeHex(authBlobHex, blob) || blob.size() != D_AUTH_LEN)
            {
                sLog.outError("proto: redirect auth blob must be %zu hex-encoded bytes",
                              D_AUTH_LEN);
                return false;
            }
            std::copy(blob.begin(), blob.end(), m_authBlob.begin());
        }

        SetBigEndian(m_modulus, modulus.data(), modulus.size());
        SetBigEndian(m_privateExponent, exponent.data(), exponent.size());

        m_loaded = true;
        return true;
    }

    std::string RedirectSigner::ExpectedClientDigest() const
    {
        std::array<uint8, D_AUTH_LEN * 2> doubled{};
        std::copy(m_authBlob.begin(), m_authBlob.end(), doubled.begin());
        std::copy(m_authBlob.begin(), m_authBlob.end(), doubled.begin() + D_AUTH_LEN);

        const std::array<uint8, 20> digest = Tag(doubled.data(), doubled.size());
        return EncodeHex(digest.data(), digest.size());
    }

    std::array<uint8, 256> RedirectSigner::BlockToPlaintext(const Block& d)
    {
        std::array<uint8, 256> plaintext{};
        for (size_t i = 0; i < 255; ++i)
        {
            plaintext[i] = d[KPERM[i]];
        }
        return plaintext;
    }

    RedirectSigner::Block RedirectSigner::PlaintextToBlock(const std::array<uint8, 256>& plaintext)
    {
        Block d{};
        for (size_t i = 0; i < 255; ++i)
        {
            d[KPERM[i]] = plaintext[i];
        }
        return d;
    }

    bool RedirectSigner::BuildConnectTo(const std::vector<uint8>& address,
                                        RedirectFamily family,
                                        uint16 port,
                                        LinkSlot target,
                                        WorldPacket& out) const
    {
        if (!m_loaded)
        {
            return false;
        }

        const size_t expected = family == RedirectFamily::IPv4 ? 4 : 16;
        if (address.size() != expected)
        {
            sLog.outError("proto: redirect address is %zu bytes, expected %zu for this family",
                          address.size(), expected);
            return false;
        }

        Block d{};

        std::copy(address.begin(), address.end(), d.begin() + D_ADDRESS);
        d[D_FAMILY]  = uint8(family);
        d[D_CONTROL] = CONTROL_VALUE;
        d[D_PORT]     = uint8(port & 0xFF);
        d[D_PORT + 1] = uint8((port >> 8) & 0xFF);
        std::copy(m_authBlob.begin(), m_authBlob.end(), d.begin() + D_AUTH);

        const std::array<uint8, D_PI_LEN>& pi = PiBcdBlock();
        std::copy(pi.begin(), pi.end(), d.begin() + D_PI);

        // Three of the struct's bytes cannot survive the trip, so tag the block
        // the client will actually hold rather than the one assembled above.
        d = PlaintextToBlock(BlockToPlaintext(d));

        std::vector<uint8> tagged;
        tagged.reserve(D_ADDRESS_LEN + D_FAMILY_LEN + D_PORT_LEN + D_AUTH_LEN + D_PI_LEN + 1);
        tagged.insert(tagged.end(), d.begin() + D_ADDRESS, d.begin() + D_ADDRESS + D_ADDRESS_LEN);
        tagged.insert(tagged.end(), d.begin() + D_FAMILY,  d.begin() + D_FAMILY  + D_FAMILY_LEN);
        tagged.insert(tagged.end(), d.begin() + D_PORT,    d.begin() + D_PORT    + D_PORT_LEN);
        tagged.insert(tagged.end(), d.begin() + D_AUTH,    d.begin() + D_AUTH    + D_AUTH_LEN);
        tagged.insert(tagged.end(), d.begin() + D_PI,      d.begin() + D_PI      + D_PI_LEN);
        tagged.push_back(d[D_CONTROL]);

        const std::array<uint8, 20> tag = Tag(tagged.data(), tagged.size());
        std::copy(tag.begin(), tag.end(), d.begin() + D_TAG);

        const std::array<uint8, 256> plaintext = BlockToPlaintext(d);

        BigNumber message;
        SetBigEndian(message, plaintext.data(), plaintext.size());

        BigNumber signature = message.ModExp(m_privateExponent, m_modulus);

        // Run the client's own side of the operation before trusting the result.
        // Two failures land here and nowhere else: a modulus and private exponent
        // that are not a pair, and a plaintext that is not below the modulus --
        // the latter wraps, so the client recovers 256 bytes that were never
        // signed. Both would otherwise present as a client that silently ignores
        // the redirect, which is the same symptom as a dozen unrelated faults.
        BigNumber recovered = signature.ModExp(BigNumber(65537), m_modulus);

        uint8* recoveredBytes = recovered.AsByteArray(int(RSA_FIELD_LEN), false);
        if (std::memcmp(recoveredBytes, plaintext.data(), RSA_FIELD_LEN) != 0)
        {
            sLog.outError("proto: signed redirect does not recover to its plaintext; "
                          "check that Redirect.Modulus and Redirect.PrivateExponent "
                          "are the same keypair");
            return false;
        }

        uint8* signatureBytes = signature.AsByteArray(int(RSA_FIELD_LEN), false);

        // The wrapper the client reads: three dwords of its own bookkeeping,
        // which it stores and never sends back, then the signed field, then the
        // target stream. Zero the bookkeeping -- nothing reads it.
        out.Initialize(SMSG_CONNECT_TO, 4 + 4 + 4 + RSA_FIELD_LEN + 1);
        out << uint32(0);
        out << uint32(0);
        out << uint32(0);
        out.append(signatureBytes, RSA_FIELD_LEN);
        out << uint8(target);

        return true;
    }
}
