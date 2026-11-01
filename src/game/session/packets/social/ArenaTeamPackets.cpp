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

#include "ArenaTeamPackets.h"
#include "Log.h"
#include "Opcodes.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "WorldPacket.h"

void BuildArenaTeamRosterPacket(WorldPacket& packet, ArenaTeam const& team)
{
    uint8 unk308 = 0;

    packet.Initialize(SMSG_ARENA_TEAM_ROSTER, 100);
    packet << uint32(team.GetId());                         // team id
    packet << uint8(unk308);                                // 308 unknown value but affect packet structure
    packet << uint32(team.GetMembersSize());                // members count
    packet << uint32(team.GetType());                       // arena team type?

    for (ArenaTeam::MemberList::const_iterator itr = team.GetMembers().begin(); itr != team.GetMembers().end(); ++itr)
    {
        Player* pl = sPlayerRegistry.Find(itr->guid);

        packet << itr->guid;                                // guid
        packet << uint8((pl ? 1 : 0));                      // online flag
        packet << itr->name;                                // member name
        packet << uint32((itr->guid == team.GetCaptainGuid() ? 0 : 1));// captain flag 0 captain 1 member
        packet << uint8((pl ? pl->getLevel() : 0));         // unknown, level?
        packet << uint8(itr->Class);                        // class
        packet << uint32(itr->games_week);                  // played this week
        packet << uint32(itr->wins_week);                   // wins this week
        packet << uint32(itr->games_season);                // played this season
        packet << uint32(itr->wins_season);                 // wins this season
        packet << uint32(itr->personal_rating);             // personal rating
        if (unk308)
        {
            packet << float(0.0);                           // 308 unk
            packet << float(0.0);                           // 308 unk
        }
    }
}

void BuildArenaTeamQueryPacket(WorldPacket& packet, ArenaTeam const& team)
{
    packet.Initialize(SMSG_ARENA_TEAM_QUERY_RESPONSE, 4 * 7 + team.GetName().size() + 1);
    packet << uint32(team.GetId());                         // team id
    packet << team.GetName();                               // team name
    packet << uint32(team.GetType());                       // arena team type (2=2x2, 3=3x3 or 5=5x5)
    packet << uint32(team.GetBackgroundColor());            // background color
    packet << uint32(team.GetEmblemStyle());                // emblem style
    packet << uint32(team.GetEmblemColor());                // emblem color
    packet << uint32(team.GetBorderStyle());                // border style
    packet << uint32(team.GetBorderColor());                // border color
}

void BuildArenaTeamStatsPacket(WorldPacket& packet, ArenaTeam const& team)
{
    ArenaTeamStats const& stats = team.GetStats();
    packet.Initialize(SMSG_ARENA_TEAM_STATS, 4 * 7);
    packet << uint32(team.GetId());                         // team id
    packet << uint32(stats.rating);                         // rating
    packet << uint32(stats.games_week);                     // games this week
    packet << uint32(stats.wins_week);                      // wins this week
    packet << uint32(stats.games_season);                   // played this season
    packet << uint32(stats.wins_season);                    // wins this season
    packet << uint32(stats.rank);                           // rank
}

void BuildArenaTeamInspectPacket(WorldPacket& packet, ArenaTeam const& team, ArenaTeamMember const& member)
{
    ArenaTeamStats const& stats = team.GetStats();
    packet.Initialize(MSG_INSPECT_ARENA_TEAMS, 8 + 1 + 4 * 6);
    packet << member.guid;                                  // player guid
    packet << uint8(team.GetSlot());                        // slot (0...2)
    packet << uint32(team.GetId());                         // arena team id
    packet << uint32(stats.rating);                         // rating
    packet << uint32(stats.games_season);                   // season played
    packet << uint32(stats.wins_season);                    // season wins
    packet << uint32(member.games_season);                  // played (count of all games, that the inspected member participated...)
    packet << uint32(member.personal_rating);               // personal rating
}

void BuildArenaTeamCalendarInvitePacket(WorldPacket& packet, ArenaTeam const& team, ObjectGuid requester)
{
    // The requester is left out of the list, so the count is written once the list is: a
    // placeholder first, then the real count. The client reads exactly `count` (packed guid,
    // uint8) pairs.
    uint32 count = 0;

    packet.Initialize(SMSG_CALENDAR_ARENA_TEAM);
    packet << uint32(count);                                // count placeholder

    for (ArenaTeam::MemberList::const_iterator itr = team.GetMembers().begin(); itr != team.GetMembers().end(); ++itr)
    {
        uint32 level = Player::GetLevelFromDB(itr->guid);

        if (itr->guid != requester)
        {
            packet << itr->guid.WriteAsPacked();
            packet << uint8(level);
            ++count;
        }
    }

    packet.put<uint32>(0, count);
}

void BuildArenaTeamEventPacket(WorldPacket& packet, ArenaTeamEvents event, ObjectGuid guid, char const* str1, char const* str2, char const* str3)
{
    uint8 strCount = !str1 ? 0 : (!str2 ? 1 : (!str3 ? 2 : 3));

    packet.Initialize(SMSG_ARENA_TEAM_EVENT, 1 + 1 + 1 * strCount + (!guid ? 0 : 8));
    packet << uint8(event);
    packet << uint8(strCount);

    if (str3)
    {
        packet << str1;
        packet << str2;
        packet << str3;
    }
    else if (str2)
    {
        packet << str1;
        packet << str2;
    }
    else if (str1)
    {
        packet << str1;
    }

    if (guid)
    {
        packet << ObjectGuid(guid);
    }
}

void SessionArenaTeamNotices::Stats(ArenaTeam const& team, Player& player)
{
    WorldPacket data;
    BuildArenaTeamStatsPacket(data, team);
    player.GetSession()->SendPacket(&data);
}

void SessionArenaTeamNotices::Event(ArenaTeam& team, ArenaTeamEvents event, ObjectGuid guid, char const* str1, char const* str2, char const* str3)
{
    WorldPacket data;
    BuildArenaTeamEventPacket(data, event, guid, str1, str2, str3);
    team.BroadcastPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_ARENA_TEAM_EVENT");
}
