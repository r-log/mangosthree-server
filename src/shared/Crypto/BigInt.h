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

#ifndef MANGOS_H_CRYPTO_BIGINT
#define MANGOS_H_CRYPTO_BIGINT

#include "Crypto/Limb.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace MaNGOS
{
    namespace Crypto
    {
        /**
         * @brief An unsigned integer of any size: 64-bit limbs, little-endian, normalised.
         *
         * The general-purpose arithmetic the protocols need around their modular
         * exponentiations: SRP6's `(v * 3 + g^b) mod N`, the reduction of a base before
         * it enters Montgomery form, key parsing, database hex, decimal for logs. None
         * of it is constant-time and none of it is on a secret-dependent path by design;
         * the exponentiation itself lives in Montgomery/ModExp.
         *
         * Subtraction below zero and division by zero are not values: they call
         * CryptoFatal.
         */
        class BigInt
        {
        public:
            BigInt() = default;
            BigInt(uint64_t value);   // NOLINT: implicit on purpose, small constants read naturally

            static BigInt FromBytesLE(const uint8_t* bytes, size_t length);
            static BigInt FromBytesBE(const uint8_t* bytes, size_t length);
            static BigInt FromLimbs(const Limb* limbs, size_t count);

            /// Hex digits only, either case, no prefix, no sign. Anything else leaves
            /// the value zero and returns false.
            bool FromHex(std::string_view hex);
            /// Uppercase, whole bytes (two digits each, no leading zero bytes), "0" for
            /// zero -- the BN_bn2hex shape the database columns and key files hold.
            std::string ToHex() const;
            std::string ToDecimal() const;

            /// Little-endian bytes, minimal length, padded with zeros at the high end
            /// up to minSize.
            std::vector<uint8_t> ToBytesLE(size_t minSize = 0) const;
            /// Big-endian bytes left-padded with zeros to `width`; the minimal encoding
            /// when width is 0; empty when the value does not fit.
            std::vector<uint8_t> ToBytesBE(size_t width) const;

            bool IsZero() const { return m_limbs.empty(); }
            bool IsOdd() const { return !m_limbs.empty() && (m_limbs[0] & 1); }
            bool IsOne() const { return m_limbs.size() == 1 && m_limbs[0] == 1; }
            size_t BitLength() const;
            size_t ByteLength() const { return (BitLength() + 7) / 8; }
            size_t LimbCount() const { return m_limbs.size(); }
            const std::vector<Limb>& Limbs() const { return m_limbs; }
            Limb Low64() const { return m_limbs.empty() ? 0 : m_limbs[0]; }
            bool Bit(size_t index) const;

            int Compare(const BigInt& other) const;
            friend bool operator==(const BigInt& a, const BigInt& b) { return a.Compare(b) == 0; }
            friend bool operator!=(const BigInt& a, const BigInt& b) { return a.Compare(b) != 0; }
            friend bool operator<(const BigInt& a, const BigInt& b)  { return a.Compare(b) < 0; }
            friend bool operator<=(const BigInt& a, const BigInt& b) { return a.Compare(b) <= 0; }
            friend bool operator>(const BigInt& a, const BigInt& b)  { return a.Compare(b) > 0; }
            friend bool operator>=(const BigInt& a, const BigInt& b) { return a.Compare(b) >= 0; }

            BigInt& operator+=(const BigInt& other);
            BigInt& operator-=(const BigInt& other);   // CryptoFatal when *this < other
            BigInt& operator*=(const BigInt& other);
            BigInt& operator/=(const BigInt& other);   // CryptoFatal when other is zero
            BigInt& operator%=(const BigInt& other);
            BigInt& operator<<=(size_t bits);
            BigInt& operator>>=(size_t bits);

            friend BigInt operator+(BigInt a, const BigInt& b) { a += b; return a; }
            friend BigInt operator-(BigInt a, const BigInt& b) { a -= b; return a; }
            friend BigInt operator*(const BigInt& a, const BigInt& b);
            friend BigInt operator/(const BigInt& a, const BigInt& b) { BigInt q, r; DivMod(a, b, q, r); return q; }
            friend BigInt operator%(const BigInt& a, const BigInt& b) { BigInt q, r; DivMod(a, b, q, r); return r; }
            friend BigInt operator<<(BigInt a, size_t bits) { a <<= bits; return a; }
            friend BigInt operator>>(BigInt a, size_t bits) { a >>= bits; return a; }

            /// a = q * d + r, 0 <= r < d. Knuth's Algorithm D on 64-bit limbs.
            static void DivMod(const BigInt& a, const BigInt& d, BigInt& q, BigInt& r);

            /// Division by one limb: returns the remainder, stores the quotient in *this.
            Limb DivModSmall(Limb divisor);

            /// Overwrite the limbs before releasing them (for values that were secrets).
            void SecureClear();

        private:
            void Normalise();

            std::vector<Limb> m_limbs;
        };
    }
}

#endif
