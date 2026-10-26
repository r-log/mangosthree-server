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

#ifndef MANGOS_H_ARENATEAMREPOSITORY
#define MANGOS_H_ARENATEAMREPOSITORY

#include <optional>
#include <string>
#include <vector>
#include "Platform/Define.h"


/// An arena team's saved stats: its rating, this week's and this season's games and wins, and
/// its rank.
struct ArenaTeamStatsRow
{
    uint32 rating = 0;
    uint32 gamesWeek = 0;
    uint32 winsWeek = 0;
    uint32 gamesSeason = 0;
    uint32 winsSeason = 0;
    uint32 rank = 0;
};

/// An arena team's saved row: its id, name, captain, type, emblem and stats.
struct ArenaTeamRow
{
    uint32 id = 0;
    std::string name;
    uint32 captainGuid = 0;
    uint32 type = 0;
    uint32 backgroundColor = 0;
    uint32 emblemStyle = 0;
    uint32 emblemColor = 0;
    uint32 borderStyle = 0;
    uint32 borderColor = 0;
    ArenaTeamStatsRow stats;
};

/// A member's saved stats: its low guid, this week's and this season's games and wins, and its
/// personal rating.
struct ArenaTeamMemberStatsRow
{
    uint32 guid = 0;
    uint32 gamesWeek = 0;
    uint32 winsWeek = 0;
    uint32 gamesSeason = 0;
    uint32 winsSeason = 0;
    uint32 personalRating = 0;
};

/// A member's saved row as the load reads it: its team, its stats, and the name and class its
/// character row joins in (an empty name when the character is gone).
struct ArenaTeamMemberRow
{
    uint32 teamId = 0;
    ArenaTeamMemberStatsRow stats;
    std::string name;
    uint8 playerClass = 0;
};

/// One arena team member (`arena_team_member`), with its team's type.
struct ArenaTeamMemberTypeRow
{
    uint32 guid = 0;
    uint32 arenateamid = 0;
    uint32 type = 0;
};

/// The arena teams' saved rows. The start-up reads run synchronously; every write is queued on the
/// character database.
namespace ArenaTeamRepository
{

    /// Writes a new team in one transaction: a DELETE of any member rows left under its id, the
    /// team row with its name bound, then its stats row.
    void Create(ArenaTeamRow const& team);

    /// Inserts a new member's row with its personal rating.
    void InsertMember(uint32 teamId, uint32 memberGuid, uint32 personalRating);

    /// Deletes every member row of a team id, as the load does for members of a team that is gone.
    void DeleteMembersOfTeam(uint32 teamId);

    /// Sets the team's captain.
    void SetCaptain(uint32 teamId, uint32 captainGuid);

    /// Deletes one member's row.
    void DeleteMember(uint32 teamId, uint32 memberGuid);

    /// Deletes the team's rows in one transaction: the team, its members and its stats.
    void Disband(uint32 teamId);

    /// Sets the team's emblem.
    void SetEmblem(uint32 teamId, uint32 backgroundColor, uint32 emblemStyle, uint32 emblemColor, uint32 borderStyle, uint32 borderColor);

    /// Each sets one of the team's stats.
    void SetRating(uint32 teamId, uint32 value);
    void SetGamesWeek(uint32 teamId, uint32 value);
    void SetWinsWeek(uint32 teamId, uint32 value);
    void SetGamesSeason(uint32 teamId, uint32 value);
    void SetWinsSeason(uint32 teamId, uint32 value);
    void SetRank(uint32 teamId, uint32 value);

    /// Writes the team's stats and then each member's in one transaction.
    void SaveStats(uint32 teamId, ArenaTeamStatsRow const& stats, std::vector<ArenaTeamMemberStatsRow> const& members);

    /// Selects the highest arena team id, at start-up.
    std::optional<uint32> QueryHighestId();

    /// Selects every arena team with its stats, ordered by team id, at start-up.
    std::vector<ArenaTeamRow> QueryTeams();

    /// Selects every arena team member with its character's name and class, ordered by team id, at start-up.
    std::vector<ArenaTeamMemberRow> QueryTeamMembers();

    /// Selects every arena team member's team and team type, at start-up.
    std::vector<ArenaTeamMemberTypeRow> QueryMemberTeamTypes();
}

#endif
