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

#ifndef MANGOS_H_REALMD_SRP6
#define MANGOS_H_REALMD_SRP6

#include "Platform/Define.h"
#include "Auth/BigNumber.h"

#include <string>

/**
 * @brief The SRP6 arithmetic of the login handshake, as the 4.3.4 client computes it.
 *
 * Pure functions over BigNumber and SHA-1: no socket, no database, no state. Every
 * quantity is hashed at the width the protocol fixes for it, never at the width a
 * value happens to have -- the client hashes the same bytes it sent and received,
 * and those are always full width.
 *
 * Kept separate from AuthSocket so the calculation the server runs is the one the
 * tests run: the golden vectors in realmd's test suite go through these functions.
 */
namespace Srp6
{
    constexpr int EphemeralWidth  = 32;   ///< N, A, B
    constexpr int SessionKeyWidth = 40;   ///< K
    constexpr int GeneratorWidth  = 1;    ///< g
    constexpr int ReconnectWidth  = 16;   ///< R1 and the server's reconnect challenge
    constexpr int SaltWidth       = 32;   ///< s
    constexpr int DigestWidth     = 20;   ///< SHA-1

    /// The 256-bit safe prime the client and server agree on.
    BigNumber Modulus();
    /// The generator, 7.
    BigNumber Generator();

    /**
     * @brief The password verifier stored in the account row.
     *
     * x = H(s | H(USERNAME:PASSWORD)), v = g^x mod N. `passwordHashHex` is the hex
     * text of H(USERNAME:PASSWORD) exactly as the account row stores it; a hash
     * with leading zeros is restored to its full 20 bytes before hashing.
     */
    BigNumber Verifier(const BigNumber& s, const std::string& passwordHashHex);

    /// B = (3v + g^b) mod N for the server's private ephemeral b.
    BigNumber ServerEphemeral(const BigNumber& v, const BigNumber& b);

    /// The client's ephemeral A is rejected when A mod N is zero.
    bool IsAcceptableClientEphemeral(const BigNumber& A);

    /// u = H(A | B), both at ephemeral width.
    BigNumber Scrambler(const BigNumber& A, const BigNumber& B);

    /**
     * @brief The 40-byte session key.
     *
     * S = (A * v^u)^b mod N, then the even and odd bytes of S are hashed separately
     * and the two digests interleaved into K.
     */
    BigNumber SessionKey(const BigNumber& A, const BigNumber& v, const BigNumber& u, const BigNumber& b);

    /**
     * @brief M1 as the client computes it: H((H(N) xor H(g)) | H(login) | s | A | B | K).
     * @param out 20 bytes.
     */
    void ClientProof(const std::string& login, const BigNumber& s, const BigNumber& A,
                     const BigNumber& B, const BigNumber& K, uint8* out);

    /**
     * @brief M2 = H(A | M1 | K), the server's answer to a valid M1.
     * @param clientProof the 20 bytes of M1.
     * @param out 20 bytes.
     */
    void ServerProof(const BigNumber& A, const uint8* clientProof, const BigNumber& K, uint8* out);

    /**
     * @brief The reconnect proof: H(login | R1 | challenge | K).
     * @param clientData the 16 bytes of R1 from the client.
     * @param challenge the 16-byte value the server sent with the reconnect challenge.
     * @param out 20 bytes.
     */
    void ReconnectProof(const std::string& login, const uint8* clientData,
                        const BigNumber& challenge, const BigNumber& K, uint8* out);
}

#endif
