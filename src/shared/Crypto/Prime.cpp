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

#include "Crypto/Prime.h"
#include "Crypto/Fatal.h"
#include "Crypto/ModExp.h"
#include "Crypto/Montgomery.h"

#include <vector>

namespace MaNGOS
{
    namespace Crypto
    {
        namespace
        {
            /// Every prime below 2^16, by Eratosthenes, built on first use.
            const std::vector<uint32_t>& SmallPrimes()
            {
                static const std::vector<uint32_t> primes = []
                {
                    const uint32_t limit = 65536;
                    std::vector<bool> composite(limit, false);
                    std::vector<uint32_t> out;
                    for (uint32_t i = 2; i < limit; ++i)
                    {
                        if (composite[i])
                        {
                            continue;
                        }
                        out.push_back(i);
                        for (uint32_t j = i * i; j < limit; j += i)
                        {
                            composite[j] = true;
                        }
                    }
                    return out;
                }();
                return primes;
            }
        }

        BigInt Gcd(const BigInt& a, const BigInt& b)
        {
            BigInt x = a, y = b;
            while (!y.IsZero())
            {
                BigInt r = x % y;
                x = y;
                y = r;
            }
            return x;
        }

        bool PassesSmallPrimeSieve(const BigInt& n)
        {
            if (n.IsZero() || n.IsOne())
            {
                return false;
            }
            for (uint32_t p : SmallPrimes())
            {
                BigInt copy(n);
                if (copy.DivModSmall(p) == 0)
                {
                    return n == BigInt(p);   // the prime itself is not its own factor
                }
            }
            return true;
        }

        bool IsProbablePrime(const BigInt& n, unsigned rounds, SystemRandom& random)
        {
            if (!n.IsOdd() || n <= BigInt(3))
            {
                CryptoFatal("IsProbablePrime: n must be odd and above 3");
            }
            if (rounds == 0)
            {
                CryptoFatal("IsProbablePrime: at least one round -- zero rounds would call everything prime");
            }
            // n - 1 = 2^s * d with d odd
            const BigInt nMinusOne = n - BigInt(1);
            BigInt d = nMinusOne;
            size_t s = 0;
            while (!d.IsOdd())
            {
                d >>= 1;
                ++s;
            }
            MontgomeryContext context(n);
            const BigInt nMinusThree = n - BigInt(3);
            for (unsigned round = 0; round < rounds; ++round)
            {
                const BigInt a = random.Below(nMinusThree) + BigInt(2);   // uniform in [2, n - 2]
                BigInt x = ModExp(a, d, context);
                if (x.IsOne() || x == nMinusOne)
                {
                    continue;
                }
                bool witness = true;
                for (size_t r = 1; r < s; ++r)
                {
                    x = (x * x) % n;
                    if (x == nMinusOne)
                    {
                        witness = false;
                        break;
                    }
                    if (x.IsOne())
                    {
                        break;   // a non-trivial square root of 1: composite
                    }
                }
                if (witness)
                {
                    return false;
                }
            }
            return true;
        }

        BigInt GeneratePrime(size_t bits, const BigInt& e, SystemRandom& random, unsigned rounds)
        {
            if (bits < 64)
            {
                CryptoFatal("GeneratePrime: at least 64 bits");
            }
            if (rounds == 0)
            {
                CryptoFatal("GeneratePrime: at least one Miller-Rabin round");
            }
            const std::vector<uint32_t>& primes = SmallPrimes();
            std::vector<uint32_t> residues(primes.size());
            for (;;)
            {
                // A fresh odd start with the top bit set, in the FIPS interval: p is
                // above sqrt(2) * 2^(bits - 1) exactly when p^2 has 2 * bits bits.
                BigInt candidate = random.Bits(bits);
                if ((candidate * candidate).BitLength() != 2 * bits)
                {
                    continue;
                }
                // residues of the start modulo every small prime; the sieve then steps
                // the candidate by two without another division
                for (size_t i = 0; i < primes.size(); ++i)
                {
                    BigInt copy(candidate);
                    residues[i] = uint32_t(copy.DivModSmall(primes[i]));
                }
                for (uint32_t step = 0; ; step += 2)
                {
                    if (step > 0 && step % 65536 == 0)
                    {
                        break;   // far enough from the start: draw afresh
                    }
                    bool survives = true;
                    for (size_t i = 0; i < primes.size(); ++i)
                    {
                        if ((residues[i] + step) % primes[i] == 0)
                        {
                            survives = false;
                            break;
                        }
                    }
                    if (!survives)
                    {
                        continue;
                    }
                    BigInt p = candidate + BigInt(step);
                    if (p.BitLength() != bits)
                    {
                        break;   // stepped past 2^bits: draw afresh
                    }
                    if (Gcd(p - BigInt(1), e) != BigInt(1))
                    {
                        continue;
                    }
                    if (IsProbablePrime(p, rounds, random))
                    {
                        return p;
                    }
                }
            }
        }
    }
}
