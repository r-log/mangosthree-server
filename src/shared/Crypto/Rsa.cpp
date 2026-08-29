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

#include "Crypto/Rsa.h"
#include "Crypto/ModExp.h"
#include "Crypto/SecureZero.h"

namespace MaNGOS
{
    namespace Crypto
    {
        bool RsaPublicKey::Load(const BigInt& n, const BigInt& e)
        {
            m_context.reset();
            if (!n.IsOdd() || n.LimbCount() < 2 || !e.IsOdd() || e <= BigInt(1) || e >= n)
            {
                return false;
            }
            m_n = n;
            m_e = e;
            m_context.emplace(n);
            return true;
        }

        RsaPrivateKey::~RsaPrivateKey()
        {
            Unload();
        }

        void RsaPrivateKey::Unload()
        {
            {
                std::lock_guard<std::mutex> guard(m_blindingLock);
                m_blindingFactor.SecureClear();
                m_blindingInverse.SecureClear();
                m_blindingUses = 0;
            }
            m_public = RsaPublicKey();
            m_d.SecureClear();
            m_crt = false;
            m_p.SecureClear();
            m_q.SecureClear();
            m_dP.SecureClear();
            m_dQ.SecureClear();
            m_qInv.SecureClear();
            m_pContext.reset();
            m_qContext.reset();
            if (!m_qInvMont.empty())
            {
                SecureZero(m_qInvMont.data(), m_qInvMont.size() * LimbBytes);
                m_qInvMont.clear();
            }
        }

        bool RsaPrivateKey::Load(const BigInt& n, const BigInt& e, const BigInt& d)
        {
            // Validate first, commit last: a Load that returns false leaves no key.
            Unload();
            RsaPublicKey publicKey;
            if (!publicKey.Load(n, e) || d.IsZero() || d >= n)
            {
                return false;
            }
            m_public = publicKey;
            m_d = d;
            return true;
        }

        bool RsaPrivateKey::Load(const BigInt& n, const BigInt& e, const BigInt& d,
                                 const BigInt& p, const BigInt& q, const BigInt& dP, const BigInt& dQ, const BigInt& qInv)
        {
            Unload();
            RsaPublicKey publicKey;
            if (!publicKey.Load(n, e) || d.IsZero() || d >= n)
            {
                return false;
            }
            // Structural checks: odd primes of at least two limbs, n = p * q, p > q (so
            // the recombination's m2 < q is already below p), the CRT exponents below
            // their primes, qInv the inverse of q modulo p.
            if (!p.IsOdd() || !q.IsOdd() || p.LimbCount() < 2 || q.LimbCount() < 2 || p <= q || p * q != n)
            {
                return false;
            }
            if (dP.IsZero() || dP >= p || dQ.IsZero() || dQ >= q || qInv.IsZero() || qInv >= p)
            {
                return false;
            }
            if ((q * qInv) % p != BigInt(1))
            {
                return false;
            }
            // Everything checked: build the contexts, then commit.
            MontgomeryContext pContext(p), qContext(q);
            std::vector<Limb> qInvMont(pContext.Limbs(), 0);
            pContext.ToMont(qInvMont.data(), qInv);

            m_public = publicKey;
            m_d = d;
            m_p = p;
            m_q = q;
            m_dP = dP;
            m_dQ = dQ;
            m_qInv = qInv;
            m_pContext.emplace(pContext);
            m_qContext.emplace(qContext);
            m_qInvMont.swap(qInvMont);
            m_crt = true;
            return true;
        }

        BigInt RsaPrivateKey::CrtOperation(const BigInt& m) const
        {
            const size_t kp = m_pContext->Limbs();
            // m1 = m^dP mod p, m2 = m^dQ mod q -- ModExp reduces m into each context.
            BigInt m1 = ModExp(m, m_dP, *m_pContext);
            BigInt m2 = ModExp(m, m_dQ, *m_qContext);

            // h = qInv * (m1 - m2) mod p, in modular arithmetic on kp limbs and with
            // masks, never an unsigned subtraction. Load guarantees p > q, so m2 < q < p
            // is already canonical modulo p and fits kp limbs.
            std::vector<Limb> a(kp, 0), b(kp, 0), t(kp, 0), tmp(kp, 0);
            for (size_t j = 0; j < m1.LimbCount(); ++j) a[j] = m1.Limbs()[j];
            for (size_t j = 0; j < m2.LimbCount(); ++j) b[j] = m2.Limbs()[j];
            const Limb* p = m_pContext->Modulus();
            {
                // t = a - b mod p: both a - b and a + p - b, selected by the borrow.
                Limb borrow = 0;
                for (size_t j = 0; j < kp; ++j) t[j] = SubBorrow(a[j], b[j], borrow);
                Limb carry = 0;
                for (size_t j = 0; j < kp; ++j) tmp[j] = AddCarry(t[j], p[j], carry);
                const Limb wentNegative = CtMask(borrow != 0);
                for (size_t j = 0; j < kp; ++j) t[j] = CtSelect(wentNegative, tmp[j], t[j]);
            }
            // h = t * qInv mod p: qInv is held in Montgomery form, so one product does it.
            std::vector<Limb> h(kp, 0);
            m_pContext->Mul(h.data(), t.data(), m_qInvMont.data());
            const BigInt hValue = BigInt::FromLimbs(h.data(), kp);

            // s = m2 + h * q
            BigInt s = m2 + hValue * m_q;

            SecureZero(a.data(), a.size() * LimbBytes);
            SecureZero(b.data(), b.size() * LimbBytes);
            SecureZero(t.data(), t.size() * LimbBytes);
            SecureZero(tmp.data(), tmp.size() * LimbBytes);
            SecureZero(h.data(), h.size() * LimbBytes);
            m1.SecureClear();
            m2.SecureClear();
            return s;
        }

        BigInt RsaPrivateKey::PrivateOperation(const BigInt& m) const
        {
            if (m_crt)
            {
                return CrtOperation(m);
            }
            return ModExp(m, m_d, m_public.Context());
        }

        void RsaPrivateKey::NextBlinding(BigInt& factor, BigInt& inverse, SystemRandom& random) const
        {
            // Called with the lock held. Every 32 uses: a fresh r, invertible, with
            // r^e and r^-1. Otherwise: square both -- they stay a matching pair for r^2.
            const BigInt& n = m_public.Modulus();
            if (m_blindingUses == 0 || m_blindingUses >= BlindingRefreshInterval || m_blindingInverse.IsZero())
            {
                for (;;)
                {
                    BigInt r = random.Below(n);
                    if (r < BigInt(2))
                    {
                        continue;
                    }
                    BigInt rInverse = ModInverse(r, n);
                    if (rInverse.IsZero())
                    {
                        continue;
                    }
                    m_blindingFactor = ModExp(r, m_public.Exponent(), m_public.Context(), ExponentKind::Public);
                    m_blindingInverse = rInverse;
                    r.SecureClear();
                    break;
                }
                m_blindingUses = 0;
            }
            else
            {
                m_blindingFactor = (m_blindingFactor * m_blindingFactor) % n;
                m_blindingInverse = (m_blindingInverse * m_blindingInverse) % n;
            }
            ++m_blindingUses;
            factor = m_blindingFactor;
            inverse = m_blindingInverse;
        }

        bool RsaPrivateKey::SignRaw(const uint8_t* message, size_t length, std::vector<uint8_t>& signature, SystemRandom& random) const
        {
            if (!Loaded() || length != Bytes())
            {
                return false;
            }
            const BigInt& n = m_public.Modulus();
            BigInt m = BigInt::FromBytesBE(message, length);
            if (m >= n)
            {
                return false;
            }

            // Blinding: m' = m * r^e, s = (m')^d * r^-1 = m^d.
            BigInt factor, inverse;
            {
                std::lock_guard<std::mutex> guard(m_blindingLock);
                NextBlinding(factor, inverse, random);
            }
            BigInt blinded = (m * factor) % n;
            BigInt s = (PrivateOperation(blinded) * inverse) % n;
            signature = s.ToBytesBE(length);

            factor.SecureClear();
            inverse.SecureClear();
            blinded.SecureClear();
            s.SecureClear();
            m.SecureClear();
            return !signature.empty();
        }

        bool RsaVerifyRaw(const RsaPublicKey& key, const uint8_t* signature, size_t length, std::vector<uint8_t>& message)
        {
            if (!key.Loaded() || length != key.Bytes())
            {
                return false;
            }
            const BigInt s = BigInt::FromBytesBE(signature, length);
            if (s >= key.Modulus())
            {
                return false;
            }
            const BigInt m = ModExp(s, key.Exponent(), key.Context(), ExponentKind::Public);
            message = m.ToBytesBE(length);
            return !message.empty();
        }
    }
}
