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

#include "Crypto/BigInt.h"
#include "Crypto/Fatal.h"
#include "Crypto/SecureZero.h"

#include <algorithm>
#include <cstring>

namespace MaNGOS
{
    namespace Crypto
    {
        BigInt::BigInt(uint64_t value)
        {
            if (value)
            {
                m_limbs.push_back(value);
            }
        }

        void BigInt::Normalise()
        {
            while (!m_limbs.empty() && m_limbs.back() == 0)
            {
                m_limbs.pop_back();
            }
        }

        BigInt BigInt::FromBytesLE(const uint8_t* bytes, size_t length)
        {
            BigInt v;
            v.m_limbs.resize((length + LimbBytes - 1) / LimbBytes, 0);
            for (size_t i = 0; i < length; ++i)
            {
                v.m_limbs[i / LimbBytes] |= Limb(bytes[i]) << (8 * (i % LimbBytes));
            }
            v.Normalise();
            return v;
        }

        BigInt BigInt::FromBytesBE(const uint8_t* bytes, size_t length)
        {
            BigInt v;
            v.m_limbs.resize((length + LimbBytes - 1) / LimbBytes, 0);
            for (size_t i = 0; i < length; ++i)
            {
                const size_t fromEnd = length - 1 - i;
                v.m_limbs[fromEnd / LimbBytes] |= Limb(bytes[i]) << (8 * (fromEnd % LimbBytes));
            }
            v.Normalise();
            return v;
        }

        BigInt BigInt::FromLimbs(const Limb* limbs, size_t count)
        {
            BigInt v;
            v.m_limbs.assign(limbs, limbs + count);
            v.Normalise();
            return v;
        }

        bool BigInt::FromHex(std::string_view hex)
        {
            m_limbs.clear();
            std::vector<Limb> limbs((hex.size() + 15) / 16, 0);
            for (size_t i = 0; i < hex.size(); ++i)
            {
                const char c = hex[hex.size() - 1 - i];
                Limb nibble;
                if (c >= '0' && c <= '9')      { nibble = Limb(c - '0'); }
                else if (c >= 'a' && c <= 'f') { nibble = Limb(c - 'a' + 10); }
                else if (c >= 'A' && c <= 'F') { nibble = Limb(c - 'A' + 10); }
                else                           { return false; }
                limbs[i / 16] |= nibble << (4 * (i % 16));
            }
            m_limbs.swap(limbs);
            Normalise();
            return true;
        }

        std::string BigInt::ToHex() const
        {
            if (m_limbs.empty())
            {
                return "0";
            }
            // Whole bytes, two digits each, leading zero bytes dropped: the shape
            // BN_bn2hex produced and the database and key files hold ("01", "0A2B").
            static const char* digits = "0123456789ABCDEF";
            const size_t length = ByteLength();
            std::string out;
            out.reserve(length * 2);
            for (size_t i = length; i-- > 0;)
            {
                const unsigned byte = unsigned((m_limbs[i / LimbBytes] >> (8 * (i % LimbBytes))) & 0xFF);
                out.push_back(digits[byte >> 4]);
                out.push_back(digits[byte & 0xF]);
            }
            return out;
        }

        std::string BigInt::ToDecimal() const
        {
            if (m_limbs.empty())
            {
                return "0";
            }
            // Peel 19 decimal digits at a time (10^19 fits a limb).
            const Limb chunk = 10000000000000000000ull;
            BigInt work(*this);
            std::vector<Limb> parts;
            while (!work.IsZero())
            {
                parts.push_back(work.DivModSmall(chunk));
            }
            std::string out = std::to_string(parts.back());
            for (size_t i = parts.size() - 1; i-- > 0;)
            {
                std::string piece = std::to_string(parts[i]);
                out.append(19 - piece.size(), '0');
                out += piece;
            }
            return out;
        }

        std::vector<uint8_t> BigInt::ToBytesLE(size_t minSize) const
        {
            const size_t length = ByteLength();
            std::vector<uint8_t> out(std::max(length, minSize), 0);
            for (size_t i = 0; i < length; ++i)
            {
                out[i] = uint8_t(m_limbs[i / LimbBytes] >> (8 * (i % LimbBytes)));
            }
            return out;
        }

        std::vector<uint8_t> BigInt::ToBytesBE(size_t width) const
        {
            const size_t length = ByteLength();
            if (width == 0)
            {
                width = length;
            }
            if (length > width)
            {
                return {};
            }
            std::vector<uint8_t> out(width, 0);
            for (size_t i = 0; i < length; ++i)
            {
                out[width - 1 - i] = uint8_t(m_limbs[i / LimbBytes] >> (8 * (i % LimbBytes)));
            }
            return out;
        }

        size_t BigInt::BitLength() const
        {
            if (m_limbs.empty())
            {
                return 0;
            }
            return m_limbs.size() * LimbBits - CountLeadingZeros(m_limbs.back());
        }

        bool BigInt::Bit(size_t index) const
        {
            const size_t limb = index / LimbBits;
            if (limb >= m_limbs.size())
            {
                return false;
            }
            return (m_limbs[limb] >> (index % LimbBits)) & 1;
        }

        int BigInt::Compare(const BigInt& other) const
        {
            if (m_limbs.size() != other.m_limbs.size())
            {
                return m_limbs.size() < other.m_limbs.size() ? -1 : 1;
            }
            for (size_t i = m_limbs.size(); i-- > 0;)
            {
                if (m_limbs[i] != other.m_limbs[i])
                {
                    return m_limbs[i] < other.m_limbs[i] ? -1 : 1;
                }
            }
            return 0;
        }

        BigInt& BigInt::operator+=(const BigInt& other)
        {
            if (other.m_limbs.size() > m_limbs.size())
            {
                m_limbs.resize(other.m_limbs.size(), 0);
            }
            Limb carry = 0;
            for (size_t i = 0; i < m_limbs.size(); ++i)
            {
                const Limb b = i < other.m_limbs.size() ? other.m_limbs[i] : 0;
                m_limbs[i] = AddCarry(m_limbs[i], b, carry);
            }
            if (carry)
            {
                m_limbs.push_back(carry);
            }
            return *this;
        }

        BigInt& BigInt::operator-=(const BigInt& other)
        {
            if (Compare(other) < 0)
            {
                CryptoFatal("BigInt: subtraction below zero");
            }
            Limb borrow = 0;
            for (size_t i = 0; i < m_limbs.size(); ++i)
            {
                const Limb b = i < other.m_limbs.size() ? other.m_limbs[i] : 0;
                m_limbs[i] = SubBorrow(m_limbs[i], b, borrow);
            }
            Normalise();
            return *this;
        }

        BigInt operator*(const BigInt& a, const BigInt& b)
        {
            BigInt out;
            if (a.IsZero() || b.IsZero())
            {
                return out;
            }
            out.m_limbs.assign(a.m_limbs.size() + b.m_limbs.size(), 0);
            for (size_t i = 0; i < a.m_limbs.size(); ++i)
            {
                Limb carry = 0;
                const Limb ai = a.m_limbs[i];
                for (size_t j = 0; j < b.m_limbs.size(); ++j)
                {
                    Limb hi;
                    Limb lo = MulWide(ai, b.m_limbs[j], hi);
                    Limb c = 0;
                    lo = AddCarry(lo, out.m_limbs[i + j], c);
                    hi += c;
                    c = 0;
                    lo = AddCarry(lo, carry, c);
                    hi += c;
                    out.m_limbs[i + j] = lo;
                    carry = hi;
                }
                out.m_limbs[i + b.m_limbs.size()] = carry;
            }
            out.Normalise();
            return out;
        }

        BigInt& BigInt::operator*=(const BigInt& other)
        {
            *this = *this * other;
            return *this;
        }

        BigInt& BigInt::operator/=(const BigInt& other)
        {
            BigInt q, r;
            DivMod(*this, other, q, r);
            *this = q;
            return *this;
        }

        BigInt& BigInt::operator%=(const BigInt& other)
        {
            BigInt q, r;
            DivMod(*this, other, q, r);
            *this = r;
            return *this;
        }

        BigInt& BigInt::operator<<=(size_t bits)
        {
            if (m_limbs.empty() || bits == 0)
            {
                return *this;
            }
            const size_t limbShift = bits / LimbBits;
            const unsigned bitShift = unsigned(bits % LimbBits);
            std::vector<Limb> out(m_limbs.size() + limbShift + 1, 0);
            for (size_t i = 0; i < m_limbs.size(); ++i)
            {
                out[i + limbShift] |= m_limbs[i] << bitShift;
                if (bitShift)
                {
                    out[i + limbShift + 1] |= m_limbs[i] >> (LimbBits - bitShift);
                }
            }
            m_limbs.swap(out);
            Normalise();
            return *this;
        }

        BigInt& BigInt::operator>>=(size_t bits)
        {
            const size_t limbShift = bits / LimbBits;
            const unsigned bitShift = unsigned(bits % LimbBits);
            if (limbShift >= m_limbs.size())
            {
                m_limbs.clear();
                return *this;
            }
            std::vector<Limb> out(m_limbs.size() - limbShift, 0);
            for (size_t i = 0; i < out.size(); ++i)
            {
                out[i] = m_limbs[i + limbShift] >> bitShift;
                if (bitShift && i + limbShift + 1 < m_limbs.size())
                {
                    out[i] |= m_limbs[i + limbShift + 1] << (LimbBits - bitShift);
                }
            }
            m_limbs.swap(out);
            Normalise();
            return *this;
        }

        Limb BigInt::DivModSmall(Limb divisor)
        {
            if (divisor == 0)
            {
                CryptoFatal("BigInt: division by zero");
            }
            Limb remainder = 0;
            for (size_t i = m_limbs.size(); i-- > 0;)
            {
                m_limbs[i] = DivWide(remainder, m_limbs[i], divisor, remainder);
            }
            Normalise();
            return remainder;
        }

        void BigInt::DivMod(const BigInt& a, const BigInt& d, BigInt& q, BigInt& r)
        {
            if (d.IsZero())
            {
                CryptoFatal("BigInt: division by zero");
            }
            if (a.Compare(d) < 0)
            {
                q = BigInt();
                r = a;
                return;
            }
            if (d.m_limbs.size() == 1)
            {
                BigInt quotient(a);
                const Limb rem = quotient.DivModSmall(d.m_limbs[0]);
                q = quotient;
                r = BigInt(rem);
                return;
            }

            // Knuth, TAOCP vol. 2, 4.3.1, Algorithm D, with 64-bit digits.
            const size_t n = d.m_limbs.size();
            const size_t m = a.m_limbs.size() - n;
            const unsigned s = CountLeadingZeros(d.m_limbs.back());   // D1: normalise so the top digit of v has its high bit set

            std::vector<Limb> v(n), u(a.m_limbs.size() + 1, 0);
            for (size_t i = n; i-- > 0;)
            {
                v[i] = (d.m_limbs[i] << s) | ((s && i > 0) ? (d.m_limbs[i - 1] >> (LimbBits - s)) : 0);
            }
            for (size_t i = a.m_limbs.size(); i-- > 0;)
            {
                u[i] = (a.m_limbs[i] << s) | ((s && i > 0) ? (a.m_limbs[i - 1] >> (LimbBits - s)) : 0);
            }
            u[a.m_limbs.size()] = s ? (a.m_limbs.back() >> (LimbBits - s)) : 0;

            std::vector<Limb> quotient(m + 1, 0);
            const Limb vn1 = v[n - 1], vn2 = v[n - 2];

            for (size_t j = m + 1; j-- > 0;)
            {
                // D3: estimate qhat from the top two digits of the remainder and the top digit of v.
                const Limb un = u[j + n], un1 = u[j + n - 1], un2 = u[j + n - 2];
                Limb qhat, rhat;
                bool rhatOverflow = false;
                if (un >= vn1)
                {
                    // un == vn1 here (the remainder is below v * B): qhat = B - 1 and
                    // rhat = un1 + vn1, which may not fit a limb.
                    qhat = ~Limb(0);
                    Limb c = 0;
                    rhat = AddCarry(un1, vn1, c);
                    rhatOverflow = c != 0;
                }
                else
                {
                    qhat = DivWide(un, un1, vn1, rhat);
                }
                // Refine: while qhat * vn2 > rhat * B + un2, decrement (at most twice).
                while (!rhatOverflow)
                {
                    Limb ph;
                    const Limb pl = MulWide(qhat, vn2, ph);
                    if (ph < rhat || (ph == rhat && pl <= un2))
                    {
                        break;
                    }
                    --qhat;
                    Limb c = 0;
                    rhat = AddCarry(rhat, vn1, c);
                    rhatOverflow = c != 0;
                }

                // D4: multiply and subtract qhat * v from u[j .. j+n].
                Limb carry = 0, borrow = 0;
                for (size_t i = 0; i < n; ++i)
                {
                    Limb ph;
                    Limb pl = MulWide(qhat, v[i], ph);
                    Limb c = 0;
                    pl = AddCarry(pl, carry, c);
                    ph += c;
                    carry = ph;
                    u[i + j] = SubBorrow(u[i + j], pl, borrow);
                }
                u[j + n] = SubBorrow(u[j + n], carry, borrow);

                // D5/D6: qhat was one too large; add v back.
                if (borrow)
                {
                    --qhat;
                    Limb c = 0;
                    for (size_t i = 0; i < n; ++i)
                    {
                        u[i + j] = AddCarry(u[i + j], v[i], c);
                    }
                    u[j + n] += c;
                }
                quotient[j] = qhat;
            }

            // D8: the remainder is u[0 .. n-1], shifted back.
            std::vector<Limb> remainder(n, 0);
            for (size_t i = 0; i < n; ++i)
            {
                remainder[i] = (u[i] >> s) | ((s && i + 1 < n + 1) ? (u[i + 1] << (LimbBits - s)) : 0);
            }
            q.m_limbs.swap(quotient);
            q.Normalise();
            r.m_limbs.swap(remainder);
            r.Normalise();
        }

        void BigInt::SecureClear()
        {
            if (!m_limbs.empty())
            {
                SecureZero(m_limbs.data(), m_limbs.size() * LimbBytes);
            }
            m_limbs.clear();
        }
    }
}
