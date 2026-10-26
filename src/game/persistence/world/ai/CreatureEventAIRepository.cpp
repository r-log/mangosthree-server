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

#include "CreatureEventAIRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace CreatureEventAIRepository
{
    static CreatureAISummonRow ReadCreatureAISummon(Field* fields)
    {
        CreatureAISummonRow row;
        row.id = fields[0].GetUInt32();
        row.positionX = fields[1].GetFloat();
        row.positionY = fields[2].GetFloat();
        row.positionZ = fields[3].GetFloat();
        row.orientation = fields[4].GetFloat();
        row.spawntimesecs = fields[5].GetUInt32();
        return row;
    }

    std::vector<CreatureAISummonRow> QuerySummons()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs` FROM `creature_ai_summons`");
        return ReadRows<CreatureAISummonRow>(result, ReadCreatureAISummon);
    }

    static CreatureAIEventRow ReadCreatureAIEvent(Field* fields)
    {
        CreatureAIEventRow row;
        row.id = fields[0].GetUInt32();
        row.creatureId = fields[1].GetUInt32();
        row.eventType = fields[2].GetUInt32();
        row.eventInversePhaseMask = fields[3].GetUInt32();
        row.eventChance = fields[4].GetUInt8();
        row.eventFlags = fields[5].GetUInt8();
        row.eventParam1 = fields[6].GetUInt32();
        row.eventParam2 = fields[7].GetUInt32();
        row.eventParam3 = fields[8].GetUInt32();
        row.eventParam4 = fields[9].GetUInt32();
        for (int j = 0; j < CREATURE_AI_EVENT_ROW_ACTIONS; ++j)
        {
            CreatureAIEventActionRow& action = row.actions[j];
            action.type = fields[10 + (j * 4)].GetUInt16();
            action.param1 = fields[11 + (j * 4)].GetUInt32();
            action.param2 = fields[12 + (j * 4)].GetUInt32();
            action.param3 = fields[13 + (j * 4)].GetUInt32();
        }
        return row;
    }

    std::vector<CreatureAIEventRow> QueryEvents()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `id`, `creature_id`, `event_type`, `event_inverse_phase_mask`, `event_chance`, `event_flags`, "
                                                  "`event_param1`, `event_param2`, `event_param3`, `event_param4`, "
                                                  "`action1_type`, `action1_param1`, `action1_param2`, `action1_param3`, "
                                                  "`action2_type`, `action2_param1`, `action2_param2`, `action2_param3`, "
                                                  "`action3_type`, `action3_param1`, `action3_param2`, `action3_param3` "
                                                  "FROM `creature_ai_scripts`");
        return ReadRows<CreatureAIEventRow>(result, ReadCreatureAIEvent);
    }
}
