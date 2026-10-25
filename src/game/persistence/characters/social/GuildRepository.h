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

#ifndef MANGOS_H_GUILDREPOSITORY
#define MANGOS_H_GUILDREPOSITORY

#include <string>
#include "Platform/Define.h"

class QueryResult;

/// A new guild's saved row, in the order of the INSERT's columns.
struct GuildRow
{
    uint32 id = 0;
    std::string name;
    uint32 leaderGuid = 0;
    std::string info;
    std::string motd;
    uint64 createDate = 0;
    uint32 emblemStyle = 0;
    uint32 emblemColor = 0;
    uint32 borderStyle = 0;
    uint32 borderColor = 0;
    uint32 backgroundColor = 0;
    uint64 bankMoney = 0;
};

/// The guilds' saved rows (`guild`, `guild_member`, `guild_rank`, `guild_bank_right`, `guild_eventlog`,
/// and the bank tables a disband deletes). The loads and the highest id run at startup, synchronously; every write is
/// queued on the character database, and one called inside CharacterTransaction joins it.
namespace GuildRepository
{
    /// Selects every guild, ordered by id: id, name, leader, the emblem's five values, info, motd,
    /// creation date, bank money and the number of bank tabs.
    QueryResult* QueryGuilds();

    /// Selects every rank, ordered by guild and rank: guild, rank, name, rights, money per day.
    QueryResult* QueryRanks();

    /// Selects every member, ordered by guild: guild, guid, rank, the two notes, the bank money reset
    /// and remainder, each tab's reset and remaining slots, and the character's name, level, class,
    /// zone, logout time and account.
    QueryResult* QueryMembers();

    /// Selects every bank tab right, ordered by guild and tab: guild, tab, rank, right, slots per day.
    QueryResult* QueryBankRights();

    /// Selects a guild's latest events, newest first: log guid, type, the two players, rank, time.
    QueryResult* QueryEventLog(uint32 guildId, uint32 maxRecords);

    /// Each deletes the event log rows above the configured count, at startup.
    void TrimEventLog(uint32 count);
    void TrimBankEventLog(uint32 count);

    /// Writes a new guild in one transaction: a DELETE of any member rows left under its id, then
    /// the guild's row, the strings bound.
    void Create(GuildRow const& row);

    /// Each sets one column of the guild's row, the strings bound.
    void SetMotd(uint32 guildId, std::string const& motd);
    void SetInfo(uint32 guildId, std::string const& info);
    void SetLeader(uint32 guildId, uint32 leaderGuid);
    void SetEmblem(uint32 guildId, uint32 emblemStyle, uint32 emblemColor, uint32 borderStyle, uint32 borderColor, uint32 backgroundColor);

    /// A disband's two groups of deletes, which Guild::Disband issues around the bank items' own
    /// deletes: the guild, its ranks and its bank tabs; then its bank items, bank rights and both
    /// event logs.
    void DeleteGuildRanksAndTabs(uint32 guildId);
    void DeleteBankAndLogs(uint32 guildId);

    /// Inserts a member's row, the notes bound.
    void InsertMember(uint32 guildId, uint32 memberGuid, uint32 rankId, std::string const& publicNote, std::string const& officerNote);

    /// Deletes a member's row.
    void DeleteMember(uint32 memberGuid);

    /// Deletes every member row of a guild id the load finds no guild for.
    void DeleteOrphanMembers(uint32 guildId);

    /// Each sets one column of a member's row, the notes bound.
    void SetMemberRank(uint32 memberGuid, uint32 rankId);
    void SetMemberPublicNote(uint32 memberGuid, std::string const& note);
    void SetMemberOfficerNote(uint32 memberGuid, std::string const& note);

    /// Deletes every rank of a guild, before its ranks are written again.
    void DeleteRanks(uint32 guildId);

    /// Deletes every rank row of a guild id the load finds no guild for.
    void DeleteOrphanRanks(uint32 guildId);

    /// Deletes every bank right of a guild, before its ranks are written again.
    void DeleteBankRights(uint32 guildId);

    /// Inserts a rank's row, the name bound.
    void InsertRank(uint32 guildId, uint32 rankId, std::string const& name, uint32 rights);

    /// Inserts a rank's bank right for a tab, with no right and no slots.
    void InsertBankRight(uint32 guildId, uint32 tabId, uint32 rankId);

    /// Deletes a rank and its bank rights and moves every higher rank one down, in one transaction.
    void DeleteRank(uint32 guildId, uint32 rankId);

    /// Writes a rank's bank right for a tab over the old one.
    void ReplaceBankRight(uint32 guildId, uint32 tabId, uint32 rankId, uint32 right, uint32 slotsPerDay);

    /// Writes a rank's row over the old one; the name is written into the statement as it is.
    void ReplaceRank(uint32 guildId, uint32 rankId, std::string const& name, uint32 rights, uint32 moneyPerDay);

    /// Each sets one column of a rank's row, the name bound.
    void SetRankName(uint32 guildId, uint32 rankId, std::string const& name);
    void SetRankRights(uint32 guildId, uint32 rankId, uint32 rights);

    /// Replaces the event log row under a log guid with a new event.
    void DeleteEventLogEntry(uint32 guildId, uint32 logGuid);
    void InsertEventLogEntry(uint32 guildId, uint32 logGuid, uint32 eventType, uint32 playerGuid1, uint32 playerGuid2, uint32 newRank, uint64 timeStamp);

    /// Selects the highest guild id, at start-up.
    QueryResult* QueryHighestId();
}

#endif
