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

#include "Framing.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace loadtest
{
    namespace
    {
        /// The server never frames a payload larger than this, but the three-byte
        /// size form exists for SMSG_UPDATE_OBJECT and its relatives, so the
        /// reader has to honour it.
        const uint32 MAX_SERVER_PACKET = 0x7FFFFF;
    }

    std::vector<uint8> EncodeClientPacket(const WorldPacket& packet,
                                          const HeaderCipher& cipher)
    {
        const uint32 size = uint32(packet.size()) + 4;

        uint8 header[6];
        header[0] = uint8((size >> 8) & 0xFF);
        header[1] = uint8(size & 0xFF);
        header[2] = uint8(packet.GetOpcode() & 0xFF);
        header[3] = uint8((packet.GetOpcode() >> 8) & 0xFF);
        header[4] = 0;
        header[5] = 0;

        if (cipher)
        {
            cipher(header, sizeof(header));
        }

        std::vector<uint8> wire;
        wire.reserve(sizeof(header) + packet.size());
        wire.insert(wire.end(), header, header + sizeof(header));
        if (!packet.empty())
        {
            wire.insert(wire.end(), packet.contents(),
                        packet.contents() + packet.size());
        }
        return wire;
    }

    std::vector<uint8> ClientBanner()
    {
        const char greeting[] = "WORLD OF WARCRAFT CONNECTION - CLIENT TO SERVER";

        std::vector<uint8> wire;
        wire.push_back(0x00);
        wire.push_back(0x30);            // 48 == the 50 bytes on the wire, less 2
        wire.insert(wire.end(), greeting, greeting + sizeof(greeting));
        return wire;
    }

    ServerFrameReader::ServerFrameReader()
        : m_headerFill(0),
          m_headerLen(0),
          m_opcode(0),
          m_payloadNeeded(0)
    {
        std::memset(m_header, 0, sizeof(m_header));
    }

    bool ServerFrameReader::Feed(const uint8* data, size_t len,
                                 std::vector<WorldPacket>& out, std::string& error)
    {
        size_t offset = 0;

        while (offset < len)
        {
            // ---- The header, one byte at a time until its length is known -----
            if (m_headerLen == 0 || m_headerFill < m_headerLen)
            {
                uint8 byte = data[offset++];
                if (m_cipher)
                {
                    m_cipher(&byte, 1);
                }
                m_header[m_headerFill++] = byte;

                if (m_headerFill == 1)
                {
                    // The top bit of the first byte is the only thing that says
                    // whether two more size bytes follow. Decided here and not
                    // after a fixed-size read, because a fixed-size read of the
                    // wrong size takes the wrong stretch of keystream and every
                    // header after it is noise.
                    m_headerLen = (m_header[0] & 0x80) != 0 ? 5 : 4;
                    continue;
                }

                if (m_headerFill < m_headerLen)
                {
                    continue;
                }

                const uint32 size = m_headerLen == 5
                    ? ((uint32(m_header[0] & 0x7F) << 16) |
                       (uint32(m_header[1]) << 8) | uint32(m_header[2]))
                    : ((uint32(m_header[0]) << 8) | uint32(m_header[1]));

                m_opcode = uint16(m_header[m_headerLen - 2]) |
                           uint16(uint16(m_header[m_headerLen - 1]) << 8);

                // `size` counts the two opcode bytes, so anything under that is
                // not a frame at all.
                if (size < 2 || size > MAX_SERVER_PACKET)
                {
                    char detail[160];
                    std::snprintf(detail, sizeof(detail),
                                  "server frame is not a frame: size=%u opcode=0x%.4X "
                                  "(header %.2X %.2X %.2X %.2X %.2X, cipher %s)",
                                  size, m_opcode,
                                  m_header[0], m_header[1], m_header[2],
                                  m_header[3], m_header[4],
                                  m_cipher ? "armed" : "clear");
                    error = detail;
                    return false;
                }

                m_payloadNeeded = size - 2;
                m_payload.clear();
                m_payload.reserve(m_payloadNeeded);
            }

            // ---- The payload, in clear -----------------------------------------
            if (m_payloadNeeded > 0)
            {
                const size_t take = std::min(m_payloadNeeded, len - offset);
                if (take == 0)
                {
                    return true;                          // need more bytes
                }

                m_payload.insert(m_payload.end(), data + offset, data + offset + take);
                offset          += take;
                m_payloadNeeded -= take;

                if (m_payloadNeeded > 0)
                {
                    return true;
                }
            }

            WorldPacket packet(m_opcode, m_payload.size());
            if (!m_payload.empty())
            {
                packet.append(m_payload.data(), m_payload.size());
            }
            out.push_back(std::move(packet));

            m_headerFill = 0;
            m_headerLen  = 0;
            m_payload.clear();
        }

        return true;
    }
}
