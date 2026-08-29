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

#ifndef MANGOS_H_CRYPTO_RSA
#define MANGOS_H_CRYPTO_RSA

#include "Crypto/BigInt.h"
#include "Crypto/Montgomery.h"
#include "Crypto/SystemRandom.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace MaNGOS
{
    namespace Crypto
    {
        /**
         * @brief Raw RSA -- the operation the client redirect is built on.
         *
         * No padding scheme: the caller signs a block it constructed to be below the
         * modulus (the redirect's permuted 258-byte block with its HMAC tag) and the
         * client recovers it with the public exponent. Byte strings are big-endian at
         * the modulus width.
         */
        class RsaPublicKey
        {
        public:
            /// n odd and at least two limbs wide, e odd and above 1 (false otherwise).
            bool Load(const BigInt& n, const BigInt& e);
            bool Loaded() const { return m_context.has_value(); }
            /// The width of a signature or a message, in bytes.
            size_t Bytes() const { return m_n.ByteLength(); }
            const BigInt& Modulus() const { return m_n; }
            const BigInt& Exponent() const { return m_e; }
            const MontgomeryContext& Context() const { return *m_context; }

        private:
            BigInt m_n;
            BigInt m_e;
            std::optional<MontgomeryContext> m_context;
        };

        class RsaPrivateKey
        {
        public:
            RsaPrivateKey() = default;
            ~RsaPrivateKey();
            RsaPrivateKey(const RsaPrivateKey&) = delete;
            RsaPrivateKey& operator=(const RsaPrivateKey&) = delete;

            /// The pair as the current key files carry it: n, e, d. Signatures go
            /// through the full 2048-bit exponentiation.
            bool Load(const BigInt& n, const BigInt& e, const BigInt& d);

            /// With the primes: n = p * q with p > q, dP = d mod (p - 1), dQ = d mod (q - 1),
            /// qInv = q^-1 mod p (PKCS#1 names). Signatures use the Chinese remainder
            /// theorem -- two exponentiations of half the width, about four times faster.
            /// The structural relations are checked here; a pair that is not a pair is
            /// caught by the caller's signed probe. Either Load commits only when every
            /// check passed; on failure nothing is loaded.
            bool Load(const BigInt& n, const BigInt& e, const BigInt& d,
                      const BigInt& p, const BigInt& q, const BigInt& dP, const BigInt& dQ, const BigInt& qInv);

            /// Erase everything; Loaded() is false afterwards.
            void Unload();

            bool Loaded() const { return m_public.Loaded(); }
            bool HasCrt() const { return m_crt; }
            size_t Bytes() const { return m_public.Bytes(); }
            const RsaPublicKey& Public() const { return m_public; }

            /**
             * @brief signature = message^d mod n, big-endian at the modulus width.
             *
             * `length` must equal Bytes() and the message must be below n (false
             * otherwise). Blinded: a random factor masks the message before the private
             * operation and is removed after it, so the exponentiation's timing is
             * decorrelated from the message; the exponentiation itself is the
             * constant-time ModExp. The blinding pair (r^e, r^-1) is kept and squared
             * after every use, and drawn afresh every 32 signatures -- the scheme of
             * OpenSSL's BN_BLINDING; an inversion per signature would cost as much as a
             * CRT half. The result is not verified here -- the caller does that with the
             * public key, which also catches a faulty CRT computation. Thread-safe.
             */
            bool SignRaw(const uint8_t* message, size_t length, std::vector<uint8_t>& signature, SystemRandom& random) const;

            /// How many signatures a blinding pair serves before it is drawn afresh.
            static constexpr unsigned BlindingRefreshInterval = 32;

        private:
            BigInt PrivateOperation(const BigInt& m) const;
            BigInt CrtOperation(const BigInt& m) const;
            void NextBlinding(BigInt& factor, BigInt& inverse, SystemRandom& random) const;

            RsaPublicKey m_public;
            BigInt m_d;
            bool m_crt = false;
            BigInt m_p, m_q, m_dP, m_dQ, m_qInv;
            std::optional<MontgomeryContext> m_pContext, m_qContext;
            std::vector<Limb> m_qInvMont;   // qInv in Montgomery form modulo p

            mutable std::mutex m_blindingLock;
            mutable BigInt m_blindingFactor;    // r^e mod n
            mutable BigInt m_blindingInverse;   // r^-1 mod n
            mutable unsigned m_blindingUses = 0;
        };

        /// message = signature^e mod n, big-endian at the modulus width. `length` must
        /// equal the key's width and the signature must be below n (false otherwise).
        bool RsaVerifyRaw(const RsaPublicKey& key, const uint8_t* signature, size_t length, std::vector<uint8_t>& message);

        /// A generated pair with its CRT parameters (PKCS#1 names), p > q.
        struct RsaKeyPair
        {
            BigInt n, e, d, p, q, dP, dQ, qInv;
            ~RsaKeyPair();
        };

        /**
         * @brief Generate an RSA pair -- FIPS 186-4 B.3.3 conditions, run offline.
         *
         * p and q are `bits / 2`-bit primes from GeneratePrime (the sqrt(2) interval,
         * so n has exactly `bits` bits; gcd(p - 1, e) = gcd(q - 1, e) = 1), with
         * |p - q| > 2^(bits/2 - 100) and p > q; d = e^-1 mod lcm(p - 1, q - 1) with
         * d > 2^(bits/2), otherwise both primes are drawn again; dP, dQ, qInv follow.
         * The pair is loaded into an RsaPrivateKey and a probe is signed and verified
         * before it is handed back. `bits` must be a multiple of 512 from 1024 up; e
         * must be odd and above 2 (65537 is what the client expects).
         */
        bool RsaGenerateKey(size_t bits, const BigInt& e, SystemRandom& random, RsaKeyPair& out, unsigned millerRabinRounds = 64);
    }
}

#endif
