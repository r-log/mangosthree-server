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

#ifndef MANGOS_H_CRYPTO_MONTGOMERY
#define MANGOS_H_CRYPTO_MONTGOMERY

#include "Crypto/BigInt.h"
#include "Crypto/Limb.h"

#include <cstddef>
#include <vector>

namespace MaNGOS
{
    namespace Crypto
    {
        /// The largest modulus the kernels take: 128 limbs = 8192 bits. RSA-2048 needs
        /// 32 (16 per CRT half), SRP6 needs 4; the tests go to 64.
        constexpr size_t MaxMontgomeryLimbs = 128;

        /**
         * @brief z = x * y * R^-1 mod m, R = 2^(64k): the Montgomery product kernel.
         *
         * Domain: x and y canonical (< m), m odd with a non-zero top limb, k limbs each;
         * n0inv = -m^-1 mod 2^64. The result is canonical. z may alias x or y. The
         * running time and the memory access pattern depend on k only.
         */
        using MontMulFn = void (*)(Limb* z, const Limb* x, const Limb* y, const Limb* m, Limb n0inv, size_t k);

        /// The portable tier: plain C++ on the Limb primitives. Always available.
        void MontMulPortable(Limb* z, const Limb* x, const Limb* y, const Limb* m, Limb n0inv, size_t k);

        /// The x86-64 intrinsic tier (`mulx`, BMI2). Present only where it compiles; may
        /// not run on this CPU -- ask HasMulxTier().
        void MontMulMulx(Limb* z, const Limb* x, const Limb* y, const Limb* m, Limb n0inv, size_t k);
        bool HasMulxTier();

        /// The assembly tier (`mulx/adcx/adox`, BMI2 + ADX; MontgomeryAsm.S): k must be a
        /// positive multiple of 4. Present only where it was assembled and the CPU has
        /// ADX -- ask HasAsmTier(); MontMulFor() routes other widths down a tier.
        void MontMulAsm(Limb* z, const Limb* x, const Limb* y, const Limb* m, Limb n0inv, size_t k);
        bool HasAsmTier();
        /// Whether this build carries the assembly tier at all (x86-64 Linux and MSVC
        /// builds do); with it compiled in and the CPU able, the active tier must be `asm`.
        bool AssemblyTierCompiledIn();

        /// The kernel selected on first use (CPUID: asm, then mulx, then portable;
        /// `MANGOS_CRYPTO_TIER=portable|mulx|asm` forces one that is available) and its
        /// name for reports.
        MontMulFn ActiveMontMul();
        const char* ActiveTierName();

        /// The kernel a context of k limbs uses: the active one, or the next tier down
        /// when the assembly tier cannot take k.
        MontMulFn MontMulFor(size_t k);

        /**
         * @brief Everything Montgomery arithmetic needs about one odd modulus.
         *
         * Built once per modulus (RSA keys and SRP6's N keep theirs), the constants come
         * from a fixed number of masked modular doublings, so building one takes time
         * that depends on the modulus width, not its value.
         */
        class MontgomeryContext
        {
        public:
            /// modulus must be odd and above 1 (CryptoFatal otherwise).
            explicit MontgomeryContext(const BigInt& modulus);
            /// The modulus may be an RSA prime: the limbs are erased.
            ~MontgomeryContext();
            MontgomeryContext(const MontgomeryContext&) = default;
            MontgomeryContext& operator=(const MontgomeryContext&) = default;

            size_t Limbs() const { return m_k; }
            const Limb* Modulus() const { return m_m.data(); }
            Limb N0Inv() const { return m_n0inv; }
            /// R mod m -- the Montgomery form of 1.
            const Limb* One() const { return m_one.data(); }
            /// R^2 mod m -- the constant that takes a value into Montgomery form.
            const Limb* R2() const { return m_r2.data(); }
            BigInt ModulusValue() const { return BigInt::FromLimbs(m_m.data(), m_k); }

            /// z = x * y * R^-1 mod m for canonical x, y. z may alias x or y.
            void Mul(Limb* z, const Limb* x, const Limb* y) const;
            void Sqr(Limb* z, const Limb* x) const { Mul(z, x, x); }

            /// z = x * R mod m. x is reduced modulo m first (a plain division), so any
            /// value may enter here -- this is where an SRP6 product or an RSA message
            /// wider than the modulus gets reduced.
            void ToMont(Limb* z, const BigInt& x) const;
            /// The value of a Montgomery-form number.
            BigInt FromMont(const Limb* x) const;

        private:
            size_t m_k;
            std::vector<Limb> m_m;
            std::vector<Limb> m_r2;
            std::vector<Limb> m_one;
            Limb m_n0inv;
            MontMulFn m_kernel;   // the tier, taken once at construction
        };
    }
}

#endif
