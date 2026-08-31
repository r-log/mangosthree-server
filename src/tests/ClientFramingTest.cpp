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

// The load-test client's framing, checked against the server's own codec rather
// than against itself.
//
// The two directions of this protocol do NOT share a header shape -- the client
// writes six bytes and the server writes four, or five for a large packet -- and
// that asymmetry has already cost this project real time twice: once as a live
// login failure, and once as a test fixture that "proved" a bug that was not
// there by reading server frames with the client's decoder.
//
// So neither side of these tests is allowed to be the harness's own opinion.
// What loadtest encodes is decoded by proto::PacketCodec, which is what the
// server actually runs; what proto::PacketCodec::Encode produces is decoded by
// loadtest. A drift in either direction fails here instead of at the far end of
// a handshake.

#include "TestHarness.h"

#include "Framing.hpp"
#include "PacketCodec.h"
#include "Auth/AuthCrypt.h"
#include "Auth/BigNumber.h"

#include <cstring>
#include <string>
#include <vector>

namespace
{
    const uint16 SOME_OPCODE = 0x0449;   // CMSG_AUTH_SESSION

    /// Bytes with no structure, so a wrong offset cannot accidentally agree.
    std::vector<uint8> Filler(size_t count)
    {
        std::vector<uint8> out;
        out.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            out.push_back(uint8((i * 97u) ^ (i >> 5)));
        }
        return out;
    }
}

TEST(ClientFraming_the_server_codec_decodes_what_the_harness_writes)
{
    // The direction that matters most: if this is wrong, no synthetic client can
    // log in, and the failure arrives as "malformed packet framing" with no clue
    // which end is at fault.
    const std::vector<uint8> payload = Filler(64);

    WorldPacket packet(SOME_OPCODE, payload.size());
    packet.append(payload.data(), payload.size());

    const std::vector<uint8> wire =
        loadtest::EncodeClientPacket(packet, loadtest::HeaderCipher());

    proto::PacketCodec codec;
    std::vector<WorldPacket> decoded;
    CHECK(codec.Feed(wire.data(), wire.size(), decoded) == proto::DecodeStatus::Ok);

    REQUIRE(decoded.size() == 1);
    CHECK_EQ(int(decoded[0].GetOpcode()), int(SOME_OPCODE));
    REQUIRE(decoded[0].size() == payload.size());
    CHECK(std::memcmp(decoded[0].contents(), payload.data(), payload.size()) == 0);
}

TEST(ClientFraming_the_harness_decodes_what_the_server_writes)
{
    // The other direction, and the one the old fixture got wrong: a server frame
    // has a four-byte header, not six. Reading it with the client's decoder takes
    // two payload bytes for part of the opcode and shifts everything after it.
    const std::vector<uint8> payload = Filler(37);

    WorldPacket packet(SOME_OPCODE, payload.size());
    packet.append(payload.data(), payload.size());

    const std::vector<uint8> wire =
        proto::PacketCodec::Encode(packet, proto::PacketCodec::HeaderEncryptor());

    loadtest::ServerFrameReader reader;
    std::vector<WorldPacket> decoded;
    std::string error;
    REQUIRE(reader.Feed(wire.data(), wire.size(), decoded, error));

    REQUIRE(decoded.size() == 1);
    CHECK_EQ(int(decoded[0].GetOpcode()), int(SOME_OPCODE));
    REQUIRE(decoded[0].size() == payload.size());
    CHECK(std::memcmp(decoded[0].contents(), payload.data(), payload.size()) == 0);
}

TEST(ClientFraming_reads_the_three_byte_size_a_large_packet_carries)
{
    // Over 0x7FFF the server prefixes a third size byte with 0x80 set, so the
    // header becomes five bytes. A reader that always takes four splits the
    // packet at the wrong place -- and once a stream cipher is armed, takes the
    // wrong stretch of keystream and never recovers.
    const std::vector<uint8> payload = Filler(0x9000);

    WorldPacket packet(SOME_OPCODE, payload.size());
    packet.append(payload.data(), payload.size());

    const std::vector<uint8> wire =
        proto::PacketCodec::Encode(packet, proto::PacketCodec::HeaderEncryptor());

    // Five header bytes, not four: the shape this test exists for.
    CHECK_EQ(int(wire.size() - payload.size()), 5);

    loadtest::ServerFrameReader reader;
    std::vector<WorldPacket> decoded;
    std::string error;
    REQUIRE(reader.Feed(wire.data(), wire.size(), decoded, error));

    REQUIRE(decoded.size() == 1);
    CHECK_EQ(int(decoded[0].GetOpcode()), int(SOME_OPCODE));
    REQUIRE(decoded[0].size() == payload.size());
    CHECK(std::memcmp(decoded[0].contents(), payload.data(), payload.size()) == 0);
}

TEST(ClientFraming_reassembles_packets_split_across_reads)
{
    // TCP delivers what it likes. Feeding the same stream one byte at a time must
    // produce the same packets as feeding it whole -- including across the header,
    // which is where the reader decides how many more bytes it needs.
    WorldPacket first(SOME_OPCODE, 0);
    WorldPacket second(0x0140, 4);                    // SMSG_RESUME_COMMS-sized
    second << uint32(0xDEADBEEF);

    std::vector<uint8> wire =
        proto::PacketCodec::Encode(first, proto::PacketCodec::HeaderEncryptor());
    const std::vector<uint8> tail =
        proto::PacketCodec::Encode(second, proto::PacketCodec::HeaderEncryptor());
    wire.insert(wire.end(), tail.begin(), tail.end());

    loadtest::ServerFrameReader reader;
    std::vector<WorldPacket> decoded;
    std::string error;

    for (size_t i = 0; i < wire.size(); ++i)
    {
        REQUIRE(reader.Feed(&wire[i], 1, decoded, error));
    }

    REQUIRE(decoded.size() == 2);
    CHECK_EQ(int(decoded[0].GetOpcode()), int(SOME_OPCODE));
    CHECK_EQ(int(decoded[0].size()), 0);
    CHECK_EQ(int(decoded[1].GetOpcode()), 0x0140);
    CHECK_EQ(int(decoded[1].size()), 4);
}

TEST(ClientFraming_survives_a_full_round_trip_through_the_header_cipher)
{
    // The cipher direction is the same trap wearing a different hat. AuthCrypt is
    // named from the server's side, so a client enciphers what it SENDS with
    // DecryptRecv and reads what it RECEIVES with EncryptSend. Swapping them keys
    // each direction off the other's half of the seed, and every header after the
    // handshake is noise -- which is indistinguishable from a desynchronised
    // stream and was the shape of the live 2026-08-30 failure.
    BigNumber sessionKey;
    sessionKey.SetRand(40 * 8);

    uint8 seed[AuthCrypt::SeedLength];
    for (size_t i = 0; i < sizeof(seed); ++i)
    {
        seed[i] = uint8(i * 7 + 1);
    }

    BigNumber serverKey = sessionKey;
    BigNumber clientKey = sessionKey;

    AuthCrypt server;
    AuthCrypt client;
    server.Init(&serverKey, seed);
    client.Init(&clientKey, seed);

    // --- client -> server -------------------------------------------------
    WorldPacket outgoing(SOME_OPCODE, 8);
    outgoing << uint64(0x0123456789ABCDEFull);

    const std::vector<uint8> sent = loadtest::EncodeClientPacket(outgoing,
        [&client](uint8* header, size_t len) { client.DecryptRecv(header, len); });

    proto::PacketCodec codec(
        [&server](uint8* header, size_t len) { server.DecryptRecv(header, len); });

    std::vector<WorldPacket> received;
    CHECK(codec.Feed(sent.data(), sent.size(), received) == proto::DecodeStatus::Ok);
    REQUIRE(received.size() == 1);
    CHECK_EQ(int(received[0].GetOpcode()), int(SOME_OPCODE));

    // --- server -> client -------------------------------------------------
    WorldPacket reply(0x4D42, 4);                     // SMSG_PONG
    reply << uint32(7);

    const std::vector<uint8> answered = proto::PacketCodec::Encode(reply,
        [&server](uint8* header, size_t len) { server.EncryptSend(header, len); });

    loadtest::ServerFrameReader reader;
    reader.SetCipher([&client](uint8* header, size_t len)
    {
        client.EncryptSend(header, len);
    });

    std::vector<WorldPacket> back;
    std::string error;
    REQUIRE(reader.Feed(answered.data(), answered.size(), back, error));

    REQUIRE(back.size() == 1);
    CHECK_EQ(int(back[0].GetOpcode()), 0x4D42);
    CHECK_EQ(int(back[0].size()), 4);
}

TEST(ClientFraming_the_banner_is_what_the_servers_codec_reads_as_a_connection)
{
    // The greeting is not a packet: a uint16 size and then the raw sentence, with
    // no opcode field at all. It only parses because the first two letters of
    // "WORLD" are themselves the opcode 0x4F57 once the six-byte header is read
    // across them. Building it as an ordinary packet yields something the server
    // reads two bytes out of step, and the client never gets past "Connecting".
    const std::vector<uint8> banner = loadtest::ClientBanner();

    proto::PacketCodec codec;
    std::vector<WorldPacket> decoded;
    CHECK(codec.Feed(banner.data(), banner.size(), decoded) == proto::DecodeStatus::Ok);

    REQUIRE(decoded.size() == 1);
    CHECK_EQ(int(decoded[0].GetOpcode()), 0x4F57);      // MSG_WOW_CONNECTION

    // And the whole greeting was consumed: nothing left dangling for the next
    // header to be read out of.
    CHECK_EQ(int(decoded[0].size() + 6), int(banner.size()));
}

TEST(ClientFraming_refuses_a_server_frame_that_cannot_be_one)
{
    // With the cipher armed, a header of noise is the only symptom a
    // desynchronised keystream produces. Reporting it as a decode failure is what
    // turns "the client hung" into a line naming the bytes.
    const uint8 impossible[4] = { 0x00, 0x00, 0x11, 0x22 };   // size 0, under the
                                                              // two opcode bytes
    loadtest::ServerFrameReader reader;
    std::vector<WorldPacket> decoded;
    std::string error;

    CHECK(!reader.Feed(impossible, sizeof(impossible), decoded, error));
    CHECK(decoded.empty());
    CHECK(!error.empty());
}
