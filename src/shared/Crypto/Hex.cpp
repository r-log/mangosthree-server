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

#include "Crypto/Hex.h"

namespace MaNGOS
{
    namespace Crypto
    {
        namespace
        {
            int Nibble(char c)
            {
                if (c >= '0' && c <= '9') { return c - '0'; }
                if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
                if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
                return -1;
            }

            bool IsSpace(char c)
            {
                return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
            }
        }

        std::string ToHex(const uint8_t* data, size_t length, bool upper)
        {
            const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
            std::string out;
            out.reserve(length * 2);
            for (size_t i = 0; i < length; ++i)
            {
                out.push_back(digits[data[i] >> 4]);
                out.push_back(digits[data[i] & 0x0F]);
            }
            return out;
        }

        bool FromHex(std::string_view text, std::vector<uint8_t>& out)
        {
            std::vector<uint8_t> bytes;
            bytes.reserve(text.size() / 2);
            int pending = -1;
            for (char c : text)
            {
                if (IsSpace(c))
                {
                    continue;
                }
                const int nibble = Nibble(c);
                if (nibble < 0)
                {
                    return false;
                }
                if (pending < 0)
                {
                    pending = nibble;
                }
                else
                {
                    bytes.push_back(static_cast<uint8_t>((pending << 4) | nibble));
                    pending = -1;
                }
            }
            if (pending >= 0)
            {
                return false;   // an odd number of digits
            }
            out.swap(bytes);
            return true;
        }
    }
}
