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

#ifndef MANGOS_H_GUILDPACKETS
#define MANGOS_H_GUILDPACKETS

#include "social/GuildNotices.h"

class ByteBuffer;
class PlayerSession;
class WorldPacket;

/// Initializes `data` as `guild`'s roster: each member's guid, notes, name, class, rank, status, zone,
/// level and time offline, then the guild information, the message of the day, the account count
/// and the creation date.
void BuildGuildRosterPacket(WorldPacket& data, Guild const& guild);
/// Initializes `data` as `guild`'s query answer: its guid and name, ten rank names, the ranks' two
/// orders, the emblem and the rank count.
void BuildGuildQueryPacket(WorldPacket& data, Guild const& guild);
/// Initializes `data` as `guild`'s ranks: each rank's name, its bank tab rights and withdrawals, its
/// money per day and its rights.
void BuildGuildRanksPacket(WorldPacket& data, Guild const& guild);
/// Initializes `data` as `guild`'s event log: the count, then each entry.
void BuildGuildEventLogPacket(WorldPacket& data, Guild const& guild);
/// Writes `entry` into the event log packet: its guid masks into `data`, its bytes into `buffer`.
void WriteGuildEventLogEntry(WorldPacket& data, ByteBuffer& buffer, GuildEventLogEntry const& entry);
/// Initializes `data` as the calendar's list of `guild`'s members other than `requester` with a level
/// from `minLevel` to `maxLevel` and a rank up to `minRank`: the count, then each member's packed
/// guid and level. False, with the list unfinished, when the list would pass the calendar's limit.
bool BuildGuildCalendarInvitePacket(WorldPacket& data, Guild const& guild, ObjectGuid requester, uint32 minLevel, uint32 maxLevel, uint32 minRank);
/// Initializes `data` as the guild event `event`: the event, the count of strings, the strings, then
/// the guid when it is not empty.
void BuildGuildEventPacket(WorldPacket& data, GuildEvents event, ObjectGuid guid, char const* str1, char const* str2, char const* str3);

/// Sends `session` `guild`'s roster.
void SendGuildRoster(PlayerSession& session, Guild const& guild);
/// Sends `guild`'s roster to every online member.
void BroadcastGuildRoster(Guild& guild);
/// Sends `session` `guild`'s query answer.
void SendGuildQuery(PlayerSession& session, Guild const& guild);
/// Sends `session` `guild`'s ranks.
void SendGuildRanks(PlayerSession& session, Guild const& guild);
/// Sends `session` `guild`'s event log.
void SendGuildEventLog(PlayerSession& session, Guild const& guild);

/// The guild's notices, each built here and told to the guild's members through the guild's own sends.
class SessionGuildNotices : public GuildNotices
{
    public:
        void Event(Guild& guild, GuildEvents event, ObjectGuid guid, char const* str1, char const* str2, char const* str3) override;
        void Chat(Guild& guild, Player& speaker, std::string const& msg, uint32 language, bool officers) override;
        void AddonChat(Guild& guild, Player& speaker, std::string const& msg, std::string const& prefix, bool officers) override;
        void BankSlotsChanged(Guild const& guild, Player& player, uint8 tabId, std::vector<int32> const& slots, uint32 withdrawRemaining) override;
        void BankTabTextChanged(Guild& guild, uint8 tabId) override;
};

#endif
