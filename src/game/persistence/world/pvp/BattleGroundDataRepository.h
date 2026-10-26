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

#ifndef MANGOS_H_BATTLEGROUNDDATAREPOSITORY
#define MANGOS_H_BATTLEGROUNDDATAREPOSITORY

#include "Platform/Define.h"

#include <optional>
#include <string>
#include <vector>

/// One `battleground_template` row.
struct BattleGroundTemplateRow
{
    uint32 id = 0;
    uint32 minPlayersPerTeam = 0;
    uint32 maxPlayersPerTeam = 0;
    uint32 allianceStartLoc = 0;
    float allianceStartO = 0.0f;
    uint32 hordeStartLoc = 0;
    float hordeStartO = 0.0f;
    float startMaxDist = 0.0f;
};

/// One `battlemaster_entry` row.
struct BattleMasterRow
{
    uint32 entry = 0;
    uint32 bgTemplate = 0;
};

/// One battleground event of a spawn: the spawn's kind (1 a game object, 2 a creature), guid and
/// events, its map and guid when the spawn exists, and the event's map, events and description when
/// `battleground_events` lists it.
struct BattleGroundEventIndexRow
{
    uint8 type = 0;
    uint32 guid = 0;
    uint8 event1 = 0;
    uint8 event2 = 0;
    uint32 map = 0;
    uint32 spawnGuid = 0;
    uint32 descriptionMap = 0;
    uint8 descriptionEvent1 = 0;
    uint8 descriptionEvent2 = 0;
    std::optional<std::string> description;
};

/// The battleground data on the world database: `BattleGroundMgr`'s loads at startup, the event
/// indexes on a reload too, each run synchronously for a loader that keeps its loop.
namespace BattleGroundDataRepository
{
    /// Selects every battleground template (`battleground_template`).
    std::vector<BattleGroundTemplateRow> QueryTemplates();

    /// Selects every battle master's battleground (`battlemaster_entry`).
    std::vector<BattleMasterRow> QueryBattleMasters();

    /// Selects the battleground event indexes of the creature and game object spawns, with the
    /// events that have no spawn.
    std::vector<BattleGroundEventIndexRow> QueryEventIndexes();
}

#endif
