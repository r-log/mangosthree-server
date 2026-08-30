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

#ifndef _AUTH_MD5_H
#define _AUTH_MD5_H

#include "Platform/Define.h"
#include "Crypto/Md5.h"

#include <cstddef>
#include <string>

/**
 * @brief MD5 with the interface the patch handling in realmd was written against, over
 * the tree's own core (src/shared/Crypto).
 *
 * The protocol identifies a patch file by its MD5, so the digest is a name here, not
 * a security property.
 */
class Md5Hash
{
    public:

        Md5Hash() = default;
        ~Md5Hash() = default;

        Md5Hash(const Md5Hash&) = delete;
        Md5Hash& operator=(const Md5Hash&) = delete;

        /// Reset, ready to hash a fresh message.
        void Initialize()
        {
            m_ctx.Reset();
        }

        void UpdateData(const uint8* data, size_t length)
        {
            m_ctx.Update(data, length);
        }

        void UpdateData(const std::string& str)
        {
            UpdateData(reinterpret_cast<const uint8*>(str.c_str()), str.size());
        }

        /// Produce the digest. Call once, after the last UpdateData().
        void Finalize()
        {
            m_ctx.Finish(m_digest);
        }

        /// The digest. Only meaningful after Finalize(); DigestLength bytes.
        uint8* GetDigest() { return m_digest; }

        static constexpr int DigestLength = int(MaNGOS::Crypto::Md5Core::DigestLength);

    private:

        MaNGOS::Crypto::Md5Core m_ctx;
        uint8                   m_digest[DigestLength]{};
};

#endif
