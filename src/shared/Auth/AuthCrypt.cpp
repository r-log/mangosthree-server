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

#include "AuthCrypt.h"
#include "HMACSHA1.h"
#include "Log/Log.h"
#include "BigNumber.h"

/**
 * Initializes the authentication crypt state in an uninitialized state.
 */
AuthCrypt::AuthCrypt() : _clientDecrypt(SHA_DIGEST_LENGTH), _serverEncrypt(SHA_DIGEST_LENGTH)
{
    _initialized = false;
}

AuthCrypt::~AuthCrypt()
{
}

/**
 * The seed pair the client carries in its own image, at 0x140992B00 in
 * Wow-64.exe 15595 -- the encryption key first, the decryption key second, in
 * exactly this order. The client hands that address to its key schedule
 * (sub_1401A78D0) for an ordinary connection, splits it in half, and keys its
 * send cipher from the SECOND half and its receive cipher from the first. Our
 * two directions are therefore the mirror: we encrypt with the first half and
 * decrypt with the second, which is what these two names have always meant.
 *
 * A redirected connection passes the same routine a different 32 bytes -- the
 * ones the server put in that connection's SMSG_AUTH_CHALLENGE -- so the split
 * is a parameter here rather than a constant.
 */
static const uint8 DEFAULT_SEED[AuthCrypt::SeedLength] =
{
    0xCC, 0x98, 0xAE, 0x04, 0xE8, 0x97, 0xEA, 0xCA, 0x12, 0xDD, 0xC0, 0x93, 0x42, 0x91, 0x53, 0x57,
    0xC2, 0xB3, 0x72, 0x3C, 0xC6, 0xAE, 0xD9, 0xB5, 0x34, 0x3C, 0x53, 0xEE, 0x2F, 0x43, 0x67, 0xCE
};

static_assert(AuthCrypt::SeedLength == 2 * SEED_KEY_SIZE,
              "the seed table is exactly the two HMAC keys, back to back");

void AuthCrypt::Init(BigNumber* K, const uint8* seed)
{
    const uint8* table = seed ? seed : DEFAULT_SEED;

    HMACSHA1 serverEncryptHmac(SEED_KEY_SIZE, (uint8*)table);
    uint8* encryptHash = serverEncryptHmac.ComputeHash(K);

    HMACSHA1 clientDecryptHmac(SEED_KEY_SIZE, (uint8*)(table + SEED_KEY_SIZE));
    uint8* decryptHash = clientDecryptHmac.ComputeHash(K);

    // SARC4 _serverDecrypt(encryptHash);
    _clientDecrypt.Init(decryptHash);
    _serverEncrypt.Init(encryptHash);
    // SARC4 _clientEncrypt(decryptHash);

    uint8 syncBuf[1024];

    memset(syncBuf, 0, 1024);

    _serverEncrypt.UpdateData(1024, syncBuf);
    //_clientEncrypt.UpdateData(1024, syncBuf);

    memset(syncBuf, 0, 1024);

    //_serverDecrypt.UpdateData(1024, syncBuf);
    _clientDecrypt.UpdateData(1024, syncBuf);

    _initialized = true;
}

/**
 * Decrypts the fixed-size encrypted receive header in place.
 */
void AuthCrypt::DecryptRecv(uint8* data, size_t len)
{
    if (!_initialized)
    {
        return;
    }

    _clientDecrypt.UpdateData(len, data);
}

/**
 * Encrypts the fixed-size outgoing packet header in place.
 */
void AuthCrypt::EncryptSend(uint8* data, size_t len)
{
    if (!_initialized)
    {
        return;
    }

    _serverEncrypt.UpdateData(len, data);
}
