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

#ifndef MANGOS_H_CHARACTERREPOSITORY
#define MANGOS_H_CHARACTERREPOSITORY

#include <functional>
#include <string>
#include "Platform/Define.h"

class QueryResult;
class SqlQueryHolder;

/// The slots of the login holder, one per table the character's load reads.
enum PlayerLoginQueryIndex
{
    PLAYER_LOGIN_QUERY_LOADFROM,
    PLAYER_LOGIN_QUERY_LOADGROUP,
    PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES,
    PLAYER_LOGIN_QUERY_LOADAURAS,
    PLAYER_LOGIN_QUERY_LOADSPELLS,
    PLAYER_LOGIN_QUERY_LOADQUESTSTATUS,
    PLAYER_LOGIN_QUERY_LOADDAILYQUESTSTATUS,
    PLAYER_LOGIN_QUERY_LOADREPUTATION,
    PLAYER_LOGIN_QUERY_LOADINVENTORY,
    PLAYER_LOGIN_QUERY_LOADITEMLOOT,
    PLAYER_LOGIN_QUERY_LOADACTIONS,
    PLAYER_LOGIN_QUERY_LOADSOCIALLIST,
    PLAYER_LOGIN_QUERY_LOADHOMEBIND,
    PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS,
    PLAYER_LOGIN_QUERY_LOADDECLINEDNAMES,
    PLAYER_LOGIN_QUERY_LOADGUILD,
    PLAYER_LOGIN_QUERY_LOADARENAINFO,
    PLAYER_LOGIN_QUERY_LOADACHIEVEMENTS,
    PLAYER_LOGIN_QUERY_LOADCRITERIAPROGRESS,
    PLAYER_LOGIN_QUERY_LOADEQUIPMENTSETS,
    PLAYER_LOGIN_QUERY_LOADBGDATA,
    PLAYER_LOGIN_QUERY_LOADACCOUNTDATA,
    PLAYER_LOGIN_QUERY_LOADSKILLS,
    PLAYER_LOGIN_QUERY_LOADGLYPHS,
    PLAYER_LOGIN_QUERY_LOADMAILS,
    PLAYER_LOGIN_QUERY_LOADMAILEDITEMS,
    PLAYER_LOGIN_QUERY_LOADTALENTS,
    PLAYER_LOGIN_QUERY_LOADWEEKLYQUESTSTATUS,
    PLAYER_LOGIN_QUERY_LOADMONTHLYQUESTSTATUS,
    PLAYER_LOGIN_QUERY_LOADCURRENCIES,
    PLAYER_LOGIN_QUERY_LOADCUFPROFILES,
    PLAYER_LOGIN_QUERY_LOADPETS,
    PLAYER_LOGIN_QUERY_LOADPETAURAS,
    PLAYER_LOGIN_QUERY_LOADPETSPELLS,
    PLAYER_LOGIN_QUERY_LOADPETSPELLCOOLDOWNS,
    PLAYER_LOGIN_QUERY_LOADPETDECLINEDNAMES,

    MAX_PLAYER_LOGIN_QUERY
};

/// The slot of the create request's login database holder: how many characters the account has
/// across every realm.
enum CharCreateAccountSlot
{
    CHAR_CREATE_ACCOUNT_REALM_CHARS = 0,
    CHAR_CREATE_ACCOUNT_COUNT       = 1
};

/// The slots of the create request's character database holder: the account's character count on
/// this realm, the account's characters (level, race, class) and the highest pet id.
enum CharCreateRealmSlot
{
    CHAR_CREATE_REALM_CHAR_COUNT    = 0,
    CHAR_CREATE_REALM_ACCOUNT_CHARS = 1,
    CHAR_CREATE_REALM_PET_ID        = 2,
    CHAR_CREATE_REALM_COUNT         = 3
};

/// The slots of the customize request's holder: the character's at_login flags and its
/// playerBytes2.
enum CharCustomizeSlot
{
    CHAR_CUSTOMIZE_AT_LOGIN      = 0,
    CHAR_CUSTOMIZE_PLAYER_BYTES2 = 1,
    CHAR_CUSTOMIZE_COUNT         = 2
};

/// The number of declined forms a character name has.
enum
{
    CHARACTER_DECLINED_NAME_ROW_CASES = 5
};

/// The callbacks of the asynchronous reads: they run on the world thread with what the reads
/// returned, which they own.
typedef std::function<void(QueryResult*)> CharacterReadCallback;
typedef std::function<void(QueryResult*, SqlQueryHolder*)> CharacterHolderCallback;

/// The characters' own rows (`characters`, `character_declinedname`) as the character screen, the
/// login, the logout and the account manager use them. The account manager's two reads and the
/// start-up's highest guid run on the thread that calls them, synchronously; every other read is asynchronous, and every write is queued
/// on the character database; a write called inside CharacterTransaction joins it.
namespace CharacterRepository
{
    /// Selects the guids of an account's characters.
    QueryResult* QueryAccountCharacters(uint32 accountId);

    /// Selects how many characters an account has on this realm.
    QueryResult* QueryAccountCharacterCount(uint32 accountId);

    /// Selects an offline character's position, map and flight path, synchronously.
    QueryResult* QueryPosition(uint32 guid);

    /// Each sets or clears one of a character's at_login flags.
    void SetAtLoginFlag(uint32 flag, uint32 guid);
    void ClearAtLoginFlag(uint32 flag, uint32 guid);

    /// Sets a character's zone.
    void SetZone(uint32 zone, uint32 guid);

    /// Writes the map a new character's phase data names over the old row.
    void SetPhaseMap(uint32 guid, uint32 map);

    /// Sets a character's gender and appearance bytes, as a customize does.
    void SetAppearance(uint32 gender, uint32 playerBytes, uint32 playerBytes2, uint32 guid);

    /// Asks for the account's characters, ordered by guid, with each one's pet in currentPetSlot
    /// and guild, and its genitive name when declinedNames is set.
    void QueryEnum(uint32 accountId, uint32 currentPetSlot, bool declinedNames, CharacterReadCallback callback);

    /// Asks the login database, in one holder, how many characters the account has across every
    /// realm. A holder the database refuses is deleted and its callback never runs.
    void QueueCreateAccountRead(uint32 accountId, CharacterHolderCallback callback);

    /// Asks, in one holder, for the account's character count, its characters (only the first when
    /// everyCharacter is clear) and the highest pet id. A holder the database refuses is deleted and
    /// its callback never runs.
    void QueueCreateRealmReads(uint32 accountId, bool everyCharacter, CharacterHolderCallback callback);

    /// Stages every read the login of a character needs into a holder, sized to
    /// MAX_PLAYER_LOGIN_QUERY; the declined names' slot stays empty unless declinedNames is set.
    /// Returns false when a read could not be staged.
    bool StageLoginReads(SqlQueryHolder* holder, uint32 guid, bool declinedNames);

    /// Queues the login holder. Returns false when the database refuses it; the holder is then
    /// the caller's to delete.
    bool QueueLoginReads(SqlQueryHolder* holder, CharacterHolderCallback callback);

    /// Marks a character online.
    void SetOnline(uint32 guid);

    /// Marks every character of an account offline.
    void SetAccountOffline(uint32 accountId);

    /// Asks for what a name query answers: a character's guid, name, race, gender and class, and
    /// its five declined names when declinedNames is set.
    void QueryName(uint32 guid, bool declinedNames, CharacterReadCallback callback);

    /// Asks for a character's guid and name when it belongs to the account and has renameFlag
    /// set in at_login.
    void QueryRenameTarget(uint32 guid, uint32 accountId, uint32 renameFlag, CharacterReadCallback callback);

    /// Sets a character's name, the name bound, clears atLoginFlag and deletes its declined names.
    void Rename(uint32 guid, std::string const& name, uint32 atLoginFlag);

    /// Replaces a character's declined names in one transaction, the forms bound.
    void SetDeclinedNames(uint32 guid, std::string const (&names)[CHARACTER_DECLINED_NAME_ROW_CASES]);

    /// Asks, in one holder, for a character's at_login flags and its playerBytes2. A holder the
    /// database refuses is deleted and its callback never runs.
    void QueueCustomizeReads(uint32 guid, CharacterHolderCallback callback);

    /// Sets the character-screen slot of one of the account's characters.
    void SetSlot(uint8 slot, uint32 guid, uint32 accountId);

    /// Selects the highest character guid, at start-up.
    QueryResult* QueryHighestGuid();
}

#endif
