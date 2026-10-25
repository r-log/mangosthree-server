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

#ifndef MANGOS_H_MAILREPOSITORY
#define MANGOS_H_MAILREPOSITORY

#include <functional>
#include <string>
#include <vector>
#include "Platform/Define.h"

class QueryResult;

/// A mail's saved row, in the order of the INSERT's columns.
struct MailRow
{
    uint32 id = 0;
    uint32 messageType = 0;
    uint32 stationery = 0;
    uint32 mailTemplateId = 0;
    uint32 sender = 0;
    uint32 receiver = 0;
    std::string subject;
    std::string body;
    uint32 hasItems = 0;
    uint64 expireTime = 0;
    uint64 deliverTime = 0;
    uint64 money = 0;
    uint64 cod = 0;
    uint32 checked = 0;
};

/// The mails' saved rows (`mail`, `mail_items`) and the item instance writes of the mail flow.
/// Every write is queued on the character database; a write called inside CharacterTransaction
/// joins that transaction. The two reads are asynchronous: the callback runs on the world thread
/// with the result, which it owns.
namespace MailRepository
{
    /// Inserts a mail's row, the subject and body bound.
    void InsertMail(MailRow const& row);

    /// Inserts one item row of a mail.
    void InsertMailItem(uint32 mailId, uint32 itemGuid, uint32 itemTemplate, uint32 receiverGuid);

    /// Marks a mail as holding items.
    void SetHasItems(uint32 mailId);

    /// Deletes a mail's row and its item rows in one transaction.
    void DeleteMail(uint32 mailId);

    /// Each deletes one part of a mail, as a character's delete does when it returns or drops
    /// the character's COD mails: the mail's row, and its item rows.
    void DeleteMailRow(uint32 mailId);
    void DeleteMailItems(uint32 mailId);

    /// Writes a changed mail's state, as the receiver's save does.
    void UpdateState(uint32 mailId, uint32 hasItems, uint64 expireTime, uint64 deliverTime, uint32 money, uint32 cod, uint32 checked);

    /// Deletes the item rows of the items taken out of a mail, through one statement.
    void DeleteTakenItems(std::vector<uint32> const& itemGuids);

    /// Deletes a mail its receiver deleted: its row, then its item rows.
    void DeleteDeletedMail(uint32 mailId);

    /// Deletes the item row of a mailed item the receiver's load drops.
    void DropMailedItem(uint32 itemGuid);

    /// Gives a mailed item's instance row to the mail's receiver.
    void SetMailedItemOwner(uint32 itemGuid, uint32 receiverGuid);

    /// Deletes the instance row of an item a mail drops.
    void DeleteDroppedItem(uint32 itemGuid);

    /// Asks for the number of mails a receiver holds.
    void QueryMailboxCount(uint32 receiverGuid, std::function<void(QueryResult*)> callback);

    /// Runs a mass mail's receiver query, whose first column is each receiver's guid.
    void QueryMassMailReceivers(char const* query, std::function<void(QueryResult*)> callback);
}

#endif
