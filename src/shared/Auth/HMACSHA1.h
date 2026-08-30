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

#ifndef _AUTH_HMACSHA1_H
#define _AUTH_HMACSHA1_H

#include "Platform/Define.h"
#include "Auth/Sha1.h"
#include "Crypto/HmacSha1.h"

#include <string>

class BigNumber;

#define SEED_KEY_SIZE 16

/**
 * @brief HMAC-SHA1 with the interface the packet cipher and the redirect were written
 * against, over the tree's own implementation (src/shared/Crypto).
 *
 * Derives the two packet-cipher keys from the session key under fixed 16-byte seeds,
 * and tags the redirect. Keyed at construction; the key material is erased when the
 * object goes.
 */
class HMACSHA1
{
    public:
        /**
         * @brief A MAC keyed with `len` bytes of `seed`
         */
        HMACSHA1(uint32 len, uint8 *seed);
        ~HMACSHA1();

        // The keyed state is owned; a copy would be a second holder of the key.
        HMACSHA1(const HMACSHA1&) = delete;
        HMACSHA1& operator=(const HMACSHA1&) = delete;

        /// Update with the minimal little-endian bytes of the number
        void UpdateBigNumber(BigNumber *bn);
        void UpdateData(const uint8 *data, int length);
        void UpdateData(const std::string &str);

        /// Produce the tag; the MAC is re-keyed afterwards, ready for another message.
        void Finalize();

        /// UpdateBigNumber, Finalize and return the tag in one call.
        uint8 *ComputeHash(BigNumber *bn);

        /// The tag. Only meaningful after Finalize(); SHA_DIGEST_LENGTH bytes.
        uint8 *GetDigest() { return m_digest; }
        int GetLength() { return SHA_DIGEST_LENGTH; }

    private:
        MaNGOS::Crypto::HmacSha1 m_mac;      /**< The keyed state */
        uint8 m_digest[SHA_DIGEST_LENGTH];   /**< The computed tag */
};
#endif
