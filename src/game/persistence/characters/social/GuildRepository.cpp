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

#include "GuildRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace GuildRepository
{
    QueryResult* QueryGuilds()
    {
        return CharacterDatabase.Query("SELECT `guild`.`guildid`,`guild`.`name`,`leaderguid`,`EmblemStyle`,`EmblemColor`,`BorderStyle`,`BorderColor`,"
                                       "`BackgroundColor`,`info`,`motd`,`createdate`,`BankMoney`,(SELECT COUNT(`guild_bank_tab`.`guildid`) FROM `guild_bank_tab` WHERE `guild_bank_tab`.`guildid` = `guild`.`guildid`) "
                                       "FROM `guild` ORDER BY `guildid` ASC");
    }

    QueryResult* QueryRanks()
    {
        return CharacterDatabase.Query("SELECT `guildid`,`rid`,`rname`,`rights`,`BankMoneyPerDay` FROM `guild_rank` ORDER BY `guildid` ASC, `rid` ASC");
    }

    QueryResult* QueryMembers()
    {
        return CharacterDatabase.Query("SELECT `guildid`,`guild_member`.`guid`,`rank`,`pnote`,`offnote`,`BankResetTimeMoney`,`BankRemMoney`,"
                                       "`BankResetTimeTab0`,`BankRemSlotsTab0`,`BankResetTimeTab1`,`BankRemSlotsTab1`,`BankResetTimeTab2`,`BankRemSlotsTab2`,"
                                       "`BankResetTimeTab3`,`BankRemSlotsTab3`,`BankResetTimeTab4`,`BankRemSlotsTab4`,`BankResetTimeTab5`,`BankRemSlotsTab5`,"
                                       "characters.name, characters.level, characters.class, characters.zone, characters.logout_time, characters.account "
                                       "FROM guild_member LEFT JOIN characters ON characters.guid = guild_member.guid ORDER BY guildid ASC");
    }

    QueryResult* QueryBankRights()
    {
        return CharacterDatabase.Query("SELECT `guildid`,`TabId`,`rid`,`gbright`,`SlotPerDay` FROM `guild_bank_right` ORDER BY `guildid` ASC, `TabId` ASC");
    }

    static GuildEventLogRow ReadGuildEventLog(Field* fields)
    {
        GuildEventLogRow row;
        row.logGuid = fields[0].GetUInt32();
        row.eventType = fields[1].GetUInt8();
        row.playerGuid1 = fields[2].GetUInt32();
        row.playerGuid2 = fields[3].GetUInt32();
        row.newRank = fields[4].GetUInt8();
        row.timeStamp = fields[5].GetUInt64();
        return row;
    }

    std::vector<GuildEventLogRow> QueryEventLog(uint32 guildId, uint32 maxRecords)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT `LogGuid`, `EventType`, `PlayerGuid1`, `PlayerGuid2`, `NewRank`, `TimeStamp` FROM `guild_eventlog` WHERE `guildid`=%u ORDER BY `TimeStamp` DESC,`LogGuid` DESC LIMIT %u", guildId, maxRecords);
        return ReadRows<GuildEventLogRow>(result, ReadGuildEventLog);
    }

    void TrimEventLog(uint32 count)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild_eventlog` WHERE `LogGuid` > '%u'", count);
    }

    void TrimBankEventLog(uint32 count)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_eventlog` WHERE `LogGuid` > '%u'", count);
    }

    void Create(GuildRow const& row)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `guild_member` WHERE `guildid`='%u'", row.id);

        static SqlStatementID insGuild;
        SqlStatement insert = CharacterDatabase.CreateStatement(insGuild,
                              "INSERT INTO `guild` (`guildid`,`name`,`leaderguid`,`info`,`motd`,`createdate`,`EmblemStyle`,`EmblemColor`,`BorderStyle`,`BorderColor`,`BackgroundColor`,`BankMoney`) "
                              "VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");
        insert.addUInt32(row.id);
        insert.addString(row.name);
        insert.addUInt32(row.leaderGuid);
        insert.addString(row.info);
        insert.addString(row.motd);
        insert.addUInt64(row.createDate);
        insert.addUInt32(row.emblemStyle);
        insert.addUInt32(row.emblemColor);
        insert.addUInt32(row.borderStyle);
        insert.addUInt32(row.borderColor);
        insert.addUInt32(row.backgroundColor);
        insert.addUInt64(row.bankMoney);
        insert.Execute();
        CharacterDatabase.CommitTransaction();
    }

    void SetMotd(uint32 guildId, std::string const& motd)
    {
        static SqlStatementID updMotd;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updMotd,
                            "UPDATE `guild` SET `motd` = ? WHERE `guildid` = ?");
        stmt.PExecute(motd.c_str(), guildId);
    }

    void SetInfo(uint32 guildId, std::string const& info)
    {
        static SqlStatementID updGinfo;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updGinfo,
                            "UPDATE `guild` SET `info` = ? WHERE `guildid` = ?");
        stmt.PExecute(info.c_str(), guildId);
    }

    void SetLeader(uint32 guildId, uint32 leaderGuid)
    {
        CharacterDatabase.PExecute("UPDATE `guild` SET `leaderguid`='%u' WHERE `guildid`='%u'", leaderGuid, guildId);
    }

    void SetEmblem(uint32 guildId, uint32 emblemStyle, uint32 emblemColor, uint32 borderStyle, uint32 borderColor, uint32 backgroundColor)
    {
        CharacterDatabase.PExecute("UPDATE `guild` SET `EmblemStyle`=%u, `EmblemColor`=%u, `BorderStyle`=%u, `BorderColor`=%u, `BackgroundColor`=%u WHERE `guildid` = %u", emblemStyle, emblemColor, borderStyle, borderColor, backgroundColor, guildId);
    }

    void DeleteGuildRanksAndTabs(uint32 guildId)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild` WHERE `guildid` = '%u'", guildId);
        CharacterDatabase.PExecute("DELETE FROM `guild_rank` WHERE `guildid` = '%u'", guildId);
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_tab` WHERE `guildid` = '%u'", guildId);
    }

    void DeleteBankAndLogs(uint32 guildId)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_item` WHERE `guildid` = '%u'", guildId);
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_right` WHERE `guildid` = '%u'", guildId);
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_eventlog` WHERE `guildid` = '%u'", guildId);
        CharacterDatabase.PExecute("DELETE FROM `guild_eventlog` WHERE `guildid` = '%u'", guildId);
    }

    void InsertMember(uint32 guildId, uint32 memberGuid, uint32 rankId, std::string const& publicNote, std::string const& officerNote)
    {
        static SqlStatementID insMember;
        SqlStatement insert = CharacterDatabase.CreateStatement(insMember,
                              "INSERT INTO `guild_member` (`guildid`,`guid`,`rank`,`pnote`,`offnote`) VALUES (?,?,?,?,?)");
        insert.addUInt32(guildId);
        insert.addUInt32(memberGuid);
        insert.addUInt32(rankId);
        insert.addString(publicNote);
        insert.addString(officerNote);
        insert.Execute();
    }

    void DeleteMember(uint32 memberGuid)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild_member` WHERE `guid` = '%u'", memberGuid);
    }

    void DeleteOrphanMembers(uint32 guildId)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild_member` WHERE `guildid` = '%u'", guildId);
    }

    void SetMemberRank(uint32 memberGuid, uint32 rankId)
    {
        CharacterDatabase.PExecute("UPDATE `guild_member` SET `rank`='%u' WHERE `guid`='%u'", rankId, memberGuid);
    }

    void SetMemberPublicNote(uint32 memberGuid, std::string const& note)
    {
        static SqlStatementID updPnote;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updPnote,
                            "UPDATE `guild_member` SET `pnote` = ? WHERE `guid` = ?");
        stmt.PExecute(note.c_str(), memberGuid);
    }

    void SetMemberOfficerNote(uint32 memberGuid, std::string const& note)
    {
        static SqlStatementID updOffnote;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updOffnote,
                            "UPDATE `guild_member` SET `offnote` = ? WHERE `guid` = ?");
        stmt.PExecute(note.c_str(), memberGuid);
    }

    void DeleteRanks(uint32 guildId)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild_rank` WHERE `guildid`='%u'", guildId);
    }

    void DeleteOrphanRanks(uint32 guildId)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild_rank` WHERE `guildid` = '%u'", guildId);
    }

    void DeleteBankRights(uint32 guildId)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_right` WHERE `guildid` = '%u'", guildId);
    }

    void InsertRank(uint32 guildId, uint32 rankId, std::string const& name, uint32 rights)
    {
        static SqlStatementID insRank;
        SqlStatement insert = CharacterDatabase.CreateStatement(insRank,
                              "INSERT INTO `guild_rank` (`guildid`,`rid`,`rname`,`rights`) VALUES (?,?,?,?)");
        insert.addUInt32(guildId);
        insert.addUInt32(rankId);
        insert.addString(name);
        insert.addUInt32(rights);
        insert.Execute();
    }

    void InsertBankRight(uint32 guildId, uint32 tabId, uint32 rankId)
    {
        CharacterDatabase.PExecute("INSERT INTO `guild_bank_right` (`guildid`,`TabId`,`rid`) VALUES ('%u','%u','%u')", guildId, tabId, rankId);
    }

    void DeleteRank(uint32 guildId, uint32 rankId)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `guild_rank` WHERE `rid` ='%u' AND `guildid` ='%u'", rankId, guildId);
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_right` WHERE `rid` ='%u' AND `guildid` ='%u'", rankId, guildId);
        CharacterDatabase.PExecute("UPDATE `guild_rank` SET `rid` = `rid` - 1 WHERE `rid` > '%u' AND `guildid` ='%u'", rankId, guildId);
        CharacterDatabase.PExecute("UPDATE `guild_bank_right` SET `rid` = `rid` - 1 WHERE `rid` > '%u' AND `guildid` ='%u'", rankId, guildId);
        CharacterDatabase.CommitTransaction();
    }

    void ReplaceBankRight(uint32 guildId, uint32 tabId, uint32 rankId, uint32 right, uint32 slotsPerDay)
    {
        CharacterDatabase.PExecute("REPLACE INTO guild_bank_right (guildid,TabId,rid,gbright,SlotPerDay) "
            "VALUES ('%u','%u','%u','%u','%u')", guildId, tabId, rankId, right, slotsPerDay);
    }

    void ReplaceRank(uint32 guildId, uint32 rankId, std::string const& name, uint32 rights, uint32 moneyPerDay)
    {
        CharacterDatabase.PExecute("REPLACE INTO guild_rank (guildid,rid,rname,rights,BankMoneyPerDay) "
            "VALUES ('%u', '%u', '%s', '%u', '%u')", guildId, rankId, name.c_str(), rights, moneyPerDay);
    }

    void SetRankName(uint32 guildId, uint32 rankId, std::string const& name)
    {
        static SqlStatementID updRankName;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updRankName,
                            "UPDATE `guild_rank` SET `rname` = ? WHERE `rid` = ? AND `guildid` = ?");
        stmt.addString(name);
        stmt.addUInt32(rankId);
        stmt.addUInt32(guildId);
        stmt.Execute();
    }

    void SetRankRights(uint32 guildId, uint32 rankId, uint32 rights)
    {
        CharacterDatabase.PExecute("UPDATE `guild_rank` SET `rights`='%u' WHERE `rid`='%u' AND `guildid`='%u'", rights, rankId, guildId);
    }

    void DeleteEventLogEntry(uint32 guildId, uint32 logGuid)
    {
        CharacterDatabase.PExecute("DELETE FROM `guild_eventlog` WHERE `guildid`='%u' AND `LogGuid`='%u'", guildId, logGuid);
    }

    void InsertEventLogEntry(uint32 guildId, uint32 logGuid, uint32 eventType, uint32 playerGuid1, uint32 playerGuid2, uint32 newRank, uint64 timeStamp)
    {
        CharacterDatabase.PExecute("INSERT INTO `guild_eventlog` (`guildid`, `LogGuid`, `EventType`, `PlayerGuid1`, `PlayerGuid2`, `NewRank`, `TimeStamp`) VALUES ('%u','%u','%u','%u','%u','%u','" UI64FMTD "')",
                                   guildId, logGuid, eventType, playerGuid1, playerGuid2, newRank, timeStamp);
    }

    std::optional<uint32> QueryHighestId()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT MAX(`guildid`) FROM `guild`");
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    static GuildMemberRankRow ReadGuildMemberRank(Field* fields)
    {
        GuildMemberRankRow row;
        row.guid = fields[0].GetUInt32();
        row.guildid = fields[1].GetUInt32();
        row.rank = fields[2].GetUInt32();
        return row;
    }

    std::vector<GuildMemberRankRow> QueryMemberRanks()
    {
        //                                              0       1          2
        QueryResult* result = CharacterDatabase.Query("SELECT `guid`, `guildid`, `rank` FROM `guild_member`");
        return ReadRows<GuildMemberRankRow>(result, ReadGuildMemberRank);
    }
}
