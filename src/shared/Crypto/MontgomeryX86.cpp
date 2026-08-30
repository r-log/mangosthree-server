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

// The x86-64 tier of the Montgomery kernel: the same CIOS as the portable one, with
// the row products on `mulx` (a flag-free multiply, so the carry chain of the
// additions is never disturbed) and the carries on `adc` through the compiler's
// intrinsics. This is what the intrinsics can express; the dual carry chain of
// mulx/adcx/adox needs the assembly tier. The functions carry a per-function target
// attribute rather than a compile flag, so the translation unit builds with the
// tree's ordinary flags and nothing here can be inlined into a caller that did not
// check CPUID first: the only caller is the dispatch pointer.

#include "Crypto/Montgomery.h"
#include "Crypto/SecureZero.h"

#if defined(__x86_64__) || defined(_M_X64)
#define MANGOS_CRYPTO_X86_64 1
#include <immintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

namespace MaNGOS
{
    namespace Crypto
    {
#if defined(MANGOS_CRYPTO_X86_64)

#if defined(__GNUC__) || defined(__clang__)
#define MANGOS_TARGET_BMI2 __attribute__((target("bmi2")))
#else
#define MANGOS_TARGET_BMI2
#endif

        namespace
        {
            /// EBX of CPUID leaf 7: bit 8 BMI2 (mulx), bit 19 ADX (adcx/adox).
            unsigned Leaf7Ebx()
            {
#if defined(_MSC_VER)
                int regs[4] = { 0, 0, 0, 0 };
                __cpuid(regs, 0);
                if (regs[0] < 7)
                {
                    return 0;
                }
                __cpuidex(regs, 7, 0);
                return unsigned(regs[1]);
#else
                unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
                if (__get_cpuid_max(0, nullptr) < 7)
                {
                    return 0;
                }
                __cpuid_count(7, 0, eax, ebx, ecx, edx);
                return ebx;
#endif
            }
        }

        bool HasMulxTier()
        {
            static const bool available = (Leaf7Ebx() & (1u << 8)) != 0;
            return available;
        }

#if defined(MANGOS_CRYPTO_HAVE_ASM)
        extern "C" void mangos_montmul_adx(uint64_t* z, const uint64_t* x, const uint64_t* y,
                                           const uint64_t* m, uint64_t n0inv, uint64_t k);
        extern "C" void mangos_montmul16_adx(uint64_t* z, const uint64_t* x, const uint64_t* y,
                                             const uint64_t* m, uint64_t n0inv);
        extern "C" void mangos_montmul32_adx(uint64_t* z, const uint64_t* x, const uint64_t* y,
                                             const uint64_t* m, uint64_t n0inv);
        extern "C" void mangos_montsqr16_adx(uint64_t* z, const uint64_t* x, const uint64_t* m, uint64_t n0inv);
        extern "C" void mangos_montsqr32_adx(uint64_t* z, const uint64_t* x, const uint64_t* m, uint64_t n0inv);

        bool HasAsmTier()
        {
            static const bool available = (Leaf7Ebx() & ((1u << 8) | (1u << 19))) == ((1u << 8) | (1u << 19));
            return available;
        }

        bool AssemblyTierCompiledIn()
        {
            return true;
        }

        // The two widths the server uses have their loops written out; every other
        // multiple of four takes the generic listing. The choice depends on k alone.
        void MontMulAsm(Limb* z, const Limb* x, const Limb* y, const Limb* m, Limb n0inv, size_t k)
        {
            if (k == 16)
            {
                mangos_montmul16_adx(z, x, y, m, n0inv);
            }
            else if (k == 32)
            {
                mangos_montmul32_adx(z, x, y, m, n0inv);
            }
            else
            {
                mangos_montmul_adx(z, x, y, m, n0inv, k);
            }
        }
        void MontSqrAsm(Limb* z, const Limb* x, const Limb* m, Limb n0inv, size_t k)
        {
            if (k == 16)
            {
                mangos_montsqr16_adx(z, x, m, n0inv);
            }
            else if (k == 32)
            {
                mangos_montsqr32_adx(z, x, m, n0inv);
            }
            else
            {
                mangos_montmul_adx(z, x, x, m, n0inv, k);
            }
        }
#else
        bool HasAsmTier()
        {
            return false;
        }

        bool AssemblyTierCompiledIn()
        {
            return false;
        }

        void MontMulAsm(Limb* z, const Limb* x, const Limb* y, const Limb* m, Limb n0inv, size_t k)
        {
            MontMulMulx(z, x, y, m, n0inv, k);
        }
        void MontSqrAsm(Limb* z, const Limb* x, const Limb* m, Limb n0inv, size_t k)
        {
            MontMulMulx(z, x, x, m, n0inv, k);
        }
#endif
        void MontSqrMulx(Limb* z, const Limb* x, const Limb* m, Limb n0inv, size_t k)
        {
            MontMulMulx(z, x, x, m, n0inv, k);
        }

        MANGOS_TARGET_BMI2
        void MontMulMulx(Limb* z, const Limb* x, const Limb* y, const Limb* m, Limb n0inv, size_t k)
        {
            unsigned long long t[MaxMontgomeryLimbs + 2];
            for (size_t j = 0; j <= k + 1; ++j)
            {
                t[j] = 0;
            }

            for (size_t i = 0; i < k; ++i)
            {
                const unsigned long long xi = x[i];
                unsigned long long carry = 0;
                for (size_t j = 0; j < k; ++j)
                {
                    unsigned long long hi;
                    unsigned long long lo = _mulx_u64(xi, y[j], &hi);
                    unsigned char c = _addcarry_u64(0, lo, t[j], &lo);
                    c = _addcarry_u64(c, hi, 0, &hi);
                    (void)c;
                    c = _addcarry_u64(0, lo, carry, &lo);
                    c = _addcarry_u64(c, hi, 0, &hi);
                    (void)c;
                    t[j] = lo;
                    carry = hi;
                }
                {
                    unsigned char c = _addcarry_u64(0, t[k], carry, &t[k]);
                    t[k + 1] = c;
                }

                const unsigned long long u = t[0] * n0inv;

                {
                    unsigned long long hi;
                    unsigned long long lo = _mulx_u64(u, m[0], &hi);
                    unsigned char c = _addcarry_u64(0, lo, t[0], &lo);
                    carry = hi + c;
                }
                for (size_t j = 1; j < k; ++j)
                {
                    unsigned long long hi;
                    unsigned long long lo = _mulx_u64(u, m[j], &hi);
                    unsigned char c = _addcarry_u64(0, lo, t[j], &lo);
                    c = _addcarry_u64(c, hi, 0, &hi);
                    (void)c;
                    c = _addcarry_u64(0, lo, carry, &lo);
                    c = _addcarry_u64(c, hi, 0, &hi);
                    (void)c;
                    t[j - 1] = lo;
                    carry = hi;
                }
                {
                    unsigned char c = _addcarry_u64(0, t[k], carry, &t[k - 1]);
                    t[k] = t[k + 1] + c;
                }
            }

            unsigned long long s[MaxMontgomeryLimbs];
            unsigned char borrow = 0;
            for (size_t j = 0; j < k; ++j)
            {
                borrow = _subborrow_u64(borrow, t[j], m[j], &s[j]);
            }
            const Limb keepDifference = CtMask((t[k] != 0) | (borrow == 0));
            for (size_t j = 0; j < k; ++j)
            {
                z[j] = CtSelect(keepDifference, s[j], t[j]);
            }
            SecureZero(t, sizeof t);
            SecureZero(s, sizeof s);
        }

#else

        bool HasMulxTier()
        {
            return false;
        }

        bool HasAsmTier()
        {
            return false;
        }

        bool AssemblyTierCompiledIn()
        {
            return false;
        }

        void MontMulMulx(Limb* z, const Limb* x, const Limb* y, const Limb* m, Limb n0inv, size_t k)
        {
            MontMulPortable(z, x, y, m, n0inv, k);
        }

        void MontMulAsm(Limb* z, const Limb* x, const Limb* y, const Limb* m, Limb n0inv, size_t k)
        {
            MontMulPortable(z, x, y, m, n0inv, k);
        }
        void MontSqrMulx(Limb* z, const Limb* x, const Limb* m, Limb n0inv, size_t k)
        {
            MontMulPortable(z, x, x, m, n0inv, k);
        }
        void MontSqrAsm(Limb* z, const Limb* x, const Limb* m, Limb n0inv, size_t k)
        {
            MontMulPortable(z, x, x, m, n0inv, k);
        }

#endif
    }
}
