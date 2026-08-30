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

#pragma once

// The client's half of 4.3.4 framing.
//
// The two directions do NOT share a header shape, and that asymmetry is the
// single easiest thing to get wrong here:
//
//   client -> server   6 bytes: uint16 size (big-endian, counting the 4 opcode
//                      bytes) then uint32 opcode (little-endian).
//                      Decoded by proto::PacketCodec.
//
//   server -> client   4 bytes: uint16 size (big-endian, counting the 2 opcode
//                      bytes) then uint16 opcode -- or 5 bytes when the size
//                      exceeds 0x7FFF, the extra leading byte carrying the top
//                      bits with 0x80 set.
//                      Written by proto::PacketCodec::Encode.
//
// Feeding a server frame to PacketCodec reads two payload bytes as part of the
// opcode field and hands back everything shifted by two -- which is exactly how
// RedirectStreamTest once "proved" a bug that was not there. So this file exists
// rather than reusing the codec: nothing in the tree decodes the server's
// framing, because nothing in the tree is a client.
//
// The cipher direction is the mirror image of the same trap. AuthCrypt names its
// two RC4 states from the SERVER's point of view, so a client uses them the
// other way round:
//
//   to send   -> AuthCrypt::DecryptRecv over our header, because that applies
//                the very keystream the server will apply to decrypt it.
//   to read   -> AuthCrypt::EncryptSend over the server's header, because RC4 is
//                an XOR stream and a second identically-keyed pass is its own
//                inverse.

#include "Platform/Define.h"
#include "WorldPacket.h"

#include <functional>
#include <string>
#include <vector>

namespace loadtest
{
    /// Applies a stream cipher to `len` header bytes in place.
    typedef std::function<void(uint8* header, size_t len)> HeaderCipher;

    /**
     * @brief Serialise one packet the way the 4.3.4 client does.
     *
     * @param cipher run over the six header bytes; empty before the crypt is
     *               armed, which is how CMSG_AUTH_SESSION and
     *               CMSG_AUTH_CONTINUED_SESSION go out in clear.
     */
    std::vector<uint8> EncodeClientPacket(const WorldPacket& packet,
                                          const HeaderCipher& cipher);

    /**
     * @brief The client's opening greeting, which is not a WorldPacket at all.
     *
     * A uint16 size and then the raw sentence -- no opcode field. It only parses
     * as MSG_WOW_CONNECTION because the first two letters of "WORLD" *are* the
     * opcode 0x4F57 once the six-byte header is read across them. Reproduce it
     * byte for byte; building it as a packet produces something the server's
     * codec reads two bytes out of step.
     */
    std::vector<uint8> ClientBanner();

    /**
     * @brief Reassembles the server's stream into packets.
     *
     * Decrypts each header byte as it arrives rather than waiting for the whole
     * header, because the header's own LENGTH depends on its first byte: RC4 is a
     * byte stream, so incremental decryption is identical to one pass, and it is
     * the only order that can decide how many more bytes to wait for.
     */
    class ServerFrameReader
    {
        public:

            ServerFrameReader();

            /// Installed at the moment the client arms its ciphers, mid-stream.
            void SetCipher(const HeaderCipher& cipher) { m_cipher = cipher; }

            /**
             * @param out receives every packet completed by these bytes
             * @return false when a header cannot be a header, which for a stream
             *         cipher means the keystream has desynchronised.
             */
            bool Feed(const uint8* data, size_t len,
                      std::vector<WorldPacket>& out, std::string& error);

        private:

            HeaderCipher m_cipher;

            uint8  m_header[5];
            size_t m_headerFill;
            size_t m_headerLen;      ///< 0 until the first byte says 4 or 5

            uint16 m_opcode;
            size_t m_payloadNeeded;
            std::vector<uint8> m_payload;
    };
}
