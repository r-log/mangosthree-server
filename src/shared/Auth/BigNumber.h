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

#ifndef MANGOS_H_AUTH_BIGNUMBER
#define MANGOS_H_AUTH_BIGNUMBER

#include "Platform/Define.h"
#include "Crypto/BigInt.h"

#include <string>
#include <vector>

/**
 * @brief The big integer the protocol code was written against, over the tree's own
 * arithmetic (src/shared/Crypto).
 *
 * The interface is the historical one and its byte conventions are load-bearing:
 * SetBinary reads little-endian bytes and AsByteArray writes them, padded at the high
 * end to the requested width; AsByteArray(size, false) writes big-endian, left-padded;
 * AsHexStr is uppercase in whole bytes ("01", "0A2B"; "0" for zero), which is the
 * shape the database columns and the key files hold. The buffers AsByteArray, AsHexStr
 * and AsDecStr return belong to the object and are replaced by the next call of the
 * same method on it.
 *
 * Every value is erased when the object goes: session keys and ephemerals pass through
 * this class.
 */
class BigNumber
{
    public:
        BigNumber();
        BigNumber(const BigNumber& bn);
        BigNumber(uint32 val);
        ~BigNumber();

        void SetDword(uint32 val);
        void SetQword(uint64 val);
        /// From `len` little-endian bytes; zero when there are none.
        void SetBinary(const uint8* bytes, int len);
        /// From big-endian hexadecimal digits; anything else sets zero and is logged.
        void SetHexStr(const char* str);

        /// A random value of exactly `numbits` bits -- top bit set, odd -- from the OS
        /// CSPRNG: the shape the SRP6 salts and ephemerals have always had.
        void SetRand(int numbits);

        BigNumber operator=(const BigNumber& bn);

        BigNumber operator+=(const BigNumber& bn);
        BigNumber operator+(const BigNumber& bn)
        {
            BigNumber t(*this);
            return t += bn;
        }
        /// Unsigned: subtracting the larger value is an assertion failure.
        BigNumber operator-=(const BigNumber& bn);
        BigNumber operator-(const BigNumber& bn)
        {
            BigNumber t(*this);
            return t -= bn;
        }
        BigNumber operator*=(const BigNumber& bn);
        BigNumber operator*(const BigNumber& bn)
        {
            BigNumber t(*this);
            return t *= bn;
        }
        BigNumber operator/=(const BigNumber& bn);
        BigNumber operator/(const BigNumber& bn)
        {
            BigNumber t(*this);
            return t /= bn;
        }
        BigNumber operator%=(const BigNumber& bn);
        BigNumber operator%(const BigNumber& bn)
        {
            BigNumber t(*this);
            return t %= bn;
        }

        bool isZero() const;

        /**
         * @brief (this ^ bn1) mod bn2, with the exponent treated as secret: the running
         * time does not depend on its bits. Zero when the modulus is zero.
         */
        BigNumber ModExp(const BigNumber& bn1, const BigNumber& bn2);
        /// this ^ bn, without a modulus.
        BigNumber Exp(const BigNumber& bn);

        /// The minimal number of bytes; 0 for zero.
        int GetNumBytes(void);

        /// The low 32 bits.
        uint32 AsDword();
        /// Little-endian, padded with zero bytes at the high end to at least `minSize`.
        uint8* AsByteArray(int minSize = 0);
        /// `reverse` = true is AsByteArray(minSize); false writes big-endian, left-padded.
        uint8* AsByteArray(int minSize, bool reverse);
        /// Uppercase hexadecimal in whole bytes; "0" for zero.
        const char* AsHexStr();
        const char* AsDecStr();

    private:
        MaNGOS::Crypto::BigInt m_value;
        std::vector<uint8>     m_array;   /**< The buffer AsByteArray returns */
        std::string            m_text;    /**< The buffer AsHexStr and AsDecStr return */
};
#endif
