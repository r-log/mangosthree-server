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

#include "GuildBankRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace GuildBankRepository
{
    static GuildBankTabRow ReadGuildBankTab(Field* fields)
    {
        GuildBankTabRow row;
        row.tabId = fields[0].GetUInt8();
        row.tabName = fields[1].GetCppString();
        row.tabIcon = fields[2].GetCppString();
        row.tabText = fields[3].GetCppString();
        return row;
    }

    std::vector<GuildBankTabRow> QueryTabs(uint32 guildId)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT `TabId`, `TabName`, `TabIcon`, `TabText` FROM `guild_bank_tab` WHERE `guildid`='%u' ORDER BY `TabId`", guildId);
        return ReadRows<GuildBankTabRow>(result, ReadGuildBankTab);
    }

    QueryResult* QueryItems(uint32 guildId)
    {
        return CharacterDatabase.PQuery("SELECT `data`, `text`, `TabId`, `SlotId`, `item_guid`, `item_entry` FROM `guild_bank_item` JOIN `item_instance` ON `item_guid` = `guid` WHERE `guildid`='%u' ORDER BY `TabId`", guildId);
    }

    static GuildBankEventLogRow ReadGuildBankEventLog(Field* fields)
    {
        GuildBankEventLogRow row;
        row.logGuid = fields[0].GetUInt32();
        row.eventType = fields[1].GetUInt8();
        row.playerGuid = fields[2].GetUInt32();
        row.itemOrMoney = fields[3].GetUInt32();
        row.itemStackCount = fields[4].GetUInt8();
        row.destTabId = fields[5].GetUInt8();
        row.timeStamp = fields[6].GetUInt64();
        return row;
    }

    std::vector<GuildBankEventLogRow> QueryEventLog(uint32 guildId, uint32 tabId, uint32 maxLogs)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT `LogGuid`, `EventType`, `PlayerGuid`, `ItemOrMoney`, `ItemStackCount`, `DestTabId`, `TimeStamp` FROM `guild_bank_eventlog` WHERE `guildid`='%u' AND `TabId`='%u' ORDER BY `TimeStamp` DESC,`LogGuid` DESC LIMIT %u", guildId, tabId, maxLogs);
        return ReadRows<GuildBankEventLogRow>(result, ReadGuildBankEventLog);
    }

    void CreateTab(uint32 guildId, uint32 tabId)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_tab` WHERE `guildid`='%u' AND `TabId`='%u'", guildId, tabId);
        CharacterDatabase.PExecute("INSERT INTO `guild_bank_tab` (`guildid`,`TabId`) VALUES ('%u','%u')", guildId, tabId);
        CharacterDatabase.CommitTransaction();
    }

    void SetTabInfo(uint32 guildId, uint32 tabId, std::string const& name, std::string const& icon)
    {
        static SqlStatementID updTabInfo;
        SqlStatement update = CharacterDatabase.CreateStatement(updTabInfo,
                              "UPDATE `guild_bank_tab` SET `TabName`=?,`TabIcon`=? WHERE `guildid`=? AND `TabId`=?");
        update.addString(name);
        update.addString(icon);
        update.addUInt32(guildId);
        update.addUInt32(tabId);
        update.Execute();
    }

    void SetTabText(uint32 guildId, uint32 tabId, std::string const& text)
    {
        static SqlStatementID updTabText;
        SqlStatement update = CharacterDatabase.CreateStatement(updTabText,
                              "UPDATE `guild_bank_tab` SET `TabText`=? WHERE `guildid`=? AND `TabId`=?");
        update.addString(text);
        update.addUInt32(guildId);
        update.addUInt32(tabId);
        update.Execute();
    }

    void StoreItem(uint32 guildId, uint32 tabId, uint32 slotId, uint32 itemGuid, uint32 itemEntry)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_item` WHERE `guildid` = '%u' AND `TabId` = '%u'AND `SlotId` = '%u'", guildId, tabId, slotId);
        CharacterDatabase.PExecute("INSERT INTO `guild_bank_item` (`guildid`,`TabId`,`SlotId`,`item_guid`,`item_entry`) "
                                   "VALUES ('%u', '%u', '%u', '%u', '%u')", guildId, tabId, slotId, itemGuid, itemEntry);
    }

    void DeleteItem(uint32 guildId, uint32 tabId, uint32 slotId)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_item` WHERE `guildid`='%u' AND `TabId`='%u' AND `SlotId`='%u'", guildId, tabId, slotId);
    }

    void SetBankMoney(uint32 guildId, int64 money)
    {
        CharacterDatabase.PExecute("UPDATE `guild` SET `BankMoney`='" UI64FMTD "' WHERE `guildid`='%u'", money, guildId);
    }

    void SetMemberRemainingMoney(uint32 guildId, uint32 memberGuid, uint32 money)
    {
        CharacterDatabase.PExecute("UPDATE `guild_member` SET `BankRemMoney`='%u' WHERE `guildid`='%u' AND `guid`='%u'",
                                   money, guildId, memberGuid);
    }

    void SetMemberRemainingSlots(uint32 guildId, uint32 memberGuid, uint32 tabId, uint32 slots)
    {
        CharacterDatabase.PExecute("UPDATE `guild_member` SET `BankRemSlotsTab%u`='%u' WHERE `guildid`='%u' AND `guid`='%u'",
                                   tabId, slots, guildId, memberGuid);
    }

    void ResetMemberSlots(uint32 guildId, uint32 memberGuid, uint32 tabId, uint32 resetTime, uint32 slots)
    {
        CharacterDatabase.PExecute("UPDATE `guild_member` SET `BankResetTimeTab%u`='%u', `BankRemSlotsTab%u`='%u' WHERE `guildid`='%u' AND `guid`='%u'",
                                   tabId, resetTime, tabId, slots, guildId, memberGuid);
    }

    void ResetMemberMoney(uint32 guildId, uint32 memberGuid, uint32 resetTime, uint32 money)
    {
        CharacterDatabase.PExecute("UPDATE `guild_member` SET `BankResetTimeMoney`='%u', `BankRemMoney`='%u' WHERE `guildid`='%u' AND `guid`='%u'",
                                   resetTime, money, guildId, memberGuid);
    }

    void SetRankMoneyPerDay(uint32 guildId, uint32 rankId, uint32 money)
    {
        CharacterDatabase.PExecute("UPDATE `guild_rank` SET `BankMoneyPerDay`='%u' WHERE `rid`='%u' AND `guildid`='%u'", money, rankId, guildId);
        CharacterDatabase.PExecute("UPDATE `guild_member` SET `BankResetTimeMoney`='0' WHERE `guildid`='%u' AND `rank`='%u'", guildId, rankId);
    }

    void DeleteBadRight(uint32 guildId, uint32 rankId, uint32 tabId)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_right` WHERE `guildid`='%u' AND `rid`='%u' AND `TabId`='%u'", guildId, rankId, tabId);
    }

    void SetRight(uint32 guildId, uint32 tabId, uint32 rankId, uint32 right, uint32 slotsPerDay)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_right` WHERE `guildid`='%u' AND `TabId`='%u' AND `rid`='%u'", guildId, tabId, rankId);
        CharacterDatabase.PExecute("INSERT INTO `guild_bank_right` (`guildid`,`TabId`,`rid`,`gbright`,`SlotPerDay`) VALUES "
                                   "('%u','%u','%u','%u','%u')", guildId, tabId, rankId, right, slotsPerDay);
        CharacterDatabase.PExecute("UPDATE `guild_member` SET `BankResetTimeTab%u`='0' WHERE `guildid`='%u' AND `rank`='%u'", tabId, guildId, rankId);
    }

    void DeleteOrphanRights(uint32 guildId)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_right` WHERE `guildid` = '%u'", guildId);
    }

    void MoveEventToMoneyTab(uint32 guildId, uint32 tabId, uint32 logGuid, uint32 moneyTab)
    {
        CharacterDatabase.PExecute("UPDATE `guild_bank_eventlog` SET `TabId`='%u' WHERE `guildid`='%u' AND `TabId`='%u' AND `LogGuid`='%u'", moneyTab, guildId, tabId, logGuid);
    }

    void DeleteEventLogEntry(uint32 guildId, uint32 logGuid, uint32 tabId)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_eventlog` WHERE `guildid`='%u' AND `LogGuid`='%u' AND `TabId`='%u'", guildId, logGuid, tabId);
    }

    void InsertEventLogEntry(uint32 guildId, uint32 logGuid, uint32 tabId, uint32 eventType, uint32 playerGuid, uint32 itemOrMoney, uint32 stackCount, uint32 destTabId, uint64 timeStamp)
    {
        CharacterDatabase.PExecute("INSERT INTO `guild_bank_eventlog` (`guildid`,`LogGuid`,`TabId`,`EventType`,`PlayerGuid`,`ItemOrMoney`,`ItemStackCount`,`DestTabId`,`TimeStamp`) VALUES ('%u','%u','%u','%u','%u','%u','%u','%u','" UI64FMTD "')",
                                   guildId, logGuid, tabId, eventType, playerGuid, itemOrMoney, stackCount, destTabId, timeStamp);
    }
}
