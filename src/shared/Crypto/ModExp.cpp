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

#include "Crypto/ModExp.h"
#include "Crypto/Fatal.h"
#include "Crypto/SecureZero.h"

#include <algorithm>
#include <vector>

namespace MaNGOS
{
    namespace Crypto
    {
        namespace
        {
            /// The w bits of the padded exponent starting at bitPosition, low bit first.
            /// The position is public (it is the loop counter), so the limb split is a
            /// plain comparison on it.
            Limb Window(const Limb* exponent, size_t limbs, size_t bitPosition, unsigned width)
            {
                const size_t limb = bitPosition / LimbBits;
                const unsigned offset = unsigned(bitPosition % LimbBits);
                Limb value = exponent[limb] >> offset;
                if (offset + width > LimbBits && limb + 1 < limbs)
                {
                    value |= exponent[limb + 1] << (LimbBits - offset);
                }
                return value & ((Limb(1) << width) - 1);
            }
        }

        BigInt ModExp(const BigInt& base, const BigInt& exponent, const MontgomeryContext& context, ExponentKind kind, size_t exponentLimbs)
        {
            const size_t k = context.Limbs();
            if (exponentLimbs == 0)
            {
                exponentLimbs = kind == ExponentKind::Public ? std::max<size_t>(1, exponent.LimbCount()) : k;
            }
            if (exponent.LimbCount() > exponentLimbs)
            {
                if (kind == ExponentKind::Secret)
                {
                    CryptoFatal("ModExp: a secret exponent wider than its declared width");
                }
                exponentLimbs = exponent.LimbCount();
            }
            // A short public exponent (65537) is cheapest with no table at all: one
            // squaring per bit and a multiplication only where a bit is set.
            const bool shortPublic = kind == ExponentKind::Public && exponent.BitLength() <= 64;
            const unsigned w = shortPublic ? 1 : (k >= 16 ? 5 : 4);
            const size_t tableSize = size_t(1) << w;

            // The table of base^0 .. base^(2^w - 1) in Montgomery form.
            std::vector<Limb> table(tableSize * k);
            for (size_t j = 0; j < k; ++j)
            {
                table[j] = context.One()[j];
            }
            context.ToMont(&table[k], base);
            for (size_t i = 2; i < tableSize; ++i)
            {
                context.Mul(&table[i * k], &table[(i - 1) * k], &table[k]);
            }

            // The exponent, zero-padded to the declared width. A public exponent is
            // walked over its own bit length instead.
            std::vector<Limb> e(exponentLimbs, 0);
            for (size_t j = 0; j < exponent.LimbCount(); ++j)
            {
                e[j] = exponent.Limbs()[j];
            }
            const size_t bits = kind == ExponentKind::Public ? exponent.BitLength() : exponentLimbs * LimbBits;
            const size_t windows = (bits + w - 1) / w;

            std::vector<Limb> acc(k), selected(k);
            for (size_t j = 0; j < k; ++j)
            {
                acc[j] = context.One()[j];
            }
            for (size_t win = windows; win-- > 0;)
            {
                for (unsigned s = 0; s < w; ++s)
                {
                    context.Sqr(acc.data(), acc.data());
                }
                const Limb index = Window(e.data(), exponentLimbs, win * w, w);
                if (shortPublic)
                {
                    // The exponent is public: a zero window costs nothing.
                    if (index)
                    {
                        context.Mul(acc.data(), acc.data(), &table[k]);
                    }
                    continue;
                }
                // Fetch table[index] by touching every entry: no address depends on the exponent.
                for (size_t j = 0; j < k; ++j)
                {
                    selected[j] = 0;
                }
                for (size_t i = 0; i < tableSize; ++i)
                {
                    const Limb mask = CtMask((Limb(i) ^ index) == 0);
                    const Limb* entry = &table[i * k];
                    for (size_t j = 0; j < k; ++j)
                    {
                        selected[j] |= entry[j] & mask;
                    }
                }
                context.Mul(acc.data(), acc.data(), selected.data());
            }

            BigInt result = context.FromMont(acc.data());
            SecureZero(acc.data(), acc.size() * LimbBytes);
            SecureZero(selected.data(), selected.size() * LimbBytes);
            SecureZero(e.data(), e.size() * LimbBytes);
            SecureZero(table.data(), table.size() * LimbBytes);
            return result;
        }

        BigInt ModExp(const BigInt& base, const BigInt& exponent, const BigInt& m, ExponentKind kind)
        {
            if (m.IsZero())
            {
                CryptoFatal("ModExp: the modulus is zero");
            }
            if (m.IsOne())
            {
                return BigInt();
            }
            if (m.IsOdd() && m.LimbCount() >= 2)
            {
                MontgomeryContext context(m);
                return ModExp(base, exponent, context, kind, std::max(m.LimbCount(), exponent.LimbCount()));
            }
            // Plain square-and-multiply with division: the public, non-Montgomery path.
            BigInt result(1);
            BigInt b = base % m;
            for (size_t bit = exponent.BitLength(); bit-- > 0;)
            {
                result = (result * result) % m;
                if (exponent.Bit(bit))
                {
                    result = (result * b) % m;
                }
            }
            return result;
        }

        BigInt ModInverse(const BigInt& a, const BigInt& m)
        {
            if (m < BigInt(2))
            {
                return BigInt();
            }
            BigInt r0 = m, r1 = a % m;
            if (r1.IsZero())
            {
                return BigInt();
            }
            // Extended Euclid with the coefficients kept as residues modulo m, so no
            // signed arithmetic is needed: t1 tracks the coefficient of a.
            BigInt t0, t1(1);
            while (!r1.IsZero())
            {
                BigInt q, r2;
                BigInt::DivMod(r0, r1, q, r2);
                BigInt t2 = (t0 + m - (q * t1) % m) % m;
                r0 = r1;
                r1 = r2;
                t0 = t1;
                t1 = t2;
            }
            if (!r0.IsOne())
            {
                return BigInt();
            }
            return t0;
        }
    }
}
