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

#include "Auth/BigNumber.h"
#include "Crypto/ModExp.h"
#include "Crypto/SecureZero.h"
#include "Crypto/SystemRandom.h"
#include "Log/Log.h"
#include "Utilities/Errors.h"

#include <algorithm>
#include <string>
#include <vector>

using MaNGOS::Crypto::BigInt;

namespace
{
    void Erase(std::vector<uint8>& bytes)
    {
        if (!bytes.empty())
        {
            MaNGOS::Crypto::SecureZero(bytes.data(), bytes.size());
        }
        bytes.clear();
    }

    void Erase(std::string& text)
    {
        if (!text.empty())
        {
            MaNGOS::Crypto::SecureZero(&text[0], text.size());
        }
        text.clear();
    }
}

BigNumber::BigNumber()
{
}

BigNumber::BigNumber(const BigNumber& bn) : m_value(bn.m_value)
{
}

BigNumber::BigNumber(uint32 val) : m_value(val)
{
}

BigNumber::~BigNumber()
{
    m_value.SecureClear();
    Erase(m_array);
    Erase(m_text);
}

void BigNumber::SetDword(uint32 val)
{
    m_value = BigInt(val);
}

void BigNumber::SetQword(uint64 val)
{
    m_value = BigInt(val);
}

void BigNumber::SetBinary(const uint8* bytes, int len)
{
    // The argument comes from callers that read lengths off the wire: nothing is
    // assumed about it beyond what the bytes say.
    if (len <= 0 || !bytes)
    {
        m_value = BigInt();
        return;
    }
    m_value = BigInt::FromBytesLE(bytes, size_t(len));
}

void BigNumber::SetHexStr(const char* str)
{
    if (!str || !m_value.FromHex(str))
    {
        m_value = BigInt();
        if (str && *str)
        {
            sLog.outError("BigNumber: '%s' is not a hexadecimal number; using zero", str);
        }
    }
}

void BigNumber::SetRand(int numbits)
{
    MANGOS_ASSERT(numbits >= 2);
    m_value = MaNGOS::Crypto::SystemRandom::Instance().Bits(size_t(numbits));
}

BigNumber BigNumber::operator=(const BigNumber& bn)
{
    if (this != &bn)
    {
        m_value = bn.m_value;
    }
    return *this;
}

BigNumber BigNumber::operator+=(const BigNumber& bn)
{
    m_value += bn.m_value;
    return *this;
}

BigNumber BigNumber::operator-=(const BigNumber& bn)
{
    MANGOS_ASSERT(!(m_value < bn.m_value) && "BigNumber is unsigned: the result would be negative");
    m_value -= bn.m_value;
    return *this;
}

BigNumber BigNumber::operator*=(const BigNumber& bn)
{
    m_value *= bn.m_value;
    return *this;
}

BigNumber BigNumber::operator/=(const BigNumber& bn)
{
    MANGOS_ASSERT(!bn.m_value.IsZero() && "BigNumber: division by zero");
    m_value /= bn.m_value;
    return *this;
}

BigNumber BigNumber::operator%=(const BigNumber& bn)
{
    MANGOS_ASSERT(!bn.m_value.IsZero() && "BigNumber: division by zero");
    m_value %= bn.m_value;
    return *this;
}

BigNumber BigNumber::Exp(const BigNumber& bn)
{
    BigNumber ret(1);
    const size_t bits = bn.m_value.BitLength();
    for (size_t i = bits; i-- > 0;)
    {
        ret.m_value *= ret.m_value;
        if (bn.m_value.Bit(i))
        {
            ret.m_value *= m_value;
        }
    }
    return ret;
}

BigNumber BigNumber::ModExp(const BigNumber& bn1, const BigNumber& bn2)
{
    BigNumber ret;
    if (bn2.m_value.IsZero())
    {
        return ret;
    }
    // Odd moduli of two limbs or more take the Montgomery kernel; the rest (never a
    // protocol modulus) the plain path, so the class stays total.
    ret.m_value = MaNGOS::Crypto::ModExp(m_value, bn1.m_value, bn2.m_value, MaNGOS::Crypto::ExponentKind::Secret);
    return ret;
}

int BigNumber::GetNumBytes(void)
{
    return int(m_value.ByteLength());
}

uint32 BigNumber::AsDword()
{
    return uint32(m_value.Low64());
}

bool BigNumber::isZero() const
{
    return m_value.IsZero();
}

uint8* BigNumber::AsByteArray(int minSize)
{
    return AsByteArray(minSize, true);
}

uint8* BigNumber::AsByteArray(int minSize, bool reverse)
{
    const size_t length = std::max(size_t(minSize > 0 ? minSize : 0), m_value.ByteLength());

    Erase(m_array);
    if (length == 0)
    {
        // Zero at width zero: a pointer the caller may hand to a hash with a length of
        // zero, never a null one.
        m_array.assign(1, 0);
        return m_array.data();
    }
    // The value is rendered at `length` bytes, so the padding lands at the high end
    // whichever way round the bytes go -- not rendered minimal and then padded, the
    // mistake that once shifted every SRP6 value with a leading zero byte, one login
    // in 256.
    m_array = reverse ? m_value.ToBytesLE(length) : m_value.ToBytesBE(length);
    return m_array.data();
}

const char* BigNumber::AsHexStr()
{
    Erase(m_text);
    m_text = m_value.ToHex();
    return m_text.c_str();
}

const char* BigNumber::AsDecStr()
{
    Erase(m_text);
    m_text = m_value.ToDecimal();
    return m_text.c_str();
}
