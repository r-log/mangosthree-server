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
#include "persistence/RowReader.h"

namespace PlayerDataRepository
{
    static PlayerCreateInfoRow ReadPlayerCreateInfo(Field* fields)
    {
        PlayerCreateInfoRow row;
        row.race = fields[0].GetUInt32();
        row.classId = fields[1].GetUInt32();
        row.map = fields[2].GetUInt32();
        row.zone = fields[3].GetUInt32();
        row.positionX = fields[4].GetFloat();
        row.positionY = fields[5].GetFloat();
        row.positionZ = fields[6].GetFloat();
        row.orientation = fields[7].GetFloat();
        row.phaseMap = fields[8].GetUInt16();
        return row;
    }

    std::vector<PlayerCreateInfoRow> QueryCreateInfo()
    {
        //                                  0       1        2      3       4             5             6             7              8
        QueryResult* result = WorldDatabase.Query("SELECT `race`, `class`, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation`, `phaseMap` FROM `playercreateinfo`");
        return ReadRows<PlayerCreateInfoRow>(result, ReadPlayerCreateInfo);
    }

    static PlayerCreateItemRow ReadPlayerCreateItem(Field* fields)
    {
        PlayerCreateItemRow row;
        row.race = fields[0].GetUInt32();
        row.classId = fields[1].GetUInt32();
        row.itemid = fields[2].GetUInt32();
        row.amount = fields[3].GetUInt32();
        return row;
    }

    std::vector<PlayerCreateItemRow> QueryCreateItems()
    {
        //                                 0     1      2       3
        QueryResult* result = WorldDatabase.Query("SELECT `race`, `class`, `itemid`, `amount` FROM `playercreateinfo_item`");
        return ReadRows<PlayerCreateItemRow>(result, ReadPlayerCreateItem);
    }

    static PlayerCreateSpellRow ReadPlayerCreateSpell(Field* fields)
    {
        PlayerCreateSpellRow row;
        row.race = fields[0].GetUInt32();
        row.classId = fields[1].GetUInt32();
        row.spell = fields[2].GetUInt32();
        return row;
    }

    std::vector<PlayerCreateSpellRow> QueryCreateSpells()
    {
        //                                 0     1      2
        QueryResult* result = WorldDatabase.Query("SELECT `race`, `class`, `Spell` FROM `playercreateinfo_spell`");
        return ReadRows<PlayerCreateSpellRow>(result, ReadPlayerCreateSpell);
    }

    static PlayerCreateActionRow ReadPlayerCreateAction(Field* fields)
    {
        PlayerCreateActionRow row;
        row.race = fields[0].GetUInt32();
        row.classId = fields[1].GetUInt32();
        row.button = fields[2].GetUInt8();
        row.action = fields[3].GetUInt32();
        row.type = fields[4].GetUInt8();
        return row;
    }

    std::vector<PlayerCreateActionRow> QueryCreateActions()
    {
        //                                 0     1      2       3       4
        QueryResult* result = WorldDatabase.Query("SELECT `race`, `class`, `button`, `action`, `type` FROM `playercreateinfo_action`");
        return ReadRows<PlayerCreateActionRow>(result, ReadPlayerCreateAction);
    }

    QueryResult* QueryLevelStats()
    {
        //                                 0     1      2      3    4    5    6    7
        return WorldDatabase.Query("SELECT `race`, `class`, `level`, `str`, `agi`, `sta`, `inte`, `spi` FROM `player_levelstats`");
    }

    static PlayerXpForLevelRow ReadPlayerXpForLevel(Field* fields)
    {
        PlayerXpForLevelRow row;
        row.lvl = fields[0].GetUInt32();
        row.xpForNextLevel = fields[1].GetUInt32();
        return row;
    }

    std::vector<PlayerXpForLevelRow> QueryXpForLevel()
    {
        //                                 0    1
        QueryResult* result = WorldDatabase.Query("SELECT `lvl`, `xp_for_next_level` FROM `player_xp_for_level`");
        return ReadRows<PlayerXpForLevelRow>(result, ReadPlayerXpForLevel);
    }

    static ExplorationBaseXpRow ReadExplorationBaseXp(Field* fields)
    {
        ExplorationBaseXpRow row;
        row.level = fields[0].GetUInt32();
        row.basexp = fields[1].GetUInt32();
        return row;
    }

    std::vector<ExplorationBaseXpRow> QueryExplorationXp()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `level`,`basexp` FROM `exploration_basexp`");
        return ReadRows<ExplorationBaseXpRow>(result, ReadExplorationBaseXp);
    }

    static LevelRewardMailRow ReadLevelRewardMail(Field* fields)
    {
        LevelRewardMailRow row;
        row.level = fields[0].GetUInt8();
        row.raceMask = fields[1].GetUInt32();
        row.mailTemplateId = fields[2].GetUInt32();
        row.senderEntry = fields[3].GetUInt32();
        return row;
    }

    std::vector<LevelRewardMailRow> QueryLevelRewardMails()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `level`, `raceMask`, `mailTemplateId`, `senderEntry` FROM `mail_level_reward`");
        return ReadRows<LevelRewardMailRow>(result, ReadLevelRewardMail);
    }

    QueryResult* QueryReservedNames()
    {
        return WorldDatabase.Query("SELECT `name` FROM `reserved_name`");
    }
}
