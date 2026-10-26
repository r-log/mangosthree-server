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

#ifndef MANGOS_H_PLAYERDATAREPOSITORY
#define MANGOS_H_PLAYERDATAREPOSITORY

#include "Platform/Define.h"

#include <string>
#include <vector>

class QueryResult;

/// One `playercreateinfo` row.
struct PlayerCreateInfoRow
{
    uint32 race = 0;
    uint32 classId = 0;
    uint32 map = 0;
    uint32 zone = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float orientation = 0.0f;
    uint16 phaseMap = 0;
};

/// One `playercreateinfo_item` row.
struct PlayerCreateItemRow
{
    uint32 race = 0;
    uint32 classId = 0;
    uint32 itemid = 0;
    uint32 amount = 0;
};

/// One `playercreateinfo_spell` row.
struct PlayerCreateSpellRow
{
    uint32 race = 0;
    uint32 classId = 0;
    uint32 spell = 0;
};

/// One `playercreateinfo_action` row.
struct PlayerCreateActionRow
{
    uint32 race = 0;
    uint32 classId = 0;
    uint8 button = 0;
    uint32 action = 0;
    uint8 type = 0;
};

/// One `player_xp_for_level` row.
struct PlayerXpForLevelRow
{
    uint32 lvl = 0;
    uint32 xpForNextLevel = 0;
};

/// One `exploration_basexp` row.
struct ExplorationBaseXpRow
{
    uint32 level = 0;
    uint32 basexp = 0;
};

/// One `mail_level_reward` row.
struct LevelRewardMailRow
{
    uint8 level = 0;
    uint32 raceMask = 0;
    uint32 mailTemplateId = 0;
    uint32 senderEntry = 0;
};

enum
{
    /// The stats a player level stats row holds (`str`, `agi`, `sta`, `inte`, `spi`).
    PLAYER_LEVEL_STATS_ROW_STATS = 5
};

/// One `player_levelstats` row: a race and class's base stats at a level.
struct PlayerLevelStatsRow
{
    uint32 race = 0;
    uint32 classId = 0;
    uint32 level = 0;
    uint8 stats[PLAYER_LEVEL_STATS_ROW_STATS] = {};
};

/// The player data on the world database: `ObjectMgr`'s loads of what a new character gets, the
/// level stats and experience, the level-up mails and the reserved names, at startup and on a
/// reload, each run synchronously and read into rows.
namespace PlayerDataRepository
{
    /// Selects every race and class's start position (`playercreateinfo`).
    std::vector<PlayerCreateInfoRow> QueryCreateInfo();

    /// Selects every race and class's start items (`playercreateinfo_item`).
    std::vector<PlayerCreateItemRow> QueryCreateItems();

    /// Selects every race and class's start spells (`playercreateinfo_spell`).
    std::vector<PlayerCreateSpellRow> QueryCreateSpells();

    /// Selects every race and class's start action buttons (`playercreateinfo_action`).
    std::vector<PlayerCreateActionRow> QueryCreateActions();

    /// Selects every race and class's stats per level (`player_levelstats`).
    std::vector<PlayerLevelStatsRow> QueryLevelStats();

    /// Selects the experience each level needs (`player_xp_for_level`).
    std::vector<PlayerXpForLevelRow> QueryXpForLevel();

    /// Selects the base exploration experience per level (`exploration_basexp`).
    std::vector<ExplorationBaseXpRow> QueryExplorationXp();

    /// Selects every level-up mail (`mail_level_reward`).
    std::vector<LevelRewardMailRow> QueryLevelRewardMails();

    /// Selects every reserved character name (`reserved_name`).
    std::vector<std::string> QueryReservedNames();
}

#endif
