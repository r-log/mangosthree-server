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

#ifndef _AUTH_SHA1_H
#define _AUTH_SHA1_H

#include "Platform/Define.h"
#include "Crypto/Sha1.h"

#include <string>

class BigNumber;

/// The SHA-1 digest width, under the name the protocol code has always used for it.
constexpr int SHA_DIGEST_LENGTH = 20;

/**
 * @brief SHA-1 with the interface the authentication code was written against, over
 * the tree's own core (src/shared/Crypto).
 *
 * Used for the SRP6 quantities, account password hashes and the world session proof.
 * Value semantics are load-bearing: realmd passes a finalized Sha1Hash to
 * AuthSocket::SendProof() by value, and a copy carries both the running state and
 * the digest.
 */
class Sha1Hash
{
    public:
        Sha1Hash() = default;
        /// The digest may be session key material; it does not outlive the object.
        ~Sha1Hash();
        Sha1Hash(const Sha1Hash& other) = default;
        Sha1Hash& operator=(const Sha1Hash& other) = default;

        /**
         * @brief Update the hash with the minimal little-endian bytes of each number
         * @param bn0 First BigNumber; the list is NULL terminated
         */
        void UpdateBigNumbers(BigNumber* bn0, ...);

        void UpdateData(const uint8* dta, int len);
        void UpdateData(const std::string& str);

        /// Reset, ready to hash a fresh message.
        void Initialize();
        /// Produce the digest; the running state is reset afterwards.
        void Finalize();

        /// The digest. Only meaningful after Finalize(); SHA_DIGEST_LENGTH bytes.
        uint8* GetDigest(void) { return mDigest; }
        int GetLength(void) { return SHA_DIGEST_LENGTH; }

    private:
        MaNGOS::Crypto::Sha1Core mC;              /**< The running state */
        uint8 mDigest[SHA_DIGEST_LENGTH]{ 0 };    /**< The computed digest */
};
#endif
