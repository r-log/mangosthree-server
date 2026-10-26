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

#ifndef MANGOS_H_DUNGEONDATAREPOSITORY
#define MANGOS_H_DUNGEONDATAREPOSITORY

#include "Platform/Define.h"

#include <optional>
#include <string>
#include <vector>

/// One `instance_encounters` row.
struct InstanceEncounterRow
{
    uint32 entry = 0;
    uint8 creditType = 0;
    uint32 creditEntry = 0;
    uint32 lastEncounterDungeon = 0;
};

/// One `dungeonfinder_requirements` row.
struct DungeonFinderRequirementRow
{
    uint32 mapId = 0;
    uint32 difficulty = 0;
    uint32 minItemLevel = 0;
    uint32 item = 0;
    uint32 item2 = 0;
    uint32 allianceQuest = 0;
    uint32 hordeQuest = 0;
    uint32 achievement = 0;
    std::optional<std::string> questIncompleteText;
};

/// One `dungeonfinder_rewards` row.
struct DungeonFinderRewardRow
{
    uint32 level = 0;
    uint32 baseXpReward = 0;
    int32 baseMonetaryReward = 0;
};

/// One `dungeonfinder_item_rewards` row.
struct DungeonFinderItemRewardRow
{
    uint32 id = 0;
    uint32 minLevel = 0;
    uint32 maxLevel = 0;
    int32 itemReward = 0;
    uint32 itemAmount = 0;
    uint32 dungeonType = 0;
};

/// One `lfg_dungeon_entrances` row.
struct DungeonEntranceRow
{
    uint32 dungeonId = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float orientation = 0.0f;
};

/// The dungeon data on the world database: the dungeon finder's and the instance encounters' loads
/// at startup and on a reload, each run synchronously and read into rows.
namespace DungeonDataRepository
{
    /// Selects every instance encounter (`instance_encounters`).
    std::vector<InstanceEncounterRow> QueryInstanceEncounters();

    /// Selects every dungeon finder entry requirement (`dungeonfinder_requirements`).
    std::vector<DungeonFinderRequirementRow> QueryDungeonFinderRequirements();

    /// Selects every dungeon finder reward (`dungeonfinder_rewards`).
    std::vector<DungeonFinderRewardRow> QueryDungeonFinderRewards();

    /// Selects every dungeon finder item reward (`dungeonfinder_item_rewards`).
    std::vector<DungeonFinderItemRewardRow> QueryDungeonFinderItemRewards();

    /// Selects every dungeon entrance (`lfg_dungeon_entrances`).
    std::vector<DungeonEntranceRow> QueryDungeonEntrances();
}

#endif
