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

#include "RealmRepository.h"
#include "Database/DatabaseEnv.h"

namespace RealmRepository
{
    void SetPopulation(float population, uint32 realmId)
    {
        static SqlStatementID id;

        SqlStatement stmt = LoginDatabase.CreateStatement(id, "UPDATE `realmlist` SET `population` = ? WHERE `id` = ?");
        stmt.PExecute(population, realmId);
    }

    void SetTypeAndZone(uint32 type, uint32 zone, uint32 realmId)
    {
        LoginDatabase.PExecute("UPDATE `realmlist` SET `icon` = %u, `timezone` = %u WHERE `id` = '%u'", type, zone, realmId);
    }

    void InsertUptime(uint32 realmId, uint64 startTime, char const* startString)
    {
        LoginDatabase.PExecute("INSERT INTO `uptime` (`realmid`, `starttime`, `startstring`, `uptime`) VALUES('%u', " UI64FMTD ", '%s', 0)",
                               realmId, startTime, startString);
    }

    void SetUptime(uint32 uptime, uint32 maxPlayers, uint32 realmId, uint64 startTime)
    {
        LoginDatabase.PExecute("UPDATE `uptime` SET `uptime` = %u, `maxplayers` = %u WHERE `realmid` = %u AND `starttime` = " UI64FMTD, uptime, maxPlayers, realmId, startTime);
    }

    void SetAllowedSecurityLevel(uint32 level, uint32 realmId)
    {
        LoginDatabase.PExecute("UPDATE `realmlist` SET `allowedSecurityLevel` = '%u' WHERE `id` = '%u'",
                               level, realmId);
    }
}
