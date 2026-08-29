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

#include "Crypto/SystemRandom.h"
#include "Crypto/Fatal.h"
#include "Crypto/SecureZero.h"

#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#elif defined(__linux__)
#include <cerrno>
#include <fcntl.h>
#include <sys/random.h>
#include <unistd.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <cstdlib>
#else
#error "SystemRandom: no CSPRNG backend for this platform"
#endif

namespace MaNGOS
{
    namespace Crypto
    {
        namespace
        {
#if defined(_WIN32)
            bool OsBackend(uint8_t* out, size_t want, size_t& got)
            {
                got = 0;
                // BCryptGenRandom takes a ULONG; deliver in bounded pieces.
                const size_t piece = want > 0x10000000u ? 0x10000000u : want;
                const NTSTATUS status = BCryptGenRandom(nullptr, out, ULONG(piece), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
                if (status != 0)
                {
                    return false;
                }
                got = piece;
                return true;
            }
#elif defined(__linux__)
            int OpenUrandom()
            {
                int fd;
                do
                {
                    fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
                }
                while (fd < 0 && errno == EINTR);
                return fd;
            }

            bool ReadUrandom(uint8_t* out, size_t want, size_t& got)
            {
                got = 0;
                // Opened once, by the first caller, under the language's thread-safe
                // initialisation of a function-local static.
                static const int fd = OpenUrandom();
                if (fd < 0)
                {
                    return false;
                }
                ssize_t n;
                do
                {
                    n = read(fd, out, want);
                }
                while (n < 0 && errno == EINTR);
                if (n < 0)
                {
                    return false;
                }
                got = size_t(n);   // 0 is end of file, which FillExact treats as fatal
                return true;
            }

            bool OsBackend(uint8_t* out, size_t want, size_t& got)
            {
                got = 0;
                ssize_t n;
                do
                {
                    n = getrandom(out, want, 0);
                }
                while (n < 0 && errno == EINTR);
                if (n < 0)
                {
                    if (errno == ENOSYS)
                    {
                        return ReadUrandom(out, want, got);
                    }
                    return false;
                }
                got = size_t(n);
                return true;
            }
#else
            bool OsBackend(uint8_t* out, size_t want, size_t& got)
            {
                arc4random_buf(out, want);
                got = want;
                return true;
            }
#endif
        }

        SystemRandom::SystemRandom() : m_backend(&OsBackend) {}

        SystemRandom::SystemRandom(Backend backend) : m_backend(backend ? backend : &OsBackend) {}

        SystemRandom& SystemRandom::Instance()
        {
            static SystemRandom instance;
            return instance;
        }

        void SystemRandom::FillExact(uint8_t* out, size_t length)
        {
            while (length)
            {
                size_t got = 0;
                if (!m_backend(out, length, got))
                {
                    CryptoFatal("SystemRandom: the operating system's random source failed");
                }
                if (got == 0)
                {
                    CryptoFatal("SystemRandom: the operating system's random source delivered nothing");
                }
                if (got > length)
                {
                    CryptoFatal("SystemRandom: the backend delivered more than asked");
                }
                out += got;
                length -= got;
            }
        }

        BigInt SystemRandom::Bits(size_t bits)
        {
            if (bits < 2)
            {
                CryptoFatal("SystemRandom: Bits needs at least 2 bits");
            }
            std::vector<uint8_t> bytes((bits + 7) / 8);
            FillExact(bytes.data(), bytes.size());
            const unsigned top = unsigned((bits - 1) % 8);
            bytes.back() &= uint8_t((2u << top) - 1);   // clear above the requested width
            bytes.back() |= uint8_t(1u << top);          // the top bit
            bytes.front() |= 1;                          // odd
            BigInt value = BigInt::FromBytesLE(bytes.data(), bytes.size());
            SecureZero(bytes.data(), bytes.size());
            return value;
        }

        BigInt SystemRandom::Below(const BigInt& n)
        {
            if (n.IsZero())
            {
                CryptoFatal("SystemRandom: Below needs a non-zero bound");
            }
            const size_t bits = n.BitLength();
            std::vector<uint8_t> bytes((bits + 7) / 8);
            const unsigned top = unsigned((bits - 1) % 8);
            for (;;)
            {
                FillExact(bytes.data(), bytes.size());
                bytes.back() &= uint8_t((2u << top) - 1);
                BigInt value = BigInt::FromBytesLE(bytes.data(), bytes.size());
                if (value < n)
                {
                    SecureZero(bytes.data(), bytes.size());
                    return value;
                }
            }
        }
    }
}
