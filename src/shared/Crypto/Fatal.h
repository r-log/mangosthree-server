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

#ifndef MANGOS_H_CRYPTO_FATAL
#define MANGOS_H_CRYPTO_FATAL

namespace MaNGOS
{
    namespace Crypto
    {
        /**
         * @brief The one way a primitive fails: loudly, and not by returning a weak result.
         *
         * A random source that cannot deliver, an unsigned subtraction that would go
         * negative, a Montgomery context over an even modulus -- none of these has a
         * value the caller could sensibly continue with. The default handler prints the
         * reason and aborts. Tests install a handler that throws, so the failure paths
         * are exercised without ending the process.
         */
        [[noreturn]] void CryptoFatal(const char* what);

        using FatalHandler = void (*)(const char* what);

        /// Installs a handler; returns the previous one. The handler must not return
        /// (throwing is fine). Passing nullptr restores the default.
        FatalHandler SetFatalHandler(FatalHandler handler);
    }
}

#endif
