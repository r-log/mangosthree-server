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

#include "Crypto/Sha1.h"

namespace MaNGOS
{
    namespace Crypto
    {
        void Sha1Traits::Init(uint32_t* h)
        {
            h[0] = 0x67452301u; h[1] = 0xEFCDAB89u; h[2] = 0x98BADCFEu; h[3] = 0x10325476u; h[4] = 0xC3D2E1F0u;
        }

        void Sha1Traits::Compress(uint32_t* h, const uint8_t* block)
        {
            uint32_t w[80];
            for (int i = 0; i < 16; ++i)
            {
                w[i] = LoadBigEndian32(block + 4 * i);
            }
            for (int i = 16; i < 80; ++i)
            {
                w[i] = RotateLeft32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
            }
            uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
            for (int i = 0; i < 80; ++i)
            {
                uint32_t f, k;
                if (i < 20)      { f = (b & c) | (~b & d);           k = 0x5A827999u; }
                else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1u; }
                else if (i < 60) { f = (b & c) | (b & d) | (c & d);  k = 0x8F1BBCDCu; }
                else             { f = b ^ c ^ d;                    k = 0xCA62C1D6u; }
                const uint32_t t = RotateLeft32(a, 5) + f + e + k + w[i];
                e = d; d = c; c = RotateLeft32(b, 30); b = a; a = t;
            }
            h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
        }

        std::array<uint8_t, 20> Sha1(const uint8_t* data, size_t length)
        {
            Sha1Core core;
            core.Update(data, length);
            std::array<uint8_t, 20> out{};
            core.Finish(out.data());
            return out;
        }
    }
}
