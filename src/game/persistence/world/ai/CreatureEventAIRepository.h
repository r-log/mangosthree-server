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

#ifndef MANGOS_H_CREATUREEVENTAIREPOSITORY
#define MANGOS_H_CREATUREEVENTAIREPOSITORY

#include "Platform/Define.h"

#include <vector>

/// One `creature_ai_summons` row.
struct CreatureAISummonRow
{
    uint32 id = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float orientation = 0.0f;
    uint32 spawntimesecs = 0;
};

enum
{
    /// The actions of a creature AI event (`action1` to `action3`).
    CREATURE_AI_EVENT_ROW_ACTIONS = 3
};

/// One action of a creature AI event: its type and its three parameters.
struct CreatureAIEventActionRow
{
    uint16 type = 0;
    uint32 param1 = 0;
    uint32 param2 = 0;
    uint32 param3 = 0;
};

/// One `creature_ai_scripts` row: an event of a creature's AI, its four parameters and its
/// actions.
struct CreatureAIEventRow
{
    uint32 id = 0;
    uint32 creatureId = 0;
    uint32 eventType = 0;
    uint32 eventInversePhaseMask = 0;
    uint8 eventChance = 0;
    uint8 eventFlags = 0;
    uint32 eventParam1 = 0;
    uint32 eventParam2 = 0;
    uint32 eventParam3 = 0;
    uint32 eventParam4 = 0;
    CreatureAIEventActionRow actions[CREATURE_AI_EVENT_ROW_ACTIONS];
};

/// The creature event AI data on the world database: `CreatureEventAIMgr`'s loads at startup and on
/// a reload, each run synchronously and read into rows.
namespace CreatureEventAIRepository
{
    /// Selects every event AI summon position (`creature_ai_summons`).
    std::vector<CreatureAISummonRow> QuerySummons();

    /// Selects every event AI script row (`creature_ai_scripts`).
    std::vector<CreatureAIEventRow> QueryEvents();
}

#endif
