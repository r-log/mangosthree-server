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

#include "Crypto/HmacSha1.h"
#include "Crypto/SecureZero.h"

#include <cstring>

namespace MaNGOS
{
    namespace Crypto
    {
        HmacSha1::HmacSha1(const uint8_t* key, size_t length)
        {
            uint8_t block[Sha1Core::BlockLength];
            std::memset(block, 0, sizeof block);
            if (length > Sha1Core::BlockLength)
            {
                Sha1Core hash;
                hash.Update(key, length);
                hash.Finish(block);
            }
            else if (length)
            {
                std::memcpy(block, key, length);
            }
            for (size_t i = 0; i < Sha1Core::BlockLength; ++i)
            {
                m_inner[i] = uint8_t(block[i] ^ 0x36);
                m_outer[i] = uint8_t(block[i] ^ 0x5C);
            }
            SecureZero(block, sizeof block);
            Start();
        }

        HmacSha1::~HmacSha1()
        {
            SecureZero(m_inner, sizeof m_inner);
            SecureZero(m_outer, sizeof m_outer);
        }

        void HmacSha1::Start()
        {
            m_core.Reset();
            m_core.Update(m_inner, sizeof m_inner);
        }

        void HmacSha1::Update(const uint8_t* data, size_t length)
        {
            m_core.Update(data, length);
        }

        void HmacSha1::Finish(uint8_t* out)
        {
            uint8_t inner[Sha1Core::DigestLength];
            m_core.Finish(inner);
            m_core.Update(m_outer, sizeof m_outer);
            m_core.Update(inner, sizeof inner);
            m_core.Finish(out);
            Start();
        }
    }
}
