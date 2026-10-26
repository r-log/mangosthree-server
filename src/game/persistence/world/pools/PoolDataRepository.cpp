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

#include "PoolDataRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace PoolDataRepository
{
    std::optional<uint16> QueryHighestPoolEntry()
    {
        QueryResult* result = WorldDatabase.Query("SELECT MAX(`entry`) FROM `pool_template`");
        return ReadRow<uint16>(result, [](Field* fields) { return fields[0].GetUInt16(); });
    }

    static PoolTemplateRow ReadPoolTemplate(Field* fields)
    {
        PoolTemplateRow row;
        row.entry = fields[0].GetUInt16();
        row.maxLimit = fields[1].GetUInt32();
        row.description = fields[2].GetCppString();
        return row;
    }

    std::vector<PoolTemplateRow> QueryPools()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`, `max_limit`, `description` FROM `pool_template`");
        return ReadRows<PoolTemplateRow>(result, ReadPoolTemplate);
    }

    static PoolCreatureRow ReadPoolCreature(Field* fields)
    {
        PoolCreatureRow row;
        row.guid = fields[0].GetUInt32();
        row.poolEntry = fields[1].GetUInt16();
        row.chance = fields[2].GetFloat();
        return row;
    }

    std::vector<PoolCreatureRow> QueryPoolCreatures()
    {
        //                                 1     2           3
        QueryResult* result = WorldDatabase.Query("SELECT `guid`, `pool_entry`, `chance` FROM `pool_creature`");
        return ReadRows<PoolCreatureRow>(result, ReadPoolCreature);
    }

    static PoolCreatureTemplateRow ReadPoolCreatureTemplate(Field* fields)
    {
        PoolCreatureTemplateRow row;
        row.guid = fields[0].GetUInt32();
        row.poolEntry = fields[1].GetUInt16();
        row.chance = fields[2].GetFloat();
        row.id = fields[3].GetUInt32();
        return row;
    }

    std::vector<PoolCreatureTemplateRow> QueryPoolCreatureTemplates()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `guid`, `pool_entry`, `chance`, `pool_creature_template`.`id` FROM `pool_creature_template` LEFT JOIN `creature` ON `creature`.`id` = `pool_creature_template`.`id`");
        return ReadRows<PoolCreatureTemplateRow>(result, ReadPoolCreatureTemplate);
    }

    static PoolGameObjectRow ReadPoolGameObject(Field* fields)
    {
        PoolGameObjectRow row;
        row.guid = fields[0].GetUInt32();
        row.poolEntry = fields[1].GetUInt16();
        row.chance = fields[2].GetFloat();
        return row;
    }

    std::vector<PoolGameObjectRow> QueryPoolGameObjects()
    {
        //                                 1     2           3
        QueryResult* result = WorldDatabase.Query("SELECT `guid`, `pool_entry`, `chance` FROM `pool_gameobject`");
        return ReadRows<PoolGameObjectRow>(result, ReadPoolGameObject);
    }

    static PoolGameObjectTemplateRow ReadPoolGameObjectTemplate(Field* fields)
    {
        PoolGameObjectTemplateRow row;
        row.guid = fields[0].GetUInt32();
        row.poolEntry = fields[1].GetUInt16();
        row.chance = fields[2].GetFloat();
        row.id = fields[3].GetUInt32();
        return row;
    }

    std::vector<PoolGameObjectTemplateRow> QueryPoolGameObjectTemplates()
    {
        //                                 1     2           3
        QueryResult* result = WorldDatabase.Query("SELECT `guid`, `pool_entry`, `chance`, `pool_gameobject_template`.`id` FROM `pool_gameobject_template` LEFT JOIN `gameobject` ON `gameobject`.`id` = `pool_gameobject_template`.`id`");
        return ReadRows<PoolGameObjectTemplateRow>(result, ReadPoolGameObjectTemplate);
    }

    static PoolPoolRow ReadPoolPool(Field* fields)
    {
        PoolPoolRow row;
        row.poolId = fields[0].GetUInt16();
        row.motherPool = fields[1].GetUInt16();
        row.chance = fields[2].GetFloat();
        return row;
    }

    std::vector<PoolPoolRow> QueryPoolPools()
    {
        //                                 1        2            3
        QueryResult* result = WorldDatabase.Query("SELECT `pool_id`, `mother_pool`, `chance` FROM `pool_pool`");
        return ReadRows<PoolPoolRow>(result, ReadPoolPool);
    }
}
