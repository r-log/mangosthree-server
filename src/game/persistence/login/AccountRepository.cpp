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
    void QueryGmLevel(uint32 accountId, std::function<void(QueryResult*)> callback)
    {
        LoginDatabase.AsyncPQuery(std::move(callback), "SELECT `gmlevel` FROM `account` WHERE `id` = '%u'", accountId);
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
