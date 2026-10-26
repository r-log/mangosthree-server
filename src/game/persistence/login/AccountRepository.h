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
/// `realmcharacters`). The authentication reads run on the network thread that asks them, and the
/// account manager's and the GM commands' reads on the thread that calls them, synchronously; every
/// write is queued, and
/// one that returns bool answers false when the database refuses it.
namespace AccountRepository
{
    /// Inserts a new account, joined now; the name and the hash are written as they are.
    bool Insert(std::string const& username, std::string const& passHash);

    /// Inserts a new account with an expansion, joined now; the name and the hash are written as
    /// they are.
    bool InsertWithExpansion(std::string const& username, std::string const& passHash, uint32 expansion);

    /// Gives every account without a `realmcharacters` row a row of no characters on every realm.
    void LinkNewAccountsToRealms();

    /// Selects a row when the account exists.
    QueryResult* QueryExists(uint32 accountId);

    /// Deletes an account's row and its `realmcharacters` rows, in one transaction; the second
    /// delete is issued only when the first was queued.
    bool Delete(uint32 accountId);

    /// Sets an account's name and password hash and clears its SRP6 values, the name escaped.
    bool SetNameAndPasswordHash(std::string const& username, std::string const& passHash, uint32 accountId);

    /// Sets an account's password hash and clears its SRP6 values.
    bool SetPasswordHash(std::string const& passHash, uint32 accountId);

    /// Selects an account's id by name, the name escaped.
    QueryResult* QueryIdByName(std::string const& username);

    /// Selects an account's GM level.
    QueryResult* QueryGmLevel(uint32 accountId);

    /// Selects an account's name.
    QueryResult* QueryName(uint32 accountId);

    /// Asks, asynchronously, for an account's name and password hash; the callback runs on the world
    /// thread with the result, which it owns. Returns false, and never runs the callback, when the
    /// database is shutting down.
    bool QueryNameAndPasswordHash(uint32 accountId, std::function<void(QueryResult*)> callback);

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

    /// Locks an account to its last address.
    void Lock(uint32 accountId);

    /// Unlocks an account from its last address.
    void Unlock(uint32 accountId);

    /// Selects the accounts playing on a realm, for a GM's online list.
    QueryResult* QueryActiveOnRealm(uint32 realmId);

    /// Sets an account's expansion.
    void SetExpansion(uint32 expansion, uint32 accountId);

    /// Sets an account's GM level.
    void SetGmLevel(int32 gmLevel, uint32 accountId);

    /// Sets the time an account's mute ends.
    void SetMuteTime(uint64 muteTime, uint32 accountId);

    /// Ends an account's mute.
    void ClearMuteTime(uint32 accountId);

    /// Selects the accounts whose email contains a text, the text escaped first, for a GM's lookup.
    QueryResult* QueryAccountsByEmail(std::string email);

    /// Selects the accounts whose last address contains a text, the text escaped first.
    QueryResult* QueryAccountsByLastIp(std::string ip);

    /// Selects the accounts whose name contains a text, the text escaped first.
    QueryResult* QueryAccountsByName(std::string account);

    /// Selects the ids and names of the accounts whose last address contains a text, the text escaped
    /// first.
    QueryResult* QueryAccountIdsByLastIp(std::string ip);

    /// Selects the ids and names of the accounts whose name contains a text, the text escaped first.
    QueryResult* QueryAccountIdsByName(std::string account);

    /// Selects the ids and names of the accounts whose email contains a text, the text escaped first.
    QueryResult* QueryAccountIdsByEmail(std::string email);

    /// Selects an account's name, GM level, last address and last login, for a GM's player info.
    QueryResult* QueryAccountInfo(uint32 accountId);

    /// Selects the names and GM levels of the game master accounts.
    QueryResult* QueryGameMasters();

    /// Rewrites an account's character count on a realm, in one transaction.
    void ReplaceRealmCharacterCount(uint32 accountId, uint32 realmId, uint32 count);

    /// Clears the active realm of every account playing on a realm, at start-up and shutdown.
    void ClearActiveRealm(uint32 realmId);
}

#endif
