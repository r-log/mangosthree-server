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

#ifndef MANGOS_H_CREATURECLASSLEVELSTATSSTORE
#define MANGOS_H_CREATURECLASSLEVELSTATSSTORE

#include "Platform/Define.h"
#include "SharedDefines.h"
#include "data/CreatureRecords.h"

/**
 * The creature base stats (`creature_template_classlevelstats`): for each level from 1 to
 * `DEFAULT_MAX_CREATURE_LEVEL`, each creature class index (`classToIndex`) and each expansion, the base
 * health, mana, damage, attack powers and armor. A slot never set answers zeros, and a level, class index
 * or expansion past the last answers NULL.
 */
class CreatureClassLevelStatsStore
{
    public:
        CreatureClassLevelStatsStore() { Clear(); }

        void Clear();
        void Set(uint32 level, uint32 classIndex, uint32 expansion, CreatureClassLvlStats const& stats);
        CreatureClassLvlStats const* Find(uint32 level, uint32 classIndex, uint32 expansion) const;

    private:
        static bool InRange(uint32 level, uint32 classIndex, uint32 expansion);

        CreatureClassLvlStats m_stats[DEFAULT_MAX_CREATURE_LEVEL + 1][MAX_CREATURE_CLASS][MAX_EXPANSION + 1];
};

#endif
