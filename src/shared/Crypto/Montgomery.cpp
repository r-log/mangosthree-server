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

#include "Crypto/Montgomery.h"
#include "Crypto/Fatal.h"
#include "Crypto/SecureZero.h"

#include <cstdlib>
#include <cstring>

namespace MaNGOS
{
    namespace Crypto
    {
        // ------------------------------------------------------------------ the portable kernel
        //
        // CIOS (coarsely integrated operand scanning): for each limb of x, add x_i * y
        // into the accumulator, then add the multiple of m that clears the lowest limb
        // and drop that limb. After k rounds the accumulator holds x * y * R^-1 with one
        // possible extra multiple of m; one masked subtraction makes it canonical.
        //
        // No branch, loop bound or address depends on the values: k rounds of k steps,
        // the same loads and stores every time, a mask instead of an `if` at the end.

        void MontMulPortable(Limb* z, const Limb* x, const Limb* y, const Limb* m, Limb n0inv, size_t k)
        {
            Limb t[MaxMontgomeryLimbs + 2];
            for (size_t j = 0; j <= k + 1; ++j)
            {
                t[j] = 0;
            }

            for (size_t i = 0; i < k; ++i)
            {
                // t += x[i] * y
                const Limb xi = x[i];
                Limb carry = 0;
                for (size_t j = 0; j < k; ++j)
                {
                    Limb hi;
                    Limb lo = MulWide(xi, y[j], hi);
                    Limb c = 0;
                    lo = AddCarry(lo, t[j], c);
                    hi += c;
                    c = 0;
                    lo = AddCarry(lo, carry, c);
                    hi += c;
                    t[j] = lo;
                    carry = hi;
                }
                {
                    Limb c = 0;
                    t[k] = AddCarry(t[k], carry, c);
                    t[k + 1] = c;
                }

                // u = t[0] * n0inv: the multiple of m whose addition zeroes t[0]
                const Limb u = t[0] * n0inv;

                // t = (t + u * m) / 2^64
                {
                    Limb hi;
                    Limb lo = MulWide(u, m[0], hi);
                    Limb c = 0;
                    lo = AddCarry(lo, t[0], c);   // lo is now zero by construction
                    (void)lo;
                    carry = hi + c;
                }
                for (size_t j = 1; j < k; ++j)
                {
                    Limb hi;
                    Limb lo = MulWide(u, m[j], hi);
                    Limb c = 0;
                    lo = AddCarry(lo, t[j], c);
                    hi += c;
                    c = 0;
                    lo = AddCarry(lo, carry, c);
                    hi += c;
                    t[j - 1] = lo;
                    carry = hi;
                }
                {
                    Limb c = 0;
                    t[k - 1] = AddCarry(t[k], carry, c);
                    t[k] = t[k + 1] + c;
                }
            }

            // t (k + 1 limbs) is below 2m: subtract m, keep the difference if it did not
            // go negative -- decided by a mask, not a branch.
            Limb borrow = 0;
            Limb s[MaxMontgomeryLimbs];
            for (size_t j = 0; j < k; ++j)
            {
                s[j] = SubBorrow(t[j], m[j], borrow);
            }
            const Limb keepDifference = CtMask((t[k] != 0) | (borrow == 0));
            for (size_t j = 0; j < k; ++j)
            {
                z[j] = CtSelect(keepDifference, s[j], t[j]);
            }
            SecureZero(t, sizeof t);
            SecureZero(s, sizeof s);
        }

        // ------------------------------------------------------------------ tier selection

        namespace
        {
            MontMulFn SelectKernel()
            {
                const char* forced = std::getenv("MANGOS_CRYPTO_TIER");
                if (forced && std::strcmp(forced, "portable") == 0)
                {
                    return &MontMulPortable;
                }
                if (forced && std::strcmp(forced, "mulx") == 0)
                {
                    return HasMulxTier() ? &MontMulMulx : &MontMulPortable;
                }
                if (HasAsmTier())
                {
                    return &MontMulAsm;
                }
                if (HasMulxTier())
                {
                    return &MontMulMulx;
                }
                return &MontMulPortable;
            }
        }

        MontMulFn ActiveMontMul()
        {
            // A function-local static: selected on first use, whichever translation
            // unit's initialisation asks first, so a context built during static
            // initialisation cannot see an unselected kernel.
            static const MontMulFn kernel = SelectKernel();
            return kernel;
        }

        const char* ActiveTierName()
        {
            const MontMulFn kernel = ActiveMontMul();
            return kernel == &MontMulAsm ? "asm" : kernel == &MontMulMulx ? "mulx" : "portable";
        }

        MontMulFn MontMulFor(size_t k)
        {
            const MontMulFn kernel = ActiveMontMul();
            if (kernel == &MontMulAsm && (k == 0 || k % 4 != 0))
            {
                return HasMulxTier() ? &MontMulMulx : &MontMulPortable;
            }
            return kernel;
        }

        // ------------------------------------------------------------------ the context

        MontgomeryContext::MontgomeryContext(const BigInt& modulus)
            : m_k(modulus.LimbCount()), m_n0inv(0), m_kernel(MontMulFor(modulus.LimbCount()))
        {
            if (modulus.IsZero() || !modulus.IsOdd() || modulus.IsOne())
            {
                CryptoFatal("MontgomeryContext: the modulus must be odd and above 1");
            }
            if (m_k > MaxMontgomeryLimbs)
            {
                CryptoFatal("MontgomeryContext: the modulus is wider than the kernels support");
            }
            m_m = modulus.Limbs();

            // n0inv = -m[0]^-1 mod 2^64 by Newton's iteration: an odd number is its own
            // inverse modulo 8, and every step doubles the correct bits (3 -> 6 -> ... -> 96).
            Limb inv = m_m[0];
            for (int i = 0; i < 6; ++i)
            {
                inv *= Limb(2) - m_m[0] * inv;
            }
            m_n0inv = Limb(0) - inv;

            // R^2 mod m by 128k masked modular doublings of 1: the loop runs the same
            // way for every modulus of this width.
            m_r2.assign(m_k, 0);
            m_r2[0] = 1;
            std::vector<Limb> doubled(m_k), reduced(m_k);
            for (size_t step = 0; step < 128 * m_k; ++step)
            {
                Limb carry = 0;
                for (size_t j = 0; j < m_k; ++j)
                {
                    const Limb v = m_r2[j];
                    doubled[j] = (v << 1) | carry;
                    carry = v >> 63;
                }
                Limb borrow = 0;
                for (size_t j = 0; j < m_k; ++j)
                {
                    reduced[j] = SubBorrow(doubled[j], m_m[j], borrow);
                }
                const Limb keepReduced = CtMask((carry != 0) | (borrow == 0));
                for (size_t j = 0; j < m_k; ++j)
                {
                    m_r2[j] = CtSelect(keepReduced, reduced[j], doubled[j]);
                }
            }

            // R mod m = Mont(1, R^2)
            m_one.assign(m_k, 0);
            std::vector<Limb> one(m_k, 0);
            one[0] = 1;
            Mul(m_one.data(), one.data(), m_r2.data());
        }

        MontgomeryContext::~MontgomeryContext()
        {
            for (std::vector<Limb>* v : { &m_m, &m_r2, &m_one })
            {
                if (!v->empty())
                {
                    SecureZero(v->data(), v->size() * LimbBytes);
                }
            }
            m_n0inv = 0;
        }

        void MontgomeryContext::Mul(Limb* z, const Limb* x, const Limb* y) const
        {
            m_kernel(z, x, y, m_m.data(), m_n0inv, m_k);
        }

        void MontgomeryContext::ToMont(Limb* z, const BigInt& x) const
        {
            BigInt reduced = x % ModulusValue();
            std::vector<Limb> limbs(m_k, 0);
            const std::vector<Limb>& src = reduced.Limbs();
            for (size_t j = 0; j < src.size(); ++j)
            {
                limbs[j] = src[j];
            }
            Mul(z, limbs.data(), m_r2.data());
            SecureZero(limbs.data(), limbs.size() * LimbBytes);
            reduced.SecureClear();
        }

        BigInt MontgomeryContext::FromMont(const Limb* x) const
        {
            std::vector<Limb> one(m_k, 0), out(m_k, 0);
            one[0] = 1;
            Mul(out.data(), x, one.data());
            BigInt value = BigInt::FromLimbs(out.data(), m_k);
            SecureZero(out.data(), out.size() * LimbBytes);
            return value;
        }
    }
}
