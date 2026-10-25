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

#include "BattleGroundStatsRepository.h"
#include "Database/DatabaseEnv.h"

namespace BattleGroundStatsRepository
{
    QueryResult* QueryHighestId()
    {
        return CharacterDatabase.Query("SELECT MAX(`id`) FROM `pvpstats_battlegrounds`");
    }

    void InsertBattleGround(uint64 id, uint32 winnerTeam, uint8 bracket, uint8 type)
    {
        static SqlStatementID insPvPstatsBattleground;

        SqlStatement stmt = CharacterDatabase.CreateStatement(insPvPstatsBattleground, "INSERT INTO pvpstats_battlegrounds (id, winner_team, bracket_id, type, date) VALUES (?, ?, ?, ?, NOW())");
        stmt.PExecute(id, winnerTeam, bracket, type);
    }

    void InsertPlayer(BattleGroundPlayerStatsRow const& row)
    {
        static SqlStatementID insPvPstatsPlayer;
        SqlStatement stmt = CharacterDatabase.CreateStatement(insPvPstatsPlayer, "INSERT INTO `pvpstats_players` (`battleground_id`, `character_guid`, `score_killing_blows`, `score_deaths`, `score_honorable_kills`, `score_bonus_honor`, `score_damage_done`, `score_healing_done`, `attr_1`, `attr_2`, `attr_3`, `attr_4`, `attr_5`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        stmt.addUInt32(row.battleGroundId);
        stmt.addUInt32(row.guid);
        stmt.addUInt32(row.killingBlows);
        stmt.addUInt32(row.deaths);
        stmt.addUInt32(row.honorableKills);
        stmt.addUInt32(row.bonusHonor);
        stmt.addUInt32(row.damageDone);
        stmt.addUInt32(row.healingDone);
        stmt.addUInt32(row.attr1);
        stmt.addUInt32(row.attr2);
        stmt.addUInt32(row.attr3);
        stmt.addUInt32(row.attr4);
        stmt.addUInt32(row.attr5);

        stmt.Execute();
    }
}
