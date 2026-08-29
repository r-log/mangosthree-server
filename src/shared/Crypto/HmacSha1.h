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

#ifndef MANGOS_H_CRYPTO_HMAC_SHA1
#define MANGOS_H_CRYPTO_HMAC_SHA1

#include "Crypto/Sha1.h"

#include <cstddef>
#include <cstdint>

namespace MaNGOS
{
    namespace Crypto
    {
        /// HMAC-SHA1 (RFC 2104), streaming. Keys longer than the 64-byte block are
        /// hashed first, as the RFC says. The key schedule is erased on destruction.
        class HmacSha1
        {
        public:
            static constexpr size_t DigestLength = Sha1Core::DigestLength;

            HmacSha1(const uint8_t* key, size_t length);
            ~HmacSha1();

            HmacSha1(const HmacSha1&) = delete;
            HmacSha1& operator=(const HmacSha1&) = delete;

            void Update(const uint8_t* data, size_t length);

            /// Writes the 20-byte tag; the object is then ready for another message
            /// under the same key.
            void Finish(uint8_t* out);

        private:
            void Start();

            uint8_t  m_inner[Sha1Core::BlockLength];
            uint8_t  m_outer[Sha1Core::BlockLength];
            Sha1Core m_core;
        };
    }
}

#endif
