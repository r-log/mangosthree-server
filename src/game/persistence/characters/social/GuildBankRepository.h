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

#ifndef MANGOS_H_GUILDBANKREPOSITORY
#define MANGOS_H_GUILDBANKREPOSITORY

#include <string>
#include <vector>
#include "Platform/Define.h"

class QueryResult;

/// One `guild_bank_tab` row.
struct GuildBankTabRow
{
    uint8 tabId = 0;
    std::string tabName;
    std::string tabIcon;
    std::string tabText;
};

/// One `guild_bank_eventlog` row.
struct GuildBankEventLogRow
{
    uint32 logGuid = 0;
    uint8 eventType = 0;
    uint32 playerGuid = 0;
    uint32 itemOrMoney = 0;
    uint8 itemStackCount = 0;
    uint8 destTabId = 0;
    uint64 timeStamp = 0;
};

/// The guild banks' saved rows (`guild_bank_tab`, `guild_bank_item`, `guild_bank_right`,
/// `guild_bank_eventlog`, and the bank columns of `guild` and `guild_member`). The loads run at
/// startup, synchronously; every write is queued on the character database, and one called inside
/// CharacterTransaction joins it.
namespace GuildBankRepository
{
    /// Selects a guild's bank tabs, ordered by tab: tab, name, icon, text.
    std::vector<GuildBankTabRow> QueryTabs(uint32 guildId);

    /// Selects a guild's bank items with their values and text, ordered by tab, for
    /// Item::LoadFromDB (the values first): values, text, tab, slot, item guid, entry.
    QueryResult* QueryItems(uint32 guildId);

    /// Selects a bank tab's latest events, newest first: log guid, type, player, item or money,
    /// stack count, destination tab, time.
    std::vector<GuildBankEventLogRow> QueryEventLog(uint32 guildId, uint32 tabId, uint32 maxLogs);

    /// Writes a newly bought tab's row in one transaction, over any row left under its id.
    void CreateTab(uint32 guildId, uint32 tabId);

    /// Each sets a tab's name and icon, or its text, the strings bound.
    void SetTabInfo(uint32 guildId, uint32 tabId, std::string const& name, std::string const& icon);
    void SetTabText(uint32 guildId, uint32 tabId, std::string const& text);

    /// Puts an item in a bank slot, over the slot's old row.
    void StoreItem(uint32 guildId, uint32 tabId, uint32 slotId, uint32 itemGuid, uint32 itemEntry);

    /// Deletes a bank slot's item row.
    void DeleteItem(uint32 guildId, uint32 tabId, uint32 slotId);

    /// Sets the guild's bank money.
    void SetBankMoney(uint32 guildId, int64 money);

    /// Each sets a member's remaining money, or remaining slots of a tab, for the day.
    void SetMemberRemainingMoney(uint32 guildId, uint32 memberGuid, uint32 money);
    void SetMemberRemainingSlots(uint32 guildId, uint32 memberGuid, uint32 tabId, uint32 slots);

    /// Each starts a member's new day: its reset time and its remaining slots of a tab, or money.
    void ResetMemberSlots(uint32 guildId, uint32 memberGuid, uint32 tabId, uint32 resetTime, uint32 slots);
    void ResetMemberMoney(uint32 guildId, uint32 memberGuid, uint32 resetTime, uint32 money);

    /// Sets a rank's money per day and clears its members' money reset time: two statements.
    void SetRankMoneyPerDay(uint32 guildId, uint32 rankId, uint32 money);

    /// Deletes a rank's bank right of a tab that is not bought or a rank that does not exist.
    void DeleteBadRight(uint32 guildId, uint32 rankId, uint32 tabId);

    /// Writes a rank's bank right of a tab over the old one and clears its members' reset time of
    /// the tab: three statements.
    void SetRight(uint32 guildId, uint32 tabId, uint32 rankId, uint32 right, uint32 slotsPerDay);

    /// Deletes every bank right row of a guild id the load finds no guild for.
    void DeleteOrphanRights(uint32 guildId);

    /// Moves a money event the load found under an item tab to the money tab.
    void MoveEventToMoneyTab(uint32 guildId, uint32 tabId, uint32 logGuid, uint32 moneyTab);

    /// Replaces the event row under a tab's log guid with a new event.
    void DeleteEventLogEntry(uint32 guildId, uint32 logGuid, uint32 tabId);
    void InsertEventLogEntry(uint32 guildId, uint32 logGuid, uint32 tabId, uint32 eventType, uint32 playerGuid, uint32 itemOrMoney, uint32 stackCount, uint32 destTabId, uint64 timeStamp);
}

#endif
