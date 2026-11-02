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

#ifndef MANGOS_H_PETLEVELINFOSTORE
#define MANGOS_H_PETLEVELINFOSTORE

#include "Platform/Define.h"
#include "SharedDefines.h"

#include <map>

struct PetLevelInfo
{
    PetLevelInfo() : health(0), mana(0), armor(0)
    {
        for (int i = 0; i < MAX_STATS; ++i) stats[i] = 0;
    }

    uint16 stats[MAX_STATS];
    uint16 health;
    uint16 mana;
    uint16 armor;
};

/**
 * The pet stats per level (`pet_levelstats`): per creature entry, an array of one `PetLevelInfo` per
 * level, level 1 first, which the store owns and deletes. The loader adds an entry's array, all zeros,
 * the first time it names the entry, and fills it in place.
 */
class PetLevelInfoStore
{
    public:
        typedef std::map<uint32, PetLevelInfo*> PetLevelInfoMap;

        PetLevelInfoStore() {}
        ~PetLevelInfoStore();

        PetLevelInfo* FindOrAddLevels(uint32 creatureId, uint32 levelCount);
        PetLevelInfo const* Levels(uint32 creatureId) const;
        PetLevelInfoMap const& All() const { return m_levels; }

    private:
        PetLevelInfoStore(PetLevelInfoStore const&) = delete;
        PetLevelInfoStore& operator=(PetLevelInfoStore const&) = delete;

        PetLevelInfoMap m_levels;
};

#endif
