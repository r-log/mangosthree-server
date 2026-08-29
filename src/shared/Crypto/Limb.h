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

#ifndef MANGOS_H_CRYPTO_LIMB
#define MANGOS_H_CRYPTO_LIMB

#include <cstddef>
#include <cstdint>

#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#endif

namespace MaNGOS
{
    namespace Crypto
    {
        /// The big-integer digit: 64 bits, little-endian arrays of them.
        using Limb = uint64_t;
        constexpr size_t LimbBits  = 64;
        constexpr size_t LimbBytes = 8;

        /// a * b as a 128-bit product: returns the low limb, stores the high one.
        inline Limb MulWide(Limb a, Limb b, Limb& hi)
        {
#if defined(_MSC_VER) && defined(_M_X64)
            return _umul128(a, b, &hi);
#elif defined(__SIZEOF_INT128__)
            const unsigned __int128 p = static_cast<unsigned __int128>(a) * b;
            hi = static_cast<Limb>(p >> 64);
            return static_cast<Limb>(p);
#else
            const uint64_t a0 = uint32_t(a), a1 = a >> 32, b0 = uint32_t(b), b1 = b >> 32;
            const uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
            const uint64_t mid = (p00 >> 32) + uint32_t(p01) + uint32_t(p10);
            hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
            return (mid << 32) | uint32_t(p00);
#endif
        }

        /// (hi:lo) / d with hi < d: returns the quotient, stores the remainder.
        inline Limb DivWide(Limb hi, Limb lo, Limb d, Limb& remainder)
        {
#if defined(_MSC_VER) && defined(_M_X64)
            return _udiv128(hi, lo, d, &remainder);
#elif defined(__SIZEOF_INT128__)
            const unsigned __int128 n = (static_cast<unsigned __int128>(hi) << 64) | lo;
            remainder = static_cast<Limb>(n % d);
            return static_cast<Limb>(n / d);
#else
            // Bit-serial long division: 64 steps of shift-and-subtract. Correct, slow,
            // only for targets with neither a 128-bit type nor the intrinsic.
            Limb q = 0;
            for (int i = 0; i < 64; ++i)
            {
                const Limb top = hi >> 63;
                hi = (hi << 1) | (lo >> 63);
                lo <<= 1;
                q <<= 1;
                if (top || hi >= d)
                {
                    hi -= d;
                    q |= 1;
                }
            }
            remainder = hi;
            return q;
#endif
        }

        /// a + b + carry; carry (0 or 1) is updated.
        inline Limb AddCarry(Limb a, Limb b, Limb& carry)
        {
            const Limb s = a + b;
            const Limb c1 = Limb(s < a);
            const Limb t = s + carry;
            const Limb c2 = Limb(t < s);
            carry = c1 | c2;
            return t;
        }

        /// a - b - borrow; borrow (0 or 1) is updated.
        inline Limb SubBorrow(Limb a, Limb b, Limb& borrow)
        {
            const Limb d = a - b;
            const Limb b1 = Limb(a < b);
            const Limb t = d - borrow;
            const Limb b2 = Limb(d < borrow);
            borrow = b1 | b2;
            return t;
        }

        /// All ones when the condition holds, zero otherwise -- no branch.
        inline Limb CtMask(bool condition)
        {
            return Limb(0) - Limb(condition);
        }

        /// whenSet if the mask is all ones, whenClear if it is zero -- no branch.
        inline Limb CtSelect(Limb mask, Limb whenSet, Limb whenClear)
        {
            return (whenSet & mask) | (whenClear & ~mask);
        }

        /// Leading zero bits of a non-zero limb.
        inline unsigned CountLeadingZeros(Limb v)
        {
#if defined(_MSC_VER) && defined(_M_X64)
            unsigned long index;
            _BitScanReverse64(&index, v);
            return 63u - unsigned(index);
#elif defined(__GNUC__) || defined(__clang__)
            return unsigned(__builtin_clzll(v));
#else
            unsigned n = 0;
            while (!(v & (Limb(1) << 63))) { v <<= 1; ++n; }
            return n;
#endif
        }
    }
}

#endif
