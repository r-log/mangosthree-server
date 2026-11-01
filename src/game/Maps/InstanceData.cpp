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

/**
 * @file InstanceData.cpp
 * @brief Instance script data persistence base implementation
 *
 * This file provides the base implementation for saving/loading instance
 * script data. InstanceData is the abstract base class that all dungeon/raid
 * scripts inherit from to persist state (boss kills, encounters, etc.) to the
 * database.
 *
 * Data is stored in either:
 * - `instance` table for dungeons/raids (instanced maps)
 * - `world` table for continent/world state (non-instanced maps)
 *
 * @see InstanceData for the abstract interface
 * @see Map for the owning map instance
 */

#include "Utilities/Errors.h"
#include <string>
#include "InstanceData.h"
#include "InstanceDataCache.h"
#include "Map.h"
#include "Log.h"
#include "Maps/MapNotices.h"
#include "persistence/characters/maps/InstanceDataRepository.h"

/**
 * @brief Save instance state to database
 *
 * Persists the instance script data to the appropriate database table.
 * Skips saving for:
 * - Battlegrounds/Arenas (no persistence needed)
 * - Instances with no data to save (Save() returns empty string)
 *
 * Data is escaped before storage to prevent SQL injection.
 *
 * @note Called periodically and on instance shutdown
 */
void InstanceData::SaveToDB() const
{
    // no reason to save BGs/Arenas
    if (instance->IsBattleGroundOrArena())
    {
        return;
    }

    if (!Save())
    {
        return;
    }

    // The data is a bound parameter, and the write tells InstanceDataCache beside itself: the
    // same string, unescaped, because that is what the table holds once the server's own parser
    // has undone the escaping. The mirror is an update too: it changes an existing entry and
    // creates none.
    const std::string data = Save();

    if (instance->Instanceable())
    {
        InstanceDataRepository::SetInstanceData(data, instance->GetInstanceId());

        sInstanceDataCache.UpdateInstance(instance->GetInstanceId(), data);
    }
    else
    {
        InstanceDataRepository::SetWorldData(data, instance->GetId());

        sInstanceDataCache.UpdateWorld(instance->GetId(), data);
    }
}

bool InstanceData::CheckAchievementCriteriaMeet(uint32 criteria_id, Player const* /*source*/, Unit const* /*target*/ /*= NULL*/, uint32 /*miscvalue1*/ /*= 0*/) const
{
    sLog.outError("Achievement system call InstanceData::CheckAchievementCriteriaMeet but instance script for map %u not have implementation for achievement criteria %u",
                  instance->GetId(), criteria_id);
    return false;
}

/**
 * @brief Check if instance condition criteria are met
 * @param source Player to check conditions for
 * @param instance_condition_id Condition identifier from database
 * @param conditionSource WorldObject triggering the condition check
 * @param conditionSourceType Type of the condition source
 * @return true if conditions are met, false otherwise
 *
 * Base implementation logs an error and returns false. Instance scripts
 * should override this to implement custom condition checking logic for
 * access requirements, quests, or other instance-specific conditions.
 *
 * @warning Derived classes must override this for condition system support
 */
bool InstanceData::CheckConditionCriteriaMeet(Player const* /*source*/, uint32 instance_condition_id, WorldObject const* /*conditionSource*/, uint32 conditionSourceType) const
{
    sLog.outError("Condition system call InstanceData::CheckConditionCriteriaMeet but instance script for map %u not have implementation for player condition criteria with internal id %u (called from %u)",
                  instance->GetId(), instance_condition_id, uint32(conditionSourceType));
    return false;
}

void InstanceData::SendEncounterFrame(uint32 type, ObjectGuid sourceGuid /*= NULL*/, uint8 param1 /*= 0*/, uint8 param2 /*= 0*/)
{
    sMapNotices.EncounterFrame(*instance, type, sourceGuid, param1, param2);
}
