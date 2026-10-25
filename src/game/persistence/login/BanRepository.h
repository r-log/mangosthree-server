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

#ifndef MANGOS_H_BANREPOSITORY
#define MANGOS_H_BANREPOSITORY

#include "Platform/Define.h"

#include <string>

class QueryResult;

/// The account and IP bans on the login database (`account_banned`, `ip_banned`), as a GM's ban commands
/// read them: every read runs on the calling thread, synchronously, a filter escaped there first; the
/// expired IP bans' delete is queued.
namespace BanRepository
{
    /// Selects a banned account's name.
    QueryResult* QueryBannedAccountName(uint32 accountId);

    /// Selects an account's bans, ordered by their end.
    QueryResult* QueryAccountBans(uint32 accountId);

    /// Selects an account's ban history, oldest first.
    QueryResult* QueryAccountBanHistory(uint32 accountId);

    /// Selects an IP address's ban, the address escaped first.
    QueryResult* QueryIpBan(std::string ip);

    /// Deletes the IP bans that have run out.
    void DeleteExpiredIpBans();

    /// Selects the IP bans still in force, ordered by their end, those whose address contains the
    /// filter when one is given; the filter is escaped first.
    QueryResult* QueryActiveIpBans(std::string filter);

    /// Selects the banned accounts' ids and names, those whose name contains the filter when one is
    /// given; the filter is escaped first.
    QueryResult* QueryActiveAccountBans(std::string filter);
}

#endif
