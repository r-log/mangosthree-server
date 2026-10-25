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

#include "MailRepository.h"
#include "Database/DatabaseEnv.h"

#include <utility>

namespace MailRepository
{
    void InsertMail(MailRow const& row)
    {
        static SqlStatementID insMail;
        SqlStatement insert = CharacterDatabase.CreateStatement(insMail,
                              "INSERT INTO `mail` (`id`,`messageType`,`stationery`,`mailTemplateId`,`sender`,`receiver`,`subject`,`body`,`has_items`,`expire_time`,`deliver_time`,`money`,`cod`,`checked`) "
                              "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
        insert.addUInt32(row.id);
        insert.addUInt32(row.messageType);
        insert.addUInt32(row.stationery);
        insert.addUInt32(row.mailTemplateId);
        insert.addUInt32(row.sender);
        insert.addUInt32(row.receiver);
        insert.addString(row.subject);
        insert.addString(row.body);
        insert.addUInt32(row.hasItems);
        insert.addUInt64(row.expireTime);
        insert.addUInt64(row.deliverTime);
        insert.addUInt64(row.money);
        insert.addUInt64(row.cod);
        insert.addUInt32(row.checked);
        insert.Execute();
    }

    void InsertMailItem(uint32 mailId, uint32 itemGuid, uint32 itemTemplate, uint32 receiverGuid)
    {
        CharacterDatabase.PExecute("INSERT INTO `mail_items` (`mail_id`,`item_guid`,`item_template`,`receiver`) VALUES ('%u', '%u', '%u','%u')",
                                   mailId, itemGuid, itemTemplate, receiverGuid);
    }

    void SetHasItems(uint32 mailId)
    {
        CharacterDatabase.PExecute("UPDATE `mail` SET `has_items` = 1 WHERE `id` = %u", mailId);
    }

    void DeleteMail(uint32 mailId)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `mail` WHERE `id` = '%u'", mailId);
        CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `mail_id` = '%u'", mailId);
        CharacterDatabase.CommitTransaction();
    }

    void SetMailedItemOwner(uint32 itemGuid, uint32 receiverGuid)
    {
        CharacterDatabase.PExecute("UPDATE `item_instance` SET `owner_guid` = '%u' WHERE `guid`='%u'", receiverGuid, itemGuid);
    }

    void DeleteDroppedItem(uint32 itemGuid)
    {
        CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid`='%u'", itemGuid);
    }

    void QueryMailboxCount(uint32 receiverGuid, std::function<void(QueryResult*)> callback)
    {
        CharacterDatabase.AsyncPQuery(std::move(callback), "SELECT COUNT(*) FROM `mail` WHERE `receiver` = '%u'", receiverGuid);
    }

    void QueryMassMailReceivers(char const* query, std::function<void(QueryResult*)> callback)
    {
        CharacterDatabase.AsyncPQuery(std::move(callback), "%s", query);
    }
}
