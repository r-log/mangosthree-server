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

#ifndef MANGOS_H_DUNGEONENCOUNTERSTORE
#define MANGOS_H_DUNGEONENCOUNTERSTORE

#include "Platform/Define.h"
#include "SharedDefines.h"

#include <map>
#include <utility>

struct DungeonEncounterEntry;

struct DungeonEncounter
{
    DungeonEncounter(DungeonEncounterEntry const* _dbcEntry, EncounterCreditType _creditType, uint32 _creditEntry, uint32 _lastEncounterDungeon)
        : dbcEntry(_dbcEntry), creditType(_creditType), creditEntry(_creditEntry), lastEncounterDungeon(_lastEncounterDungeon) { }
    DungeonEncounterEntry const* dbcEntry;
    EncounterCreditType creditType;
    uint32 creditEntry;
    uint32 lastEncounterDungeon;
};

typedef std::multimap<uint32, DungeonEncounter const*> DungeonEncounterMap;
typedef std::pair<DungeonEncounterMap::const_iterator, DungeonEncounterMap::const_iterator> DungeonEncounterMapBounds;

/**
 * The instance encounters (`instance_encounters`): per credit entry (the creature killed or the spell
 * cast that completes an encounter), the encounters it credits, in the order added. The store owns
 * the encounters and deletes them when it is cleared and when it goes.
 */
class DungeonEncounterStore
{
    public:
        DungeonEncounterStore() {}
        ~DungeonEncounterStore() { Clear(); }

        void Clear();
        void Add(uint32 creditEntry, DungeonEncounter const* encounter) { m_encounters.insert(DungeonEncounterMap::value_type(creditEntry, encounter)); }
        DungeonEncounterMapBounds ForCredit(uint32 creditEntry) const { return m_encounters.equal_range(creditEntry); }
        size_t Count() const { return m_encounters.size(); }

    private:
        DungeonEncounterStore(DungeonEncounterStore const&) = delete;
        DungeonEncounterStore& operator=(DungeonEncounterStore const&) = delete;

        DungeonEncounterMap m_encounters;
};

#endif
