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

#ifndef MANGOS_H_CRYPTO_SYSTEM_RANDOM
#define MANGOS_H_CRYPTO_SYSTEM_RANDOM

#include "Crypto/BigInt.h"

#include <cstddef>
#include <cstdint>

namespace MaNGOS
{
    namespace Crypto
    {
        /**
         * @brief The operating system's CSPRNG, and nothing else.
         *
         * `BCryptGenRandom` on Windows, `getrandom(2)` on Linux (`/dev/urandom` only when
         * the syscall does not exist), `arc4random_buf` on the BSDs and macOS. FillExact
         * delivers every byte asked for or calls CryptoFatal -- there is no weak fallback
         * and no short result. The backend is a function pointer so the retry, EOF and
         * error paths are testable without an operating system that misbehaves.
         */
        class SystemRandom
        {
        public:
            /// Fills up to `want` bytes; `got` may be short. Returns false on a hard error.
            /// A backend retries its own EINTR; a true return with got == 0 is end of data.
            using Backend = bool (*)(uint8_t* out, size_t want, size_t& got);

            SystemRandom();
            explicit SystemRandom(Backend backend);

            /// The process-wide instance on the OS backend.
            static SystemRandom& Instance();

            /// Exactly `length` bytes, or CryptoFatal.
            void FillExact(uint8_t* out, size_t length);

            /// A value of exactly `bits` bits: the top bit set and the value odd -- the
            /// shape BN_rand(top = 0, bottom = 1) produced for the SRP6 ephemerals and
            /// salts. bits must be at least 2.
            BigInt Bits(size_t bits);

            /// Uniform in [0, n) by rejection sampling. n must be non-zero.
            BigInt Below(const BigInt& n);

        private:
            Backend m_backend;
        };
    }
}

#endif
