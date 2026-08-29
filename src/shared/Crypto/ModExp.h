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
         * @brief Whether the exponent is a secret.
         *
         * Secret (the private exponent, SRP6's b and x): the exponent is padded to the
         * modulus width and every window is processed, so the work depends on the widths
         * alone. Public (65537 in RSA, the client's side of the protocol): the exponent's
         * own bit length is used -- 17 bits cost 17 squarings, not 2048. Nothing else
         * differs; the two give the same result.
         */
        enum class ExponentKind
        {
            Secret,
            Public
        };

        /**
         * @brief base^exponent mod m over a Montgomery context: the constant-time path.
         *
         * Fixed windows (5 bits for 16 limbs and up, 4 below), the exponent copied into
         * a zero-padded buffer of a public width so the number of windows depends on
         * that width alone, a multiplication in every window including the zero ones,
         * and the table entry fetched by a masked pass over the whole table -- no secret
         * decides a branch or an address. The base may be any width; it is reduced on
         * the way in.
         *
         * `exponentLimbs` is that public width. For a secret exponent it defaults to
         * the modulus width, which every protocol exponent fits (d < n, SRP6's b and x
         * below N); a secret wider than the declared width is fatal, because deriving
         * the width from the value would make the work follow the secret. For a public
         * exponent the width is its own bit length.
         */
        BigInt ModExp(const BigInt& base, const BigInt& exponent, const MontgomeryContext& context,
                      ExponentKind kind = ExponentKind::Secret, size_t exponentLimbs = 0);

        /**
         * @brief base^exponent mod m for any modulus.
         *
         * An odd modulus of two limbs or more takes the Montgomery path above (a context
         * built for the call); an even or single-limb modulus takes plain square-and-
         * multiply with division -- the protocols never exponentiate under such a
         * modulus, so that path is never secret-dependent. m = 0 is fatal; m = 1 gives 0.
         *
         * The exponent width passed to the Montgomery path is the larger of the modulus
         * width and the exponent's own limb count -- an exponent wider than the modulus
         * therefore sets the amount of work by its width. A secret wider than its
         * modulus must go through the context overload with the field's width.
         */
        BigInt ModExp(const BigInt& base, const BigInt& exponent, const BigInt& m,
                      ExponentKind kind = ExponentKind::Secret);

        /// a^-1 mod m, or zero when gcd(a, m) != 1 or m < 2. Extended Euclid: variable
        /// time, for public or fresh random operands (the blinding factor, key generation).
        BigInt ModInverse(const BigInt& a, const BigInt& m);
    }
}

#endif
