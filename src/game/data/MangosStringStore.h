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

#ifndef MANGOS_H_MANGOSSTRINGSTORE
#define MANGOS_H_MANGOSSTRINGSTORE

#include "Platform/Define.h"
#include "SharedDefines.h"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

struct MangosStringLocale
{
    MangosStringLocale() : SoundId(0), Type(0), LanguageId(LANG_UNIVERSAL), Emote(0) { }

    std::vector<std::string> Content;                       // 0 -> default, i -> i-1 locale index
    uint32 SoundId;
    uint8  Type;
    Language LanguageId;
    uint32 Emote;
};

/**
 * The server's own strings and the script texts (`mangos_string` and the script libraries' text
 * tables), one per entry, each its text in every locale and, for a script text, its sound, chat type,
 * language and emote; and how many strings each table's load read, by the lowest entry of its range.
 * A table's load erases its range, then fills each entry in place through `FindOrAdd`.
 */
class MangosStringStore
{
    public:
        void EraseRange(int32 startEntry, int32 endEntry);
        MangosStringLocale& FindOrAdd(int32 entry) { return m_strings[entry]; }
        MangosStringLocale const* Find(int32 entry) const;

        void SetLoadedCount(int32 minEntry, uint32 count) { m_loadedCounts[minEntry] = count; }
        uint32 LoadedCount(int32 minEntry) const;

    private:
        std::unordered_map<int32, MangosStringLocale> m_strings;
        std::map<int32, uint32> m_loadedCounts;
};

#endif
