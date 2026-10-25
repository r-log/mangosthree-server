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

#include "ArenaTeamRepository.h"
#include "Database/DatabaseEnv.h"

namespace ArenaTeamRepository
{
    ArenaTeamRow ReadTeam(Field* fields)
    {
        ArenaTeamRow team;
        team.id                = fields[0].GetUInt32();
        team.name              = fields[1].GetCppString();
        team.captainGuid       = fields[2].GetUInt32();
        team.type              = fields[3].GetUInt32();
        team.backgroundColor   = fields[4].GetUInt32();
        team.emblemStyle       = fields[5].GetUInt32();
        team.emblemColor       = fields[6].GetUInt32();
        team.borderStyle       = fields[7].GetUInt32();
        team.borderColor       = fields[8].GetUInt32();
        team.stats.rating      = fields[9].GetUInt32();
        team.stats.gamesWeek   = fields[10].GetUInt32();
        team.stats.winsWeek    = fields[11].GetUInt32();
        team.stats.gamesSeason = fields[12].GetUInt32();
        team.stats.winsSeason  = fields[13].GetUInt32();
        team.stats.rank        = fields[14].GetUInt32();
        return team;
    }

    ArenaTeamMemberRow ReadMember(Field* fields)
    {
        ArenaTeamMemberRow member;
        member.teamId               = fields[0].GetUInt32();
        member.stats.guid           = fields[1].GetUInt32();
        member.stats.gamesWeek      = fields[2].GetUInt32();
        member.stats.winsWeek       = fields[3].GetUInt32();
        member.stats.gamesSeason    = fields[4].GetUInt32();
        member.stats.winsSeason     = fields[5].GetUInt32();
        member.stats.personalRating = fields[6].GetUInt32();
        member.name                 = fields[7].GetCppString();
        member.playerClass          = fields[8].GetUInt8();
        return member;
    }

    void Create(ArenaTeamRow const& team)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `arena_team_member` WHERE `arenateamid`='%u'", team.id);

        // The name is bound, not escaped; the statement joins the transaction opened above.
        static SqlStatementID insArenaTeam;
        SqlStatement insert = CharacterDatabase.CreateStatement(insArenaTeam,
                              "INSERT INTO `arena_team` (`arenateamid`,`name`,`captainguid`,`type`,`BackgroundColor`,`EmblemStyle`,`EmblemColor`,`BorderStyle`,`BorderColor`) "
                              "VALUES(?,?,?,?,?,?,?,?,?)");
        insert.addUInt32(team.id);
        insert.addString(team.name);
        insert.addUInt32(team.captainGuid);
        insert.addUInt32(team.type);
        insert.addUInt32(team.backgroundColor);
        insert.addUInt32(team.emblemStyle);
        insert.addUInt32(team.emblemColor);
        insert.addUInt32(team.borderStyle);
        insert.addUInt32(team.borderColor);
        insert.Execute();

        CharacterDatabase.PExecute("INSERT INTO `arena_team_stats` (`arenateamid`, `rating`, `games_week`, `wins_week`, `games_season`, `wins_season`, `rank`) VALUES "
                                   "('%u', '%u', '%u', '%u', '%u', '%u', '%u')", team.id, team.stats.rating, team.stats.gamesWeek, team.stats.winsWeek, team.stats.gamesSeason, team.stats.winsSeason, team.stats.rank);

        CharacterDatabase.CommitTransaction();
    }

    void InsertMember(uint32 teamId, uint32 memberGuid, uint32 personalRating)
    {
        CharacterDatabase.PExecute("INSERT INTO `arena_team_member` (`arenateamid`, `guid`, `personal_rating`) VALUES ('%u', '%u', '%u')", teamId, memberGuid, personalRating);
    }

    void DeleteMembersOfTeam(uint32 teamId)
    {
        CharacterDatabase.PExecute("DELETE FROM `arena_team_member` WHERE `arenateamid` = '%u'", teamId);
    }

    void SetCaptain(uint32 teamId, uint32 captainGuid)
    {
        CharacterDatabase.PExecute("UPDATE `arena_team` SET `captainguid` = '%u' WHERE `arenateamid` = '%u'", captainGuid, teamId);
    }

    void DeleteMember(uint32 teamId, uint32 memberGuid)
    {
        CharacterDatabase.PExecute("DELETE FROM `arena_team_member` WHERE `arenateamid` = '%u' AND `guid` = '%u'", teamId, memberGuid);
    }

    void Disband(uint32 teamId)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `arena_team` WHERE `arenateamid` = '%u'", teamId);
        CharacterDatabase.PExecute("DELETE FROM `arena_team_member` WHERE `arenateamid` = '%u'", teamId);
        CharacterDatabase.PExecute("DELETE FROM `arena_team_stats` WHERE `arenateamid` = '%u'", teamId);
        CharacterDatabase.CommitTransaction();
    }

    void SetEmblem(uint32 teamId, uint32 backgroundColor, uint32 emblemStyle, uint32 emblemColor, uint32 borderStyle, uint32 borderColor)
    {
        CharacterDatabase.PExecute("UPDATE `arena_team` SET `BackgroundColor`='%u', `EmblemStyle`='%u', `EmblemColor`='%u', `BorderStyle`='%u', `BorderColor`='%u' WHERE `arenateamid`='%u'", backgroundColor, emblemStyle, emblemColor, borderStyle, borderColor, teamId);
    }

    void SetRating(uint32 teamId, uint32 value)
    {
        CharacterDatabase.PExecute("UPDATE `arena_team_stats` SET `rating` = '%u' WHERE `arenateamid` = '%u'", value, teamId);
    }

    void SetGamesWeek(uint32 teamId, uint32 value)
    {
        CharacterDatabase.PExecute("UPDATE `arena_team_stats` SET `games_week` = '%u' WHERE `arenateamid` = '%u'", value, teamId);
    }

    void SetWinsWeek(uint32 teamId, uint32 value)
    {
        CharacterDatabase.PExecute("UPDATE `arena_team_stats` SET `wins_week` = '%u' WHERE `arenateamid` = '%u'", value, teamId);
    }

    void SetGamesSeason(uint32 teamId, uint32 value)
    {
        CharacterDatabase.PExecute("UPDATE `arena_team_stats` SET `games_season` = '%u' WHERE `arenateamid` = '%u'", value, teamId);
    }

    void SetWinsSeason(uint32 teamId, uint32 value)
    {
        CharacterDatabase.PExecute("UPDATE `arena_team_stats` SET `wins_season` = '%u' WHERE `arenateamid` = '%u'", value, teamId);
    }

    void SetRank(uint32 teamId, uint32 value)
    {
        CharacterDatabase.PExecute("UPDATE `arena_team_stats` SET `rank` = '%u' WHERE `arenateamid` = '%u'", value, teamId);
    }

    void SaveStats(uint32 teamId, ArenaTeamStatsRow const& stats, std::vector<ArenaTeamMemberStatsRow> const& members)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("UPDATE `arena_team_stats` SET `rating` = '%u',`games_week` = '%u',`games_season` = '%u',`rank` = '%u',`wins_week` = '%u',`wins_season` = '%u' WHERE `arenateamid` = '%u'", stats.rating, stats.gamesWeek, stats.gamesSeason, stats.rank, stats.winsWeek, stats.winsSeason, teamId);
        for (std::vector<ArenaTeamMemberStatsRow>::const_iterator itr = members.begin(); itr != members.end(); ++itr)
        {
            CharacterDatabase.PExecute("UPDATE `arena_team_member` SET `played_week` = '%u', `wons_week` = '%u', `played_season` = '%u', `wons_season` = '%u', `personal_rating` = '%u' WHERE `arenateamid` = '%u' AND `guid` = '%u'", itr->gamesWeek, itr->winsWeek, itr->gamesSeason, itr->winsSeason, itr->personalRating, teamId, itr->guid);
        }
        CharacterDatabase.CommitTransaction();
    }

    QueryResult* QueryHighestId()
    {
        return CharacterDatabase.Query("SELECT MAX(`arenateamid`) FROM `arena_team`");
    }

    QueryResult* QueryTeams()
    {
        //                                      0                          1      2             3      4                 5
        return CharacterDatabase.Query("SELECT `arena_team`.`arenateamid`,`name`,`captainguid`,`type`,`BackgroundColor`,`EmblemStyle`,"
                                       //   6          7             8              9        10           11          12             13            14
                                       "`EmblemColor`,`BorderStyle`,`BorderColor`, `rating`,`games_week`,`wins_week`,`games_season`,`wins_season`,`rank` "
                                       "FROM `arena_team` LEFT JOIN `arena_team_stats` ON `arena_team`.`arenateamid` = `arena_team_stats`.`arenateamid` ORDER BY `arena_team`.`arenateamid` ASC");
    }

    QueryResult* QueryTeamMembers()
    {
        //                                     0                   1      2             3           4               5             6                 7      8
        return CharacterDatabase.Query("SELECT `arenateamid`,`member`.`guid`,`played_week`,`wons_week`,`played_season`,`wons_season`,`personal_rating`,`name`,`class` "
                                       "FROM `arena_team_member` member LEFT JOIN `characters` chars on member.`guid` = chars.`guid` ORDER BY member.`arenateamid` ASC");
    }

    QueryResult* QueryMemberTeamTypes()
    {
        //                                                                            0                   1                  2
        return CharacterDatabase.Query("SELECT `arena_team_member`.`guid`, `arena_team_member`.`arenateamid`, `arena_team`.`type` "
                                       "FROM `arena_team_member` JOIN `arena_team` ON `arena_team_member`.`arenateamid` = `arena_team`.`arenateamid`");
    }
}
