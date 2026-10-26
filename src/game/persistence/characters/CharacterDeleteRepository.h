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

#ifndef MANGOS_H_CHARACTERDELETEREPOSITORY
#define MANGOS_H_CHARACTERDELETEREPOSITORY

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Platform/Define.h"
#include "persistence/characters/inventory/ItemRepository.h"

class QueryResult;
class SqlQueryHolder;

/**
 * @brief The slots of the character-delete holder.
 *
 * The first two are read for every delete method; the last four only for a full delete, the only
 * one that returns mail, deletes pets and cleans friend lists. PLAYER_DELETE_READ_OWNER is the
 * delete request's ownership read, staged into the same holder; nothing else uses it, which is why
 * the count above it stops at PLAYER_DELETE_READ_COUNT.
 */
enum PlayerDeleteReadSlot
{
    PLAYER_DELETE_READ_GROUP                = 0,
    PLAYER_DELETE_READ_PETITION_SIGNS       = 1,
    PLAYER_DELETE_READ_COD_MAIL             = 2,
    PLAYER_DELETE_READ_COD_MAIL_ITEMS       = 3,
    PLAYER_DELETE_READ_PETS                 = 4,
    PLAYER_DELETE_READ_FRIENDS              = 5,
    PLAYER_DELETE_READ_COUNT                = 6,

    PLAYER_DELETE_READ_OWNER                = 6,
    PLAYER_DELETE_READ_WITH_OWNER_COUNT     = 7
};

/// One COD mail with items the delete returns: its id, type, template, sender, subject, body,
/// money and whether it has items.
struct DeleteCodMailRow
{
    uint32 id = 0;
    uint16 messageType = 0;
    uint16 mailTemplateId = 0;
    uint32 sender = 0;
    std::string subject;
    std::string body;
    uint32 money = 0;
    bool hasItems = false;
};

/// One item of a COD mail the delete returns: its instance, guid, template and mail.
struct DeleteCodMailItemRow
{
    ItemInstanceRow instance;
    uint32 itemGuid = 0;
    uint32 itemTemplate = 0;
    uint32 mailId = 0;
};

/// A character's delete as the database sees it: the reads the delete needs, every row it deletes,
/// the soft delete that keeps the row, and the purge of soft-deleted characters. Every read is
/// asynchronous, and the answered group, pet, friend and COD mail slots are read into values and
/// rows; every write is queued on the character database, and one called inside
/// CharacterTransaction joins it.
namespace CharacterDeleteRepository
{
    /// Stages the delete's reads into a holder sized to at least PLAYER_DELETE_READ_COUNT: the
    /// group and the petition signatures always, and for a full delete the COD mails with items,
    /// those mails' items (ordered by mail and item), the pets and the friend list's owners.
    void StageDeleteReads(SqlQueryHolder* holder, uint32 guid, bool fullDelete);

    /// Asks, in one holder, for the delete's reads, and with withOwner also for the character's
    /// account and name. A holder the database refuses is deleted and its callback never runs.
    void QueueDeleteReads(uint32 guid, bool fullDelete, bool withOwner,
                          std::function<void(QueryResult*, SqlQueryHolder*)> callback);

    /// Deletes every row a full delete removes, in order: the character, its own tables, the
    /// instance binds it leads, its items, mail, pets, achievements, equipment sets, its lines in
    /// both guild event logs and its currencies.
    void DeleteEveryRowOf(uint32 guid);

    /// Unlinks a character from its account and frees its name, keeping both and the time in the
    /// row's delete columns.
    void MarkDeleted(uint32 guid, uint64 deleteTime);

    /// Asks for the soft-deleted characters deleted before a time: guid and the account they had.
    void QueryDeletedBefore(uint64 time, std::function<void(QueryResult*)> callback);

    /// Reads the delete holder's group id, none when the character is in no group.
    std::optional<uint32> DeleteGroupId(SqlQueryHolder* holder);

    /// Reads the delete holder's pet ids.
    std::vector<uint32> DeletePetIds(SqlQueryHolder* holder);

    /// Reads the delete holder's guids of the characters that list the character.
    std::vector<uint32> DeleteFriendGuids(SqlQueryHolder* holder);

    /// Reads the delete holder's COD mails, ordered by id.
    std::vector<DeleteCodMailRow> DeleteCodMails(SqlQueryHolder* holder);

    /// Reads the delete holder's COD mail items, ordered by mail.
    std::vector<DeleteCodMailItemRow> DeleteCodMailItems(SqlQueryHolder* holder);
}

#endif
