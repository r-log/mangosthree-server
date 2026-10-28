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

/// The world server takes the session key at the 40 bytes the client hashes and keys with
/// (Wow-64.exe 15595: RealmConnection's login digest, NetClient::ClientConnectionAuthChallengeHandler,
/// both SetEncryptionKey calls, all over sizeof(m_sessionKey)). A key whose top byte is zero, about
/// 1 in 256, has 39 significant bytes and is still hashed with its zero top byte.

#include "TestHarness.h"

#include "ClientConnection.h"
#include "IClientLink.h"
#include "IWorldGateway.h"
#include "Opcodes.h"
#include "RedirectRegistry.h"

#include "Auth/AuthCrypt.h"
#include "Auth/ARC4.h"
#include "Auth/BigNumber.h"
#include "Auth/HMACSHA1.h"
#include "Auth/Sha1.h"

#include <array>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace
{
    const char* const CLIENT_ADDRESS = "203.0.113.9";
    const char* const ACCOUNT        = "ADMIN";
    const char* const SHORT_KEY      =
        "00A1B2C3D4E5F60718293A4B5C6D7E8F90A1B2C3D4E5F60718293A4B5C6D7E8F90A1B2C3D4E5F607";

    /// A world that accepts the account with the given key and records whether the session
    /// was attached, which happens only once the client's proof checked out.
    class AcceptingGateway : public proto::IWorldGateway
    {
        public:
            explicit AcceptingGateway(BigNumber const& key) : m_key(key) {}

            proto::AuthLookup LookupAccount(const proto::AuthRequest&) override
            {
                proto::AuthLookup lookup;
                lookup.status = proto::AuthStatus::Ok;
                lookup.sessionKey = m_key;
                return lookup;
            }

            proto::SessionId Attach(const proto::AuthRequest&,
                                    const std::shared_ptr<proto::IClientLink>&,
                                    const std::shared_ptr<proto::AuthContext>&) override
            {
                attached = true;
                return 7;
            }

            void TracePacket(proto::SessionId, const WorldPacket&, bool, proto::LinkSlot) override {}
            void Deliver(proto::SessionId, WorldPacket&&) override {}
            void Detach(proto::SessionId) override {}

            bool attached = false;

        private:
            BigNumber m_key;
    };

    std::vector<uint8> Frame(uint16 opcode, const std::vector<uint8>& payload)
    {
        const uint32 size = uint32(payload.size()) + 4;

        std::vector<uint8> out;
        out.push_back(uint8((size >> 8) & 0xFF));
        out.push_back(uint8(size & 0xFF));
        out.push_back(uint8(opcode & 0xFF));
        out.push_back(uint8((opcode >> 8) & 0xFF));
        out.push_back(0);
        out.push_back(0);
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }

    std::vector<uint8> ClientBanner()
    {
        const char greeting[] = "WORLD OF WARCRAFT CONNECTION - CLIENT TO SERVER";

        std::vector<uint8> wire;
        wire.push_back(0x00);
        wire.push_back(0x30);
        wire.insert(wire.end(), greeting, greeting + sizeof(greeting));
        return wire;
    }

    /// The server nonce the connection put in its SMSG_AUTH_CHALLENGE: the four bytes after
    /// the 32-byte cipher seed. Server frames have a four-byte header (a big-endian size
    /// counting the opcode, then the opcode).
    uint32 ServerSeedOf(const std::vector<uint8>& wire)
    {
        size_t offset = 0;
        while (offset + 4 <= wire.size())
        {
            const uint32 size   = uint32(wire[offset]) << 8 | uint32(wire[offset + 1]);
            const uint16 opcode = uint16(wire[offset + 2]) | uint16(uint16(wire[offset + 3]) << 8);
            const size_t payloadLen = size - 2;
            if (opcode == SMSG_AUTH_CHALLENGE && payloadLen >= AuthCrypt::SeedLength + 4)
            {
                uint32 seed = 0;
                std::memcpy(&seed, wire.data() + offset + 4 + AuthCrypt::SeedLength, sizeof(seed));
                return seed;
            }
            offset += 4 + payloadLen;
        }
        return 0;
    }

    /// The client's login proof: SHA-1 over the account name, a zero, both halves of the nonce
    /// and `keyBytes` bytes of the session key.
    std::array<uint8, 20> LoginProof(BigNumber key, uint32 clientSeed, uint32 serverSeed, int keyBytes)
    {
        const uint32 zero = 0;

        Sha1Hash sha;
        sha.UpdateData(std::string(ACCOUNT));
        sha.UpdateData(reinterpret_cast<const uint8*>(&zero), 4);
        sha.UpdateData(reinterpret_cast<const uint8*>(&clientSeed), 4);
        sha.UpdateData(reinterpret_cast<const uint8*>(&serverSeed), 4);
        sha.UpdateData(key.AsByteArray(keyBytes), keyBytes);
        sha.Finalize();

        std::array<uint8, 20> digest{};
        std::memcpy(digest.data(), sha.GetDigest(), digest.size());
        return digest;
    }

    void Put32(std::vector<uint8>& out, uint32 value)
    {
        for (int i = 0; i < 4; ++i)
        {
            out.push_back(uint8(value >> (8 * i)));
        }
    }

    /// CMSG_AUTH_SESSION in the order ClientConnection::HandleAuthSession reads it, the digest
    /// bytes interleaved with the other fields, an empty addon block and the account name.
    std::vector<uint8> AuthSession(const std::array<uint8, 20>& d, uint32 clientSeed)
    {
        std::vector<uint8> p;
        Put32(p, 0);
        Put32(p, 0);
        p.push_back(0);
        p.push_back(d[10]);
        p.push_back(d[18]);
        p.push_back(d[12]);
        p.push_back(d[5]);
        Put32(p, 0);
        Put32(p, 0);
        p.push_back(d[15]);
        p.push_back(d[9]);
        p.push_back(d[19]);
        p.push_back(d[4]);
        p.push_back(d[7]);
        p.push_back(d[16]);
        p.push_back(d[3]);
        p.push_back(uint8(15595 & 0xFF));
        p.push_back(uint8(15595 >> 8));
        p.push_back(d[8]);
        Put32(p, 0);
        p.push_back(0);
        p.push_back(d[17]);
        p.push_back(d[6]);
        p.push_back(d[0]);
        p.push_back(d[1]);
        p.push_back(d[11]);
        Put32(p, clientSeed);
        p.push_back(d[2]);
        Put32(p, 0);
        p.push_back(d[14]);
        p.push_back(d[13]);
        Put32(p, 0);

        const std::string account = ACCOUNT;
        p.push_back(uint8(account.size() >> 5));
        p.push_back(uint8((account.size() & 0x1F) << 3));
        p.insert(p.end(), account.begin(), account.end());
        return Frame(CMSG_AUTH_SESSION, p);
    }

    /// Logs in on a fresh stream-0 connection with the proof taken over `keyBytes` bytes of the
    /// key; true when the world attached the session.
    bool LogsIn(BigNumber const& key, int keyBytes)
    {
        AcceptingGateway gateway(key);
        proto::RedirectRegistry registry(std::chrono::milliseconds(60000));
        proto::EndpointPolicy policy;

        std::shared_ptr<proto::ClientConnection> connection =
            std::make_shared<proto::ClientConnection>(gateway, registry, policy);
        connection->setPeerAddress(CLIENT_ADDRESS);
        connection->setSender([](const uint8_t*, size_t) {});
        connection->setCloser([]() {});

        const uint32 serverSeed = ServerSeedOf(connection->onConnect());
        const uint32 clientSeed = 0x1234ABCD;

        const std::vector<uint8> banner = ClientBanner();
        connection->onData(banner.data(), banner.size());
        const std::vector<uint8> session = AuthSession(LoginProof(key, clientSeed, serverSeed, keyBytes), clientSeed);
        connection->onData(session.data(), session.size());
        return gateway.attached;
    }
}

TEST(SessionKeyWidth_a_login_with_a_key_whose_top_byte_is_zero_proves_the_full_key)
{
    BigNumber key;
    key.SetHexStr(SHORT_KEY);
    REQUIRE(key.GetNumBytes() == 39);

    CHECK(LogsIn(key, AuthCrypt::SessionKeyLength));
}

TEST(SessionKeyWidth_a_proof_over_the_significant_bytes_alone_is_refused)
{
    BigNumber key;
    key.SetHexStr(SHORT_KEY);
    REQUIRE(key.GetNumBytes() == 39);

    CHECK(!LogsIn(key, 39));
}

TEST(SessionKeyWidth_the_cipher_is_keyed_from_all_40_bytes)
{
    BigNumber key;
    key.SetHexStr(SHORT_KEY);
    REQUIRE(key.GetNumBytes() == 39);

    uint8 seed[AuthCrypt::SeedLength];
    for (size_t i = 0; i < sizeof(seed); ++i)
    {
        seed[i] = uint8(0x40 + i);
    }

    // The outgoing direction as the client keys its incoming one: ARC4 under HMAC-SHA1 of the
    // 40-byte key with the first 16 seed bytes, its first 1024 bytes dropped.
    HMACSHA1 hmac(SEED_KEY_SIZE, seed);
    hmac.UpdateData(key.AsByteArray(40), 40);
    hmac.Finalize();
    ARC4 reference(SHA_DIGEST_LENGTH);
    reference.Init(hmac.GetDigest());
    std::vector<uint8> drop(1024, 0);
    reference.UpdateData(int(drop.size()), drop.data());
    uint8 expected[16] = {};
    reference.UpdateData(int(sizeof(expected)), expected);

    AuthCrypt crypt;
    crypt.Init(&key, seed);
    uint8 sent[16] = {};
    crypt.EncryptSend(sent, sizeof(sent));

    CHECK(std::memcmp(sent, expected, sizeof(sent)) == 0);
}
