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

#ifndef MANGOS_H_CRYPTO_MODEXP
#define MANGOS_H_CRYPTO_MODEXP

#include "Crypto/BigInt.h"
#include "Crypto/Montgomery.h"

namespace MaNGOS
{
    namespace Crypto
    {
        /**
         * @brief base^exponent mod m over a Montgomery context: the constant-time path.
         *
         * Fixed windows (5 bits for 16 limbs and up, 4 below), the exponent copied into
         * a zero-padded buffer so the number of windows depends on the widths alone,
         * a multiplication in every window including the zero ones, and the table entry
         * fetched by a masked pass over the whole table -- no secret decides a branch or
         * an address. The base may be any width; it is reduced on the way in.
         */
        BigInt ModExp(const BigInt& base, const BigInt& exponent, const MontgomeryContext& context);

        /**
         * @brief base^exponent mod m for any modulus.
         *
         * An odd modulus of two limbs or more takes the Montgomery path above (a context
         * built for the call); an even or single-limb modulus takes plain square-and-
         * multiply with division -- the protocols never exponentiate under such a
         * modulus, so that path is never secret-dependent. m = 0 is fatal; m = 1 gives 0.
         */
        BigInt ModExp(const BigInt& base, const BigInt& exponent, const BigInt& m);

        /// a^-1 mod m, or zero when gcd(a, m) != 1 or m < 2. Extended Euclid: variable
        /// time, for public or fresh random operands (the blinding factor, key generation).
        BigInt ModInverse(const BigInt& a, const BigInt& m);
    }
}

#endif
