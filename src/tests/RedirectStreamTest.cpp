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

// The second world stream, from the connection's own point of view: what the
// 4.3.4 client sends on the redirected socket, in what order, and in the clear
// or enciphered. Every assertion here was read out of Wow-64.exe 15595 and then
// seen on the wire on 2026-08-30 -- the first live redirect this fork completed.
//
// The sequence, from the client's sub_1400AA560:
//
//   server: MSG_WOW_CONNECTION, SMSG_AUTH_CHALLENGE      (both plain text)
//   client: MSG_WOW_CONNECTION                           (plain text)
//   client: CMSG_AUTH_CONTINUED_SESSION                  (PLAIN TEXT, and only
//           then does the client key its ciphers)
//   both:   enciphered headers, keyed from the session key and the 32 bytes the
//           challenge carried
//
// Getting the arming point wrong by one packet cost a login: the server armed on
// the banner, decrypted a plaintext header into noise, and dropped the socket --
// "malformed packet framing ... 42 byte(s) read, 0 packet(s) decoded before it".

#include "TestHarness.h"

#include "ClientConnection.h"
#include "IClientLink.h"
#include "IWorldGateway.h"
#include "OpcodeSlots.h"
#include "PacketCodec.h"
#include "RedirectRegistry.h"
#include "SessionLinks.h"

#include "Auth/AuthCrypt.h"
#include "Auth/Sha1.h"
#include "Auth/BigNumber.h"

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace
{
    const char* const CLIENT_ADDRESS = "203.0.113.7";
    const char* const ACCOUNT        = "ADMIN";
    const proto::SessionId SESSION   = 42;

    // Wire values, written out rather than taken from the opcode table: this
    // test is about what the client puts on the wire, so the numbers are the
    // subject and not a detail to be inherited. The asserts below pin the table
    // to them -- CMSG_AUTH_CONTINUED_SESSION carried a placeholder (0x1513) until
    // this handshake was traced, which is why one of them is worth having.
    const uint16 WIRE_WOW_CONNECTION          = 0x4F57;
    const uint16 WIRE_AUTH_CHALLENGE          = 0x4542;
    const uint16 WIRE_AUTH_CONTINUED_SESSION  = 0x044D;
    const uint16 WIRE_PING                    = 0x444D;
    const uint16 WIRE_RESUME_COMMS            = 0x0140;

    static_assert(WIRE_WOW_CONNECTION == MSG_WOW_CONNECTION, "opcode table drifted");
    static_assert(WIRE_AUTH_CHALLENGE == SMSG_AUTH_CHALLENGE, "opcode table drifted");
    static_assert(WIRE_AUTH_CONTINUED_SESSION == CMSG_AUTH_CONTINUED_SESSION,
                  "opcode table drifted");
    static_assert(WIRE_PING == CMSG_PING, "opcode table drifted");
    static_assert(WIRE_RESUME_COMMS == SMSG_RESUME_COMMS, "opcode table drifted");

    /// Stream 0, which the session already holds. It does nothing here; the
    /// SessionLinks under test only needs something to be its first stream.
    class SilentLink : public proto::IClientLink
    {
        public:
            void SendPacket(const WorldPacket&) override {}
            void Close() override {}
            const std::string& GetRemoteAddress() const override { return m_address; }
            bool IsClosed() const override { return false; }

        private:
            std::string m_address = CLIENT_ADDRESS;
    };

    /// A world that only records what reached it.
    class RecordingGateway : public proto::IWorldGateway
    {
        public:
            proto::AuthLookup LookupAccount(const proto::AuthRequest&) override
            {
                return proto::AuthLookup();
            }

            proto::SessionId Attach(const proto::AuthRequest&,
                                    const std::shared_ptr<proto::IClientLink>&,
                                    const std::shared_ptr<proto::AuthContext>&) override
            {
                return proto::INVALID_SESSION_ID;
            }

            void TracePacket(proto::SessionId, const WorldPacket&, bool) override {}

            void Deliver(proto::SessionId, WorldPacket&& packet) override
            {
                delivered.push_back(uint16(packet.GetOpcode()));
            }

            void Detach(proto::SessionId) override {}

            std::vector<uint16> delivered;
    };

    /// One framed packet, header in clear.
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

    /// The client's banner reply, byte for byte.
    std::vector<uint8> ClientBanner()
    {
        const char greeting[] = "WORLD OF WARCRAFT CONNECTION - CLIENT TO SERVER";

        std::vector<uint8> wire;
        wire.push_back(0x00);
        wire.push_back(0x30);
        wire.insert(wire.end(), greeting, greeting + sizeof(greeting));
        return wire;
    }

    /// The proof the client puts in CMSG_AUTH_CONTINUED_SESSION: SHA-1 over the
    /// account name, the 40-byte session key and the 4-byte server seed, in that
    /// order (Wow-64.exe 15595, sub_1400AA560).
    std::array<uint8, 20> StreamProof(const BigNumber& sessionKey, uint32 serverSeed)
    {
        BigNumber key = sessionKey;

        Sha1Hash sha;
        sha.UpdateData(std::string(ACCOUNT));
        sha.UpdateBigNumbers(&key, NULL);
        sha.UpdateData(reinterpret_cast<const uint8*>(&serverSeed), sizeof(serverSeed));
        sha.Finalize();

        std::array<uint8, 20> digest{};
        std::memcpy(digest.data(), sha.GetDigest(), digest.size());
        return digest;
    }

    /// CMSG_AUTH_CONTINUED_SESSION as sub_1400AA560 builds it, byte for byte --
    /// the shape a live capture confirmed on 2026-08-30:
    ///   uint64 key (the connect-to key, echoed), uint64 counter, uint8 digest[20]
    /// Where each digest byte sits on the wire: the client interleaves them, as
    /// it does in CMSG_AUTH_SESSION, and this is that order. Derived from live
    /// handshakes on 2026-08-30 -- see ClientConnection::VerifyContinuedSession.
    const uint8 DIGEST_WIRE_ORDER[20] =
    {
        5, 2, 6, 10, 8, 17, 11, 15, 7, 1, 4, 16, 0, 12, 14, 13, 18, 9, 19, 3
    };

    std::vector<uint8> ContinuedSession(const std::array<uint8, 20>& digest)
    {
        std::vector<uint8> payload(36, 0);
        payload[8] = 0x02;   // the proof-of-work counter it searched for
        for (size_t i = 0; i < 20; ++i)
        {
            payload[16 + i] = digest[DIGEST_WIRE_ORDER[i]];
        }
        return Frame(WIRE_AUTH_CONTINUED_SESSION, payload);
    }

    /// Everything a staged stream-1 connection needs to exist.
    struct Fixture
    {
        RecordingGateway gateway;
        proto::RedirectRegistry registry;
        std::shared_ptr<proto::SessionLinks> links;
        std::shared_ptr<proto::ClientConnection> connection;

        std::vector<uint8> sent;      ///< what the connection wrote after onConnect
        bool closed = false;

        BigNumber sessionKey;
        uint32    serverSeed = 0;     ///< read out of the challenge this connection sent

        Fixture()
            : registry(std::chrono::milliseconds(60000)),
              links(std::make_shared<proto::SessionLinks>(std::make_shared<SilentLink>()))
        {
            sessionKey.SetRand(40 * 8);

            proto::RedirectTicket ticket;
            ticket.clientAddress = CLIENT_ADDRESS;
            ticket.session       = SESSION;
            ticket.generation    = 1;
            ticket.sessionKey    = sessionKey;
            ticket.accountName   = ACCOUNT;
            ticket.links         = links;

            links->ExpectSlotOne(1);
            registry.Open(ticket);

            proto::EndpointPolicy policy;
            policy.role = proto::ConnRole::Staging1;

            connection = std::make_shared<proto::ClientConnection>(gateway, registry, policy);
            connection->setPeerAddress(CLIENT_ADDRESS);
            connection->setSender([this](const uint8_t* data, size_t len)
            {
                sent.insert(sent.end(), data, data + len);
            });
            connection->setCloser([this]() { closed = true; });
        }

        /// Drive onConnect and hand back the 32 seed bytes of its challenge.
        ///
        /// The wire a server writes is NOT framed the way a client's is: its
        /// header is four bytes -- a big-endian size counting the two opcode
        /// bytes, then the opcode -- against the client's six. Feeding it to
        /// PacketCodec (which decodes what clients send) reads two payload bytes
        /// as part of the opcode field and hands back everything shifted by two,
        /// which is exactly how this fixture first "proved" a bug that was not
        /// there. Walk the server frames by hand.
        std::array<uint8, AuthCrypt::SeedLength> OpenAndReadChallengeSeed()
        {
            const std::vector<uint8_t> wire = connection->onConnect();

            std::array<uint8, AuthCrypt::SeedLength> seed{};

            size_t offset = 0;
            while (offset + 4 <= wire.size())
            {
                const uint32 size   = uint32(wire[offset]) << 8 | uint32(wire[offset + 1]);
                const uint16 opcode = uint16(wire[offset + 2]) |
                                      uint16(uint16(wire[offset + 3]) << 8);
                const uint8* payload = wire.data() + offset + 4;
                const size_t payloadLen = size - 2;

                if (opcode == WIRE_AUTH_CHALLENGE && payloadLen >= seed.size() + 4)
                {
                    std::memcpy(seed.data(), payload, seed.size());
                    // The 4 bytes after the cipher seed are the server's own
                    // nonce, which the client hashes into its stream proof.
                    std::memcpy(&serverSeed, payload + seed.size(), sizeof(serverSeed));
                }

                offset += 4 + payloadLen;
            }
            return seed;
        }

        void Feed(const std::vector<uint8>& wire)
        {
            connection->onData(wire.data(), wire.size());
        }
    };
}

TEST(RedirectStream_continued_session_arrives_in_clear_and_is_not_dropped)
{
    // The regression. The client sends CMSG_AUTH_CONTINUED_SESSION before it
    // keys anything, so this packet's header is plain text -- a connection that
    // armed on the banner reads it as garbage and closes.
    Fixture fixture;
    fixture.OpenAndReadChallengeSeed();

    fixture.Feed(ClientBanner());
    fixture.Feed(ContinuedSession(StreamProof(fixture.sessionKey, fixture.serverSeed)));

    CHECK(!fixture.closed);

    // And it is transport, not game: the world never sees it.
    CHECK_EQ(int(fixture.gateway.delivered.size()), 0);
}

TEST(RedirectStream_ciphers_are_keyed_from_the_challenge_seed)
{
    // What the client does with those 32 bytes (sub_1401A78D0, reached from
    // sub_1400AA560): they are the key pair for THIS stream, in place of the two
    // constants at 0x140992B00 that an ordinary connection uses. A server that
    // keys the redirected stream from the constants decodes nothing after the
    // handshake, however correct the rest of the redirect is.
    Fixture fixture;
    const std::array<uint8, AuthCrypt::SeedLength> seed =
        fixture.OpenAndReadChallengeSeed();

    fixture.Feed(ClientBanner());
    fixture.Feed(ContinuedSession(StreamProof(fixture.sessionKey, fixture.serverSeed)));
    REQUIRE(!fixture.closed);

    // The client's side of the same schedule. RC4 is an XOR stream and the two
    // directions are keyed from opposite halves, so running DecryptRecv here
    // produces exactly the ciphertext the client would have sent.
    BigNumber key = fixture.sessionKey;
    AuthCrypt clientSide;
    clientSide.Init(&key, seed.data());

    std::vector<uint8> ping = Frame(WIRE_PING, std::vector<uint8>(8, 0));
    clientSide.DecryptRecv(ping.data(), proto::CLIENT_HEADER_SIZE);

    fixture.Feed(ping);

    CHECK(!fixture.closed);
    REQUIRE(fixture.gateway.delivered.size() == 1);
    CHECK_EQ(int(fixture.gateway.delivered[0]), int(WIRE_PING));
}

TEST(RedirectStream_the_old_constant_seed_no_longer_decodes)
{
    // The negative control, and the exact shape of the live failure: a client
    // keyed from the challenge bytes talking to a server keyed from the shipped
    // constants. Every header after the handshake is noise, and the packet the
    // client meant to send never arrives.
    //
    // What is NOT asserted is that the connection drops. A header of noise is a
    // random 16-bit size, and roughly one in seven of those is a size the codec
    // finds plausible -- it then waits for a payload that never comes instead of
    // refusing the peer. Asserting the drop made this test fail about that
    // often; the property that actually matters, and that holds every time, is
    // that nothing keyed with the wrong table is ever accepted as a packet.
    Fixture fixture;
    fixture.OpenAndReadChallengeSeed();

    fixture.Feed(ClientBanner());
    fixture.Feed(ContinuedSession(StreamProof(fixture.sessionKey, fixture.serverSeed)));
    REQUIRE(!fixture.closed);

    BigNumber key = fixture.sessionKey;
    AuthCrypt constants;
    constants.Init(&key);                       // the stream-0 pair

    std::vector<uint8> ping = Frame(WIRE_PING, std::vector<uint8>(8, 0));
    constants.DecryptRecv(ping.data(), proto::CLIENT_HEADER_SIZE);

    fixture.Feed(ping);

    CHECK_EQ(int(fixture.gateway.delivered.size()), 0);
}

TEST(RedirectStream_answers_the_handshake_with_resume_comms)
{
    // The packet that makes the stream real. The client parks a redirected
    // socket in a staging slot, where sub_1400A9B60 refuses every ordinary
    // packet with disconnect reason 3 -- and, separately, queues everything its
    // own router would send on stream 1 (its pending flag is set at
    // construction). SMSG_RESUME_COMMS ends both: sub_1400AC1D0 promotes the
    // connection and flushes that queue.
    //
    // Live symptom of omitting it, 2026-08-30: the world loaded, and the Logout
    // button did nothing -- CMSG_LOGOUT_REQUEST is a stream-1 opcode and sat in
    // the client's queue. No stream-1 opcode ever reached the server at all.
    Fixture fixture;
    const std::array<uint8, AuthCrypt::SeedLength> seed =
        fixture.OpenAndReadChallengeSeed();

    fixture.Feed(ClientBanner());
    fixture.sent.clear();
    fixture.Feed(ContinuedSession(StreamProof(fixture.sessionKey, fixture.serverSeed)));

    REQUIRE(!fixture.closed);
    REQUIRE(fixture.sent.size() >= 4);

    // It is enciphered, like everything after the handshake, and keyed from the
    // challenge seed -- so the client reads it with the pair it just derived.
    BigNumber key = fixture.sessionKey;
    AuthCrypt clientSide;
    clientSide.Init(&key, seed.data());

    // EncryptSend, not DecryptRecv: the outgoing direction is keyed with the
    // first half of the seed and the incoming one with the second, so recovering
    // what the SERVER sent means running the server's own direction again -- RC4
    // is an XOR stream, so a second identically keyed pass is the inverse.
    std::vector<uint8> header(fixture.sent.begin(), fixture.sent.begin() + 4);
    clientSide.EncryptSend(header.data(), header.size());

    const uint32 size   = uint32(header[0]) << 8 | uint32(header[1]);
    const uint16 opcode = uint16(header[2]) | uint16(uint16(header[3]) << 8);

    CHECK_EQ(int(opcode), int(WIRE_RESUME_COMMS));
    CHECK_EQ(int(size), 2);                       // the opcode alone; no payload
    CHECK_EQ(int(fixture.sent.size()), 4);        // and it is the whole of it

    // A control opcode, which is what lets it reach a connection the client has
    // not promoted yet. Nothing else may be written there.
    CHECK(proto::IsTransportPlane(WIRE_RESUME_COMMS));
}

TEST(RedirectStream_a_superseded_socket_is_never_announced_to_the_client)
{
    // Review finding F1. SMSG_RESUME_COMMS is not a courtesy: it makes the
    // client adopt the socket, clear its stream-1 pending flag and flush
    // everything it had queued for that stream. Sending it to a socket whose
    // redirect has since been reissued means the client flushes that backlog
    // onto a connection the server closes an instant later, and those packets
    // are lost. The generation guard exists to stop exactly that, and it has to
    // be consulted BEFORE the client is told anything.
    Fixture fixture;
    fixture.OpenAndReadChallengeSeed();
    fixture.Feed(ClientBanner());

    // The session gave up on this redirect and issued another one.
    fixture.links->ExpectSlotOne(2);

    fixture.sent.clear();
    fixture.Feed(ContinuedSession(StreamProof(fixture.sessionKey, fixture.serverSeed)));

    // Refused, and silently: nothing was written to a socket that is not going
    // to be the stream.
    CHECK_EQ(int(fixture.sent.size()), 0);
    CHECK(fixture.closed);
    CHECK_EQ(int(fixture.gateway.delivered.size()), 0);
}

TEST(RedirectStream_the_staging_socket_takes_the_handshake_and_nothing_else)
{
    // Review finding F2. The client sends CMSG_AUTH_CONTINUED_SESSION and
    // nothing else on a socket it holds in staging -- its own router cannot
    // reach that socket until the promotion. A peer framing anything else is not
    // the client we redirected, and arming the cipher for it would leave a
    // genuine continued session arriving later to be decrypted as noise.
    Fixture fixture;
    fixture.OpenAndReadChallengeSeed();
    fixture.Feed(ClientBanner());
    fixture.sent.clear();

    fixture.Feed(Frame(WIRE_PING, std::vector<uint8>(8, 0)));

    CHECK(fixture.closed);
    CHECK_EQ(int(fixture.sent.size()), 0);          // no resume for a stranger
    CHECK_EQ(int(fixture.gateway.delivered.size()), 0);  // and the world is not told
}

TEST(RedirectStream_the_handshake_must_be_the_right_size)
{
    // The same guard on the length. Checked rather than parsed: proto reads none
    // of that payload, so all it can say is that the shape is wrong.
    Fixture fixture;
    fixture.OpenAndReadChallengeSeed();
    fixture.Feed(ClientBanner());
    fixture.sent.clear();

    fixture.Feed(Frame(WIRE_AUTH_CONTINUED_SESSION, std::vector<uint8>(12, 0)));

    CHECK(fixture.closed);
    CHECK_EQ(int(fixture.sent.size()), 0);
    CHECK_EQ(int(fixture.gateway.delivered.size()), 0);
}

TEST(RedirectStream_refuses_a_second_stream_that_cannot_prove_the_session_key)
{
    // A redirect ticket is claimed by address alone, so without this check the
    // second stream belongs to whoever reaches the port first from that address
    // -- another client behind the same router, or anything else on the machine.
    // The digest is the only thing on this socket that says who is on it, and it
    // is over a secret only the real client holds.
    Fixture fixture;
    fixture.OpenAndReadChallengeSeed();
    fixture.Feed(ClientBanner());
    fixture.sent.clear();

    std::array<uint8, 20> wrong = StreamProof(fixture.sessionKey, fixture.serverSeed);
    wrong[0] = uint8(wrong[0] ^ 0xFF);

    fixture.Feed(ContinuedSession(wrong));

    CHECK(fixture.closed);
    CHECK_EQ(int(fixture.sent.size()), 0);          // nothing announced to a stranger
    CHECK_EQ(int(fixture.gateway.delivered.size()), 0);
}

TEST(RedirectStream_the_proof_is_bound_to_this_connections_own_seed)
{
    // The seed is drawn per connection, so a digest lifted from another socket's
    // challenge -- a replay of a proof seen once -- does not open this one.
    Fixture fixture;
    fixture.OpenAndReadChallengeSeed();
    fixture.Feed(ClientBanner());
    fixture.sent.clear();

    const uint32 someoneElsesSeed = uint32(fixture.serverSeed + 1);
    fixture.Feed(ContinuedSession(StreamProof(fixture.sessionKey, someoneElsesSeed)));

    CHECK(fixture.closed);
    CHECK_EQ(int(fixture.sent.size()), 0);
}

TEST(RedirectStream_accepts_a_handshake_captured_from_the_live_client)
{
    // Not a round trip through our own helpers: these are the bytes a real
    // 4.3.4 client put on the wire on 2026-08-30, with the session key its
    // realmd handed it and the seed this server sent it. Nothing here is
    // computed by the code under test, so it fails if either the digest formula
    // or the interleaving drifts -- which the round-trip tests cannot catch,
    // since they build the packet with the same rules they check.
    const char* const KEY_HEX =
        "A243D2994CCBF95639739E4A1C9510A25CF6F86C826AD846B4038DBE11CF6A6E6C251B0A2E59751C";

    BigNumber sessionKey;
    sessionKey.SetHexStr(KEY_HEX);

    // The 4 seed bytes as they sat in that connection's SMSG_AUTH_CHALLENGE.
    const uint8 seedBytes[4] = { 0xDB, 0xB2, 0x8E, 0xCB };
    uint32 serverSeed = 0;
    std::memcpy(&serverSeed, seedBytes, sizeof(serverSeed));

    Sha1Hash sha;
    sha.UpdateData(std::string("ADMIN"));
    sha.UpdateBigNumbers(&sessionKey, NULL);
    sha.UpdateData(seedBytes, sizeof(seedBytes));
    sha.Finalize();

    // What the client actually sent, byte for byte.
    const uint8 onTheWire[20] =
    {
        0x13, 0x3E, 0xB9, 0x40, 0x92, 0xD1, 0xA9, 0xA6, 0xF2, 0x62,
        0x4E, 0x58, 0x25, 0xE4, 0x37, 0x59, 0x5A, 0x66, 0x23, 0xCA
    };

    // Unscrambling it with the order the server reads must give the digest.
    uint8 rebuilt[20] = {};
    for (size_t i = 0; i < 20; ++i)
    {
        rebuilt[DIGEST_WIRE_ORDER[i]] = onTheWire[i];
    }

    CHECK(std::memcmp(rebuilt, sha.GetDigest(), 20) == 0);
}

TEST(RedirectStream_challenge_seed_differs_per_connection)
{
    // Both streams of one session share a session key, so the seed is what keeps
    // their keystreams apart. Zero bytes here -- which is what the challenge
    // carried before this was understood -- would key both halves identically
    // and run one keystream in both directions.
    Fixture first;
    Fixture second;

    const std::array<uint8, AuthCrypt::SeedLength> a = first.OpenAndReadChallengeSeed();
    const std::array<uint8, AuthCrypt::SeedLength> b = second.OpenAndReadChallengeSeed();

    CHECK(a != b);

    const std::array<uint8, AuthCrypt::SeedLength> zero{};
    CHECK(a != zero);

    // The two halves are separate keys; equal halves would collapse the pair.
    CHECK(std::memcmp(a.data(), a.data() + AuthCrypt::SeedLength / 2,
                      AuthCrypt::SeedLength / 2) != 0);
}
