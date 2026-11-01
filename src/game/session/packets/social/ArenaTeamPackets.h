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

#ifndef MANGOS_H_ARENATEAMPACKETS
#define MANGOS_H_ARENATEAMPACKETS

#include "social/ArenaTeamNotices.h"

class WorldPacket;

/// Initializes `packet` as `team`'s roster: the team id, 0, the member count and the team type, then
/// each member's guid, online flag, name, captain flag, level, class, week and season games and wins
/// and personal rating.
void BuildArenaTeamRosterPacket(WorldPacket& packet, ArenaTeam const& team);
/// Initializes `packet` as `team`'s query answer: the id, the name, the type and the emblem's five
/// colours and styles.
void BuildArenaTeamQueryPacket(WorldPacket& packet, ArenaTeam const& team);
/// Initializes `packet` as `team`'s stats: the id, the rating, the week and season games and wins,
/// then the rank.
void BuildArenaTeamStatsPacket(WorldPacket& packet, ArenaTeam const& team);
/// Initializes `packet` as the inspect of `member`'s place in `team`: the member's guid, the team's
/// slot, id, rating and season games and wins, then the member's season games and personal rating.
void BuildArenaTeamInspectPacket(WorldPacket& packet, ArenaTeam const& team, ArenaTeamMember const& member);
/// Initializes `packet` as the calendar's list of `team`'s members other than `requester`: the count,
/// then each member's packed guid and level.
void BuildArenaTeamCalendarInvitePacket(WorldPacket& packet, ArenaTeam const& team, ObjectGuid requester);
/// Initializes `packet` as the team event `event`: the event, the count of strings, the strings, then
/// the guid when it is not empty.
void BuildArenaTeamEventPacket(WorldPacket& packet, ArenaTeamEvents event, ObjectGuid guid, char const* str1, char const* str2, char const* str3);

/// The arena team's notices, each built here: a player's own goes to its client, the team's to its
/// members through the team's own send.
class SessionArenaTeamNotices : public ArenaTeamNotices
{
    public:
        void Stats(ArenaTeam const& team, Player& player) override;
        void Event(ArenaTeam& team, ArenaTeamEvents event, ObjectGuid guid, char const* str1, char const* str2, char const* str3) override;
};

#endif
