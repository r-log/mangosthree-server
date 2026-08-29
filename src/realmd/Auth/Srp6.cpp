/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Auth/Srp6.h"
#include "Auth/Sha1.h"
#include "Utilities/Errors.h"

#include <algorithm>
#include <cstring>

namespace
{
    /// Hash a number at a protocol width, never at the width it happens to have.
    void UpdateFixed(Sha1Hash& sha, const BigNumber& value, int width)
    {
        BigNumber copy(value);   // AsByteArray is not const: it owns the buffer it returns
        sha.UpdateData(copy.AsByteArray(width), width);
    }
}

namespace Srp6
{
    BigNumber Modulus()
    {
        BigNumber N;
        N.SetHexStr("894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7");
        return N;
    }

    BigNumber Generator()
    {
        BigNumber g;
        g.SetDword(7);
        return g;
    }

    BigNumber Verifier(const BigNumber& s, const std::string& passwordHashHex)
    {
        BigNumber I;
        I.SetHexStr(passwordHashHex.c_str());

        // In case of leading zeros in the stored hash, restore them: asking for the full
        // digest width pads at the front, where the minimal encoding would have lost them.
        uint8 digest[DigestWidth];
        std::memset(digest, 0, DigestWidth);
        if (I.GetNumBytes() <= DigestWidth)
        {
            std::memcpy(digest, I.AsByteArray(DigestWidth), DigestWidth);
        }
        std::reverse(digest, digest + DigestWidth);

        // x = H(s | H(USER:PASS)) over the whole salt. A short salt here is not a failed
        // login but a verifier stored against a byte string the client will never
        // produce, so that account is broken for good rather than until the next attempt.
        Sha1Hash sha;
        UpdateFixed(sha, s, SaltWidth);
        sha.UpdateData(digest, DigestWidth);
        sha.Finalize();

        BigNumber x;
        x.SetBinary(sha.GetDigest(), DigestWidth);
        return Generator().ModExp(x, Modulus());
    }

    BigNumber ServerEphemeral(const BigNumber& v, const BigNumber& b)
    {
        BigNumber N = Modulus();
        BigNumber gmod = Generator().ModExp(b, N);
        MANGOS_ASSERT(gmod.GetNumBytes() <= EphemeralWidth);
        BigNumber threeV = BigNumber(v) * BigNumber(3);
        return (threeV + gmod) % N;
    }

    bool IsAcceptableClientEphemeral(const BigNumber& A)
    {
        BigNumber copy(A);
        return !(copy % Modulus()).isZero();
    }

    BigNumber Scrambler(const BigNumber& A, const BigNumber& B)
    {
        Sha1Hash sha;
        UpdateFixed(sha, A, EphemeralWidth);
        UpdateFixed(sha, B, EphemeralWidth);
        sha.Finalize();

        BigNumber u;
        u.SetBinary(sha.GetDigest(), DigestWidth);
        return u;
    }

    BigNumber SessionKey(const BigNumber& A, const BigNumber& v, const BigNumber& u, const BigNumber& b)
    {
        BigNumber N = Modulus();
        BigNumber vCopy(v);
        BigNumber S = (BigNumber(A) * vCopy.ModExp(u, N)).ModExp(b, N);

        uint8 t[EphemeralWidth];
        uint8 half[EphemeralWidth / 2];
        uint8 vK[SessionKeyWidth];
        std::memcpy(t, S.AsByteArray(EphemeralWidth), EphemeralWidth);

        Sha1Hash sha;
        for (int i = 0; i < EphemeralWidth / 2; ++i)
        {
            half[i] = t[i * 2];
        }
        sha.UpdateData(half, EphemeralWidth / 2);
        sha.Finalize();
        for (int i = 0; i < DigestWidth; ++i)
        {
            vK[i * 2] = sha.GetDigest()[i];
        }

        for (int i = 0; i < EphemeralWidth / 2; ++i)
        {
            half[i] = t[i * 2 + 1];
        }
        sha.Initialize();
        sha.UpdateData(half, EphemeralWidth / 2);
        sha.Finalize();
        for (int i = 0; i < DigestWidth; ++i)
        {
            vK[i * 2 + 1] = sha.GetDigest()[i];
        }

        BigNumber K;
        K.SetBinary(vK, SessionKeyWidth);
        return K;
    }

    void ClientProof(const std::string& login, const BigNumber& s, const BigNumber& A,
                     const BigNumber& B, const BigNumber& K, uint8* out)
    {
        // H(N) xor H(g)
        uint8 hash[DigestWidth];
        Sha1Hash sha;
        UpdateFixed(sha, Modulus(), EphemeralWidth);
        sha.Finalize();
        std::memcpy(hash, sha.GetDigest(), DigestWidth);

        sha.Initialize();
        UpdateFixed(sha, Generator(), GeneratorWidth);
        sha.Finalize();
        for (int i = 0; i < DigestWidth; ++i)
        {
            hash[i] ^= sha.GetDigest()[i];
        }

        // H(login)
        uint8 loginHash[DigestWidth];
        sha.Initialize();
        sha.UpdateData(login);
        sha.Finalize();
        std::memcpy(loginHash, sha.GetDigest(), DigestWidth);

        // M1 = H(H(N) xor H(g) | H(login) | s | A | B | K)
        sha.Initialize();
        sha.UpdateData(hash, DigestWidth);
        sha.UpdateData(loginHash, DigestWidth);
        UpdateFixed(sha, s, SaltWidth);
        UpdateFixed(sha, A, EphemeralWidth);
        UpdateFixed(sha, B, EphemeralWidth);
        UpdateFixed(sha, K, SessionKeyWidth);
        sha.Finalize();
        std::memcpy(out, sha.GetDigest(), DigestWidth);
    }

    void ServerProof(const BigNumber& A, const uint8* clientProof, const BigNumber& K, uint8* out)
    {
        Sha1Hash sha;
        UpdateFixed(sha, A, EphemeralWidth);
        sha.UpdateData(clientProof, DigestWidth);
        UpdateFixed(sha, K, SessionKeyWidth);
        sha.Finalize();
        std::memcpy(out, sha.GetDigest(), DigestWidth);
    }

    void ReconnectProof(const std::string& login, const uint8* clientData,
                        const BigNumber& challenge, const BigNumber& K, uint8* out)
    {
        Sha1Hash sha;
        sha.UpdateData(login);
        sha.UpdateData(clientData, ReconnectWidth);
        UpdateFixed(sha, challenge, ReconnectWidth);
        UpdateFixed(sha, K, SessionKeyWidth);
        sha.Finalize();
        std::memcpy(out, sha.GetDigest(), DigestWidth);
    }
}
