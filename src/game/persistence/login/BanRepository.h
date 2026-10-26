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

#include <optional>
#include <string>
#include <vector>

class QueryResult;

/// A banned account as the ban list reads it: its id, and its name when the list read it.
struct BanListAccountRow
{
    uint32 accountId = 0;
    std::optional<std::string> username;
};

/// A banned account's name, none when it is NULL.
struct BannedAccountNameRow
{
    std::optional<std::string> username;
};

/// One ban of an account: its dates, who banned it and why.
struct AccountBanRow
{
    uint64 banDate = 0;
    uint64 unbanDate = 0;
    std::string bannedBy;
    std::string banReason;
};

/// One ban of an account's history: when it began (as text), how long it lasts, whether it is
/// active, when it ends, why and by whom.
struct AccountBanHistoryRow
{
    std::optional<std::string> banDate;
    uint64 duration = 0;
    bool active = false;
    uint64 unbanDate = 0;
    std::string banReason;
    std::string bannedBy;
};

/// An address's ban: the address, when it began and ends (as text), the time left, why, by whom
/// and how long it lasts.
struct IpBanRow
{
    std::string ip;
    std::optional<std::string> banDate;
    std::optional<std::string> unbanDate;
    uint64 remaining = 0;
    std::string banReason;
    std::string bannedBy;
    uint64 duration = 0;
};

/// An active address ban as the ban list reads it: the address, its dates, by whom and why.
struct ActiveIpBanRow
{
    std::string ip;
    uint64 banDate = 0;
    uint64 unbanDate = 0;
    std::string bannedBy;
    std::string banReason;
};

/// The account and IP bans on the login database (`account_banned`, `ip_banned`), as a GM's ban commands
/// read them, and the bans set and lifted through the world: every read runs on the calling thread,
/// synchronously, a filter escaped there first or by the caller through EscapeString; every write is
/// queued.
namespace BanRepository
{
    /// Selects a banned account's name.
    std::optional<BannedAccountNameRow> QueryBannedAccountName(uint32 accountId);

    /// Selects an account's bans, ordered by their end.
    std::vector<AccountBanRow> QueryAccountBans(uint32 accountId);

    /// Selects an account's ban history, oldest first.
    std::vector<AccountBanHistoryRow> QueryAccountBanHistory(uint32 accountId);

    /// Selects an IP address's ban, the address escaped first.
    std::optional<IpBanRow> QueryIpBan(std::string ip);

    /// Deletes the IP bans that have run out.
    void DeleteExpiredIpBans();

    /// Selects the IP bans still in force, ordered by their end, those whose address contains the
    /// filter when one is given; the filter is escaped first.
    std::vector<ActiveIpBanRow> QueryActiveIpBans(std::string filter);

    /// Selects the banned accounts' ids and names, those whose name contains the filter when one is
    /// given; the filter is escaped first.
    std::vector<BanListAccountRow> QueryActiveAccountBans(std::string filter);

    /// Escapes a text for the login database, as a ban escapes its target, reason and author.
    void EscapeString(std::string& text);

    /// Selects the accounts last seen on an address, the address already escaped.
    QueryResult* QueryAccountsOnIp(std::string const& ip);

    /// Inserts an address's ban, the texts already escaped.
    void InsertIpBan(std::string const& ip, uint32 duration, std::string const& author, std::string const& reason);

    /// Selects an account's id by its name, the name already escaped.
    QueryResult* QueryAccountIdNamed(std::string const& name);

    /// Inserts an account's active ban, the texts already escaped.
    void InsertAccountBan(uint32 accountId, uint32 duration, std::string const& author, std::string const& reason);

    /// Deletes an address's bans, the address escaped first.
    void DeleteIpBan(std::string ip);

    /// Ends an account's bans.
    void DeactivateAccountBans(uint32 accountId);
}

#endif
