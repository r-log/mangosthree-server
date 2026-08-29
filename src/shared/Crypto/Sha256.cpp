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

#include "Crypto/Sha256.h"

namespace MaNGOS
{
    namespace Crypto
    {
        namespace
        {
            constexpr uint32_t K[64] =
            {
                0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u, 0x3956C25Bu, 0x59F111F1u, 0x923F82A4u, 0xAB1C5ED5u,
                0xD807AA98u, 0x12835B01u, 0x243185BEu, 0x550C7DC3u, 0x72BE5D74u, 0x80DEB1FEu, 0x9BDC06A7u, 0xC19BF174u,
                0xE49B69C1u, 0xEFBE4786u, 0x0FC19DC6u, 0x240CA1CCu, 0x2DE92C6Fu, 0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu,
                0x983E5152u, 0xA831C66Du, 0xB00327C8u, 0xBF597FC7u, 0xC6E00BF3u, 0xD5A79147u, 0x06CA6351u, 0x14292967u,
                0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu, 0x53380D13u, 0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u,
                0xA2BFE8A1u, 0xA81A664Bu, 0xC24B8B70u, 0xC76C51A3u, 0xD192E819u, 0xD6990624u, 0xF40E3585u, 0x106AA070u,
                0x19A4C116u, 0x1E376C08u, 0x2748774Cu, 0x34B0BCB5u, 0x391C0CB3u, 0x4ED8AA4Au, 0x5B9CCA4Fu, 0x682E6FF3u,
                0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u, 0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u, 0xC67178F2u
            };
        }

        void Sha256Traits::Init(uint32_t* h)
        {
            h[0] = 0x6A09E667u; h[1] = 0xBB67AE85u; h[2] = 0x3C6EF372u; h[3] = 0xA54FF53Au;
            h[4] = 0x510E527Fu; h[5] = 0x9B05688Cu; h[6] = 0x1F83D9ABu; h[7] = 0x5BE0CD19u;
        }

        void Sha256Traits::Compress(uint32_t* h, const uint8_t* block)
        {
            uint32_t w[64];
            for (int i = 0; i < 16; ++i)
            {
                w[i] = LoadBigEndian32(block + 4 * i);
            }
            for (int i = 16; i < 64; ++i)
            {
                const uint32_t s0 = RotateRight32(w[i - 15], 7) ^ RotateRight32(w[i - 15], 18) ^ (w[i - 15] >> 3);
                const uint32_t s1 = RotateRight32(w[i - 2], 17) ^ RotateRight32(w[i - 2], 19) ^ (w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }
            uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
            for (int i = 0; i < 64; ++i)
            {
                const uint32_t s1  = RotateRight32(e, 6) ^ RotateRight32(e, 11) ^ RotateRight32(e, 25);
                const uint32_t ch  = (e & f) ^ (~e & g);
                const uint32_t t1  = hh + s1 + ch + K[i] + w[i];
                const uint32_t s0  = RotateRight32(a, 2) ^ RotateRight32(a, 13) ^ RotateRight32(a, 22);
                const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                const uint32_t t2  = s0 + maj;
                hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
            }
            h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
        }

        std::array<uint8_t, 32> Sha256(const uint8_t* data, size_t length)
        {
            Sha256Core core;
            core.Update(data, length);
            std::array<uint8_t, 32> out{};
            core.Finish(out.data());
            return out;
        }
    }
}
