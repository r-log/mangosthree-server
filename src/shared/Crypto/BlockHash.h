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

#ifndef MANGOS_H_CRYPTO_BLOCK_HASH
#define MANGOS_H_CRYPTO_BLOCK_HASH

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace MaNGOS
{
    namespace Crypto
    {
        /**
         * @brief The Merkle-Damgard skeleton SHA-1, SHA-256 and MD5 share.
         *
         * The three differ only in their compression function, their initial state,
         * the byte order of their words and the byte order of the length they append.
         * Everything else -- buffering partial blocks, the 0x80 pad, the 64-bit bit
         * count -- is here once. Streaming: Update takes any number of bytes any number
         * of times; nothing is copied beyond one 64-byte block.
         *
         * Traits: `static constexpr size_t StateWords, DigestLength; static constexpr
         * bool BigEndian; static void Init(uint32_t* state); static void Compress(uint32_t*
         * state, const uint8_t* block);`
         */
        template <class Traits>
        class BlockHash
        {
        public:
            static constexpr size_t BlockLength  = 64;
            static constexpr size_t DigestLength = Traits::DigestLength;

            BlockHash() { Reset(); }

            void Reset()
            {
                Traits::Init(m_state);
                m_buffered = 0;
                m_total = 0;
            }

            void Update(const uint8_t* data, size_t length)
            {
                m_total += length;
                if (m_buffered)
                {
                    const size_t take = (length < BlockLength - m_buffered) ? length : BlockLength - m_buffered;
                    std::memcpy(m_buffer + m_buffered, data, take);
                    m_buffered += take;
                    data += take;
                    length -= take;
                    if (m_buffered < BlockLength)
                    {
                        return;
                    }
                    Traits::Compress(m_state, m_buffer);
                    m_buffered = 0;
                }
                while (length >= BlockLength)
                {
                    Traits::Compress(m_state, data);
                    data += BlockLength;
                    length -= BlockLength;
                }
                if (length)
                {
                    std::memcpy(m_buffer, data, length);
                    m_buffered = length;
                }
            }

            /// Pads, appends the bit count, and writes the digest. The object is then
            /// reset and can be reused.
            void Finish(uint8_t* out)
            {
                const uint64_t bits = m_total * 8;
                uint8_t pad = 0x80;
                Update(&pad, 1);
                const uint8_t zero = 0;
                while (m_buffered != 56)
                {
                    Update(&zero, 1);
                }
                uint8_t length[8];
                for (int i = 0; i < 8; ++i)
                {
                    length[i] = Traits::BigEndian ? uint8_t(bits >> (56 - 8 * i)) : uint8_t(bits >> (8 * i));
                }
                Update(length, 8);   // completes the block: Compress runs, m_buffered is 0
                for (size_t w = 0; w < Traits::StateWords; ++w)
                {
                    const uint32_t v = m_state[w];
                    if (Traits::BigEndian)
                    {
                        out[4 * w] = uint8_t(v >> 24); out[4 * w + 1] = uint8_t(v >> 16);
                        out[4 * w + 2] = uint8_t(v >> 8); out[4 * w + 3] = uint8_t(v);
                    }
                    else
                    {
                        out[4 * w] = uint8_t(v); out[4 * w + 1] = uint8_t(v >> 8);
                        out[4 * w + 2] = uint8_t(v >> 16); out[4 * w + 3] = uint8_t(v >> 24);
                    }
                }
                Reset();
            }

        private:
            uint32_t m_state[Traits::StateWords];
            uint8_t  m_buffer[BlockLength];
            size_t   m_buffered;
            uint64_t m_total;
        };

        inline uint32_t RotateLeft32(uint32_t v, unsigned n)  { return (v << n) | (v >> (32u - n)); }
        inline uint32_t RotateRight32(uint32_t v, unsigned n) { return (v >> n) | (v << (32u - n)); }

        inline uint32_t LoadBigEndian32(const uint8_t* p)
        {
            return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
        }

        inline uint32_t LoadLittleEndian32(const uint8_t* p)
        {
            return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
        }
    }
}

#endif
