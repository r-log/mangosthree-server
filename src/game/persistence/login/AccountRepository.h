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

#ifndef MANGOS_H_ACCOUNTREPOSITORY
#define MANGOS_H_ACCOUNTREPOSITORY

#include <functional>
#include <string>
#include "Platform/Define.h"

class QueryResult;

/// The accounts' saved rows on the login database (`account`, `account_banned`, `ip_banned`,
/// `realmcharacters`). The authentication reads run on the network thread that asks them,
/// synchronously; every write is queued.
namespace AccountRepository
{
    /// Registers the calling thread with the database client library, once per thread; a thread
    /// that reads the login database outside the database's own threads calls it first.
    void RegisterThread();

    /// Selects the account a client authenticates as, by name, the name escaped: id, GM level,
    /// session key, last IP, the IP lock, expansion, mute time, locale and client OS.
    QueryResult* QueryAuthAccount(std::string const& username);

    /// Selects a row when the account or the IP has an active ban.
    QueryResult* QueryActiveBan(uint32 accountId, std::string const& ip);

    /// Records the IP an account last authenticated from, both strings bound.
    void SetLastIp(std::string const& ip, std::string const& username);

    /// Asks, asynchronously, for an account's GM level; the callback runs on the world thread with
    /// the result, which it owns.
    void QueryGmLevel(uint32 accountId, std::function<void(QueryResult*)> callback);

    /// Asks, asynchronously, for what a whois answers: an account's name, email and last IP; the
    /// callback runs on the world thread with the result, which it owns. Returns false, and never
    /// runs the callback, when the database is shutting down.
    bool QueryWhois(uint32 accountId, std::function<void(QueryResult*)> callback);

    /// Sets the realm an account is playing on.
    void SetActiveRealm(uint32 realmId, uint32 accountId);

    /// Writes the account's character count on a realm over the old one.
    void SetRealmCharacterCount(uint32 accountId, uint32 realmId, uint8 count);
}

#endif
