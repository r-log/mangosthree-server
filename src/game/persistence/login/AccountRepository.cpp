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

#include "AccountRepository.h"
#include "Database/DatabaseEnv.h"

#include <utility>

namespace AccountRepository
{
    bool Insert(std::string const& username, std::string const& passHash)
    {
        return LoginDatabase.PExecute("INSERT INTO `account` (`username`,`sha_pass_hash`,`joindate`) VALUES ('%s','%s',NOW())", username.c_str(), passHash.c_str());
    }

    bool InsertWithExpansion(std::string const& username, std::string const& passHash, uint32 expansion)
    {
        return LoginDatabase.PExecute("INSERT INTO `account`(`username`,`sha_pass_hash`,`joindate`,`expansion`) VALUES('%s','%s',NOW(),'%u')", username.c_str(), passHash.c_str(), expansion);
    }

    void LinkNewAccountsToRealms()
    {
        LoginDatabase.Execute("INSERT INTO `realmcharacters` (`realmid`, `acctid`, `numchars`) SELECT `realmlist`.`id`, `account`.`id`, 0 FROM `realmlist`,`account` LEFT JOIN `realmcharacters` ON `acctid`=`account`.`id` WHERE `acctid` IS NULL");
    }

    QueryResult* QueryExists(uint32 accountId)
    {
        return LoginDatabase.PQuery("SELECT 1 FROM `account` WHERE `id`='%u'", accountId);
    }

    bool Delete(uint32 accountId)
    {
        LoginDatabase.BeginTransaction();

        bool res =
            LoginDatabase.PExecute("DELETE FROM `account` WHERE `id`='%u'", accountId) &&
            LoginDatabase.PExecute("DELETE FROM `realmcharacters` WHERE `acctid`='%u'", accountId);

        LoginDatabase.CommitTransaction();
        return res;
    }

    bool SetNameAndPasswordHash(std::string const& username, std::string const& passHash, uint32 accountId)
    {
        std::string safe_new_uname = username;
        LoginDatabase.escape_string(safe_new_uname);

        return LoginDatabase.PExecute("UPDATE `account` SET `v`='0',`s`='0',`username`='%s',`sha_pass_hash`='%s' WHERE `id`='%u'", safe_new_uname.c_str(),
                                      passHash.c_str(), accountId);
    }

    bool SetPasswordHash(std::string const& passHash, uint32 accountId)
    {
        return LoginDatabase.PExecute("UPDATE `account` SET `v`='0', `s`='0', `sha_pass_hash`='%s' WHERE `id`='%u'",
                                      passHash.c_str(), accountId);
    }

    QueryResult* QueryIdByName(std::string const& username)
    {
        std::string safeName = username;
        LoginDatabase.escape_string(safeName);
        return LoginDatabase.PQuery("SELECT `id` FROM `account` WHERE `username` = '%s'", safeName.c_str());
    }

    QueryResult* QueryGmLevel(uint32 accountId)
    {
        return LoginDatabase.PQuery("SELECT `gmlevel` FROM `account` WHERE `id` = '%u'", accountId);
    }

    QueryResult* QueryName(uint32 accountId)
    {
        return LoginDatabase.PQuery("SELECT `username` FROM `account` WHERE `id` = '%u'", accountId);
    }

    bool QueryNameAndPasswordHash(uint32 accountId, std::function<void(QueryResult*)> callback)
    {
        return LoginDatabase.AsyncPQuery(std::move(callback), "SELECT `username`, `sha_pass_hash` FROM `account` WHERE `id`='%u'", accountId);
    }

    void RegisterThread()
    {
        // A function-local thread_local runs the guard's constructor the first time a thread gets
        // here and its destructor when that thread exits.
        static thread_local DbThreadGuard guard(&LoginDatabase);
        (void)guard;
    }

    QueryResult* QueryAuthAccount(std::string const& username)
    {
        std::string safeAccount = username;
        LoginDatabase.escape_string(safeAccount);

        return LoginDatabase.PQuery("SELECT "
                                    "`id`, "          // 0
                                    "`gmlevel`, "     // 1
                                    "`sessionkey`, "  // 2
                                    "`last_ip`, "     // 3
                                    "`locked`, "      // 4
                                    "`expansion`, "   // 5
                                    "`mutetime`, "    // 6
                                    "`locale`, "      // 7
                                    "`os` "           // 8
                                    "FROM `account` WHERE `username` = '%s'",
                                    safeAccount.c_str());
    }

    QueryResult* QueryActiveBan(uint32 accountId, std::string const& ip)
    {
        return LoginDatabase.PQuery("SELECT 1 FROM `account_banned` WHERE `id` = %u AND `active` = 1 "
                                    "AND (`unbandate` > UNIX_TIMESTAMP() OR `unbandate` = `bandate`) "
                                    "UNION "
                                    "SELECT 1 FROM `ip_banned` WHERE (`unbandate` = `bandate` OR "
                                    "`unbandate` > UNIX_TIMESTAMP()) AND `ip` = '%s'",
                                    accountId, ip.c_str());
    }

    void SetLastIp(std::string const& ip, std::string const& username)
    {
        static SqlStatementID updAccount;
        SqlStatement stmt = LoginDatabase.CreateStatement(
            updAccount, "UPDATE `account` SET `last_ip` = ? WHERE `username` = ?");
        stmt.PExecute(ip.c_str(), username.c_str());
    }

    void QueryGmLevel(uint32 accountId, std::function<void(QueryResult*)> callback)
    {
        LoginDatabase.AsyncPQuery(std::move(callback), "SELECT `gmlevel` FROM `account` WHERE `id` = '%u'", accountId);
    }

    bool QueryWhois(uint32 accountId, std::function<void(QueryResult*)> callback)
    {
        return LoginDatabase.AsyncPQuery(std::move(callback), "SELECT `username`,`email`,`last_ip` FROM `account` WHERE `id`=%u", accountId);
    }

    void SetActiveRealm(uint32 realmId, uint32 accountId)
    {
        static SqlStatementID updAccount;
        SqlStatement stmt = LoginDatabase.CreateStatement(updAccount, "UPDATE `account` SET `active_realm_id` = ? WHERE `id` = ?");
        stmt.PExecute(realmId, accountId);
    }

    void SetRealmCharacterCount(uint32 accountId, uint32 realmId, uint8 count)
    {
        LoginDatabase.PExecute("DELETE FROM `realmcharacters` WHERE `acctid`= '%u' AND `realmid`= '%u'", accountId, realmId);
        LoginDatabase.PExecute("INSERT INTO `realmcharacters` (`numchars`, `acctid`, `realmid`) VALUES (%u, %u, %u)",  count, accountId, realmId);
    }

    void Lock(uint32 accountId)
    {
        LoginDatabase.PExecute("UPDATE `account` SET `locked` = '1' WHERE `id` = '%u'", accountId);
    }

    void Unlock(uint32 accountId)
    {
        LoginDatabase.PExecute("UPDATE `account` SET `locked` = '0' WHERE `id` = '%u'", accountId);
    }

    QueryResult* QueryActiveOnRealm(uint32 realmId)
    {
        //                                  0   1         2        3        4
        return LoginDatabase.PQuery("SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `active_realm_id` = %u", realmId);
    }

    void SetExpansion(uint32 expansion, uint32 accountId)
    {
        LoginDatabase.PExecute("UPDATE `account` SET `expansion` = '%u' WHERE `id` = '%u'", expansion, accountId);
    }

    void SetGmLevel(int32 gmLevel, uint32 accountId)
    {
        LoginDatabase.PExecute("UPDATE `account` SET `gmlevel` = '%i' WHERE `id` = '%u'", gmLevel, accountId);
    }

    void SetMuteTime(uint64 muteTime, uint32 accountId)
    {
        LoginDatabase.PExecute("UPDATE `account` SET `mutetime` = " UI64FMTD " WHERE `id` = '%u'", muteTime, accountId);
    }

    void ClearMuteTime(uint32 accountId)
    {
        LoginDatabase.PExecute("UPDATE `account` SET `mutetime` = '0' WHERE `id` = '%u'", accountId);
    }
}
