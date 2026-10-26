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

#include "PlayerDataRepository.h"
#include "Database/DatabaseEnv.h"

namespace PlayerDataRepository
{
    QueryResult* QueryCreateInfo()
    {
        //                                  0       1        2      3       4             5             6             7              8
        return WorldDatabase.Query("SELECT `race`, `class`, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation`, `phaseMap` FROM `playercreateinfo`");
    }

    QueryResult* QueryCreateItems()
    {
        //                                 0     1      2       3
        return WorldDatabase.Query("SELECT `race`, `class`, `itemid`, `amount` FROM `playercreateinfo_item`");
    }

    QueryResult* QueryCreateSpells()
    {
        //                                 0     1      2
        return WorldDatabase.Query("SELECT `race`, `class`, `Spell` FROM `playercreateinfo_spell`");
    }

    QueryResult* QueryCreateActions()
    {
        //                                 0     1      2       3       4
        return WorldDatabase.Query("SELECT `race`, `class`, `button`, `action`, `type` FROM `playercreateinfo_action`");
    }

    QueryResult* QueryLevelStats()
    {
        //                                 0     1      2      3    4    5    6    7
        return WorldDatabase.Query("SELECT `race`, `class`, `level`, `str`, `agi`, `sta`, `inte`, `spi` FROM `player_levelstats`");
    }

    QueryResult* QueryXpForLevel()
    {
        //                                 0    1
        return WorldDatabase.Query("SELECT `lvl`, `xp_for_next_level` FROM `player_xp_for_level`");
    }

    QueryResult* QueryExplorationXp()
    {
        return WorldDatabase.Query("SELECT `level`,`basexp` FROM `exploration_basexp`");
    }

    QueryResult* QueryLevelRewardMails()
    {
        return WorldDatabase.Query("SELECT `level`, `raceMask`, `mailTemplateId`, `senderEntry` FROM `mail_level_reward`");
    }

    QueryResult* QueryReservedNames()
    {
        return WorldDatabase.Query("SELECT `name` FROM `reserved_name`");
    }
}
