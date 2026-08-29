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

#ifndef MANGOS_H_CRYPTO_MD5
#define MANGOS_H_CRYPTO_MD5

#include "Crypto/BlockHash.h"

#include <array>

namespace MaNGOS
{
    namespace Crypto
    {
        /// MD5 (RFC 1321), streaming. 16-byte digest. Used for patch-file identity only.
        struct Md5Traits
        {
            static constexpr size_t StateWords   = 4;
            static constexpr size_t DigestLength = 16;
            static constexpr bool   BigEndian    = false;
            static void Init(uint32_t* state);
            static void Compress(uint32_t* state, const uint8_t* block);
        };

        using Md5Core = BlockHash<Md5Traits>;

        /// One-shot convenience.
        std::array<uint8_t, 16> Md5(const uint8_t* data, size_t length);
    }
}

#endif
