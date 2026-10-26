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

#ifndef MANGOS_H_BATTLEGROUNDSTATSREPOSITORY
#define MANGOS_H_BATTLEGROUNDSTATSREPOSITORY

#include "Platform/Define.h"

class QueryResult;

/// One player's saved score of a battleground, in the order of the INSERT's columns.
struct BattleGroundPlayerStatsRow
{
    uint32 battleGroundId = 0;
    uint32 guid = 0;
    uint32 killingBlows = 0;
    uint32 deaths = 0;
    uint32 honorableKills = 0;
    uint32 bonusHonor = 0;
    uint32 damageDone = 0;
    uint32 healingDone = 0;
    uint32 attr1 = 0;
    uint32 attr2 = 0;
    uint32 attr3 = 0;
    uint32 attr4 = 0;
    uint32 attr5 = 0;
};

/// The battleground score statistics (`pvpstats_battlegrounds`, `pvpstats_players`), written when
/// they are switched on. The startup read runs synchronously; every write is queued on the
/// character database.
namespace BattleGroundStatsRepository
{
    /// Selects the highest battleground statistics id.
    QueryResult* QueryHighestId();

    /// Inserts a finished battleground's row, dated now.
    void InsertBattleGround(uint64 id, uint32 winnerTeam, uint8 bracket, uint8 type);

    /// Inserts one player's score of a finished battleground.
    void InsertPlayer(BattleGroundPlayerStatsRow const& row);
}

#endif
