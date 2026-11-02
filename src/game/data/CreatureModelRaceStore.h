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

#ifndef MANGOS_H_CREATUREMODELRACESTORE
#define MANGOS_H_CREATUREMODELRACESTORE

#include "Platform/Define.h"
#include "data/CreatureRecords.h"

#include <map>
#include <utility>

/**
 * The racial models (`creature_model_race`): per native model id, the models that replace it for
 * the races in each entry's mask, in the order added. The loader clears and fills it.
 */
class CreatureModelRaceStore
{
    public:
        typedef std::multimap<uint32, CreatureModelRace> CreatureModelRaceMap;
        typedef std::pair<CreatureModelRaceMap::const_iterator, CreatureModelRaceMap::const_iterator> CreatureModelRaceMapBounds;

        void Clear() { m_races.clear(); }
        void Add(CreatureModelRace const& race) { m_races.insert(CreatureModelRaceMap::value_type(race.modelid, race)); }
        CreatureModelRaceMapBounds ForModel(uint32 modelId) const { return m_races.equal_range(modelId); }

    private:
        CreatureModelRaceMap m_races;
};

#endif
