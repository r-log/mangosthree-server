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

#include "Auth/HMACSHA1.h"
#include "Auth/BigNumber.h"
#include "Crypto/SecureZero.h"

#include <string>

HMACSHA1::HMACSHA1(uint32 len, uint8 *seed) : m_mac(seed, seed ? size_t(len) : 0), m_digest()
{
}

HMACSHA1::~HMACSHA1()
{
    MaNGOS::Crypto::SecureZero(m_digest, sizeof m_digest);
}

void HMACSHA1::UpdateBigNumber(BigNumber *bn)
{
    UpdateData(bn->AsByteArray(), bn->GetNumBytes());
}

void HMACSHA1::UpdateData(const uint8 *data, int length)
{
    if (length > 0)
    {
        m_mac.Update(data, size_t(length));
    }
}

void HMACSHA1::UpdateData(const std::string &str)
{
    UpdateData((uint8 const*)str.c_str(), int(str.length()));
}

void HMACSHA1::Finalize()
{
    m_mac.Finish(m_digest);
}

uint8 *HMACSHA1::ComputeHash(BigNumber *bn)
{
    UpdateBigNumber(bn);
    Finalize();
    return m_digest;
}
