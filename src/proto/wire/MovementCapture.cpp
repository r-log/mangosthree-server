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

#include "wire/MovementCapture.h"

#include <cstdio>
#include <mutex>

namespace Wire
{
    namespace
    {
        std::mutex g_lock;
        FILE* g_file = nullptr;
    }

    bool MovementCapture::Open(std::string const& path)
    {
        std::lock_guard<std::mutex> guard(g_lock);
        if (g_file)
        {
            std::fclose(g_file);
            g_file = nullptr;
        }
        g_file = std::fopen(path.c_str(), "ab");
        return g_file != nullptr;
    }

    bool MovementCapture::IsOpen()
    {
        std::lock_guard<std::mutex> guard(g_lock);
        return g_file != nullptr;
    }

    void MovementCapture::Record(char direction, uint16 opcode, uint8 const* bytes, size_t size)
    {
        static const char digits[] = "0123456789ABCDEF";

        std::lock_guard<std::mutex> guard(g_lock);
        if (!g_file)
        {
            return;
        }

        std::fprintf(g_file, "%c 0x%04X ", direction, unsigned(opcode));
        for (size_t i = 0; i < size; ++i)
        {
            std::fputc(digits[bytes[i] >> 4], g_file);
            std::fputc(digits[bytes[i] & 0x0F], g_file);
        }
        std::fputc('\n', g_file);
        std::fflush(g_file);
    }

    void MovementCapture::Close()
    {
        std::lock_guard<std::mutex> guard(g_lock);
        if (g_file)
        {
            std::fclose(g_file);
            g_file = nullptr;
        }
    }
}
