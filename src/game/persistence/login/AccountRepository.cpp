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
}
