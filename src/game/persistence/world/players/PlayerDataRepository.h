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

class QueryResult;

/// The player data on the world database: `ObjectMgr`'s loads of what a new character gets, the
/// level stats and experience, the level-up mails and the reserved names, at startup and on a
/// reload, each run synchronously for a loader that keeps its loop.
namespace PlayerDataRepository
{
    /// Selects every race and class's start position (`playercreateinfo`).
    QueryResult* QueryCreateInfo();

    /// Selects every race and class's start items (`playercreateinfo_item`).
    QueryResult* QueryCreateItems();

    /// Selects every race and class's start spells (`playercreateinfo_spell`).
    QueryResult* QueryCreateSpells();

    /// Selects every race and class's start action buttons (`playercreateinfo_action`).
    QueryResult* QueryCreateActions();

    /// Selects every race and class's stats per level (`player_levelstats`).
    QueryResult* QueryLevelStats();

    /// Selects the experience each level needs (`player_xp_for_level`).
    QueryResult* QueryXpForLevel();

    /// Selects the base exploration experience per level (`exploration_basexp`).
    QueryResult* QueryExplorationXp();

    /// Selects every level-up mail (`mail_level_reward`).
    QueryResult* QueryLevelRewardMails();

    /// Selects every reserved character name (`reserved_name`).
    QueryResult* QueryReservedNames();
}

#endif
