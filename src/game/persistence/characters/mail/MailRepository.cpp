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
#include "persistence/RowReader.h"
#include "Database/SqlOperations.h"

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
        DeleteMailRow(mailId);
        DeleteMailItems(mailId);
        CharacterDatabase.CommitTransaction();
    }

    void DeleteMailRow(uint32 mailId)
    {
        CharacterDatabase.PExecute("DELETE FROM `mail` WHERE `id` = '%u'", mailId);
    }

    void DeleteMailItems(uint32 mailId)
    {
        CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `mail_id` = '%u'", mailId);
    }

    void UpdateState(uint32 mailId, uint32 hasItems, uint64 expireTime, uint64 deliverTime, uint32 money, uint32 cod, uint32 checked)
    {
        static SqlStatementID updateMail ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updateMail, "UPDATE `mail` SET `has_items` = ?, `expire_time` = ?, `deliver_time` = ?, `money` = ?, `cod` = ?, `checked` = ? WHERE `id` = ?");
        stmt.addUInt32(hasItems);
        stmt.addUInt64(expireTime);
        stmt.addUInt64(deliverTime);
        stmt.addUInt32(money);
        stmt.addUInt32(cod);
        stmt.addUInt32(checked);
        stmt.addUInt32(mailId);
        stmt.Execute();
    }

    void DeleteTakenItems(std::vector<uint32> const& itemGuids)
    {
        static SqlStatementID deleteMailItems ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(deleteMailItems, "DELETE FROM `mail_items` WHERE `item_guid` = ?");

        for (std::vector<uint32>::const_iterator itr = itemGuids.begin(); itr != itemGuids.end(); ++itr)
        {
            stmt.PExecute(*itr);
        }
    }

    void DeleteDeletedMail(uint32 mailId)
    {
        static SqlStatementID deleteMain ;
        static SqlStatementID deleteItems ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(deleteMain, "DELETE FROM `mail` WHERE `id` = ?");
        stmt.PExecute(mailId);

        stmt = CharacterDatabase.CreateStatement(deleteItems, "DELETE FROM `mail_items` WHERE `mail_id` = ?");
        stmt.PExecute(mailId);
    }

    void DropMailedItem(uint32 itemGuid)
    {
        CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `item_guid` = '%u'", itemGuid);
    }

    void SetMailedItemOwner(uint32 itemGuid, uint32 receiverGuid)
    {
        CharacterDatabase.PExecute("UPDATE `item_instance` SET `owner_guid` = '%u' WHERE `guid`='%u'", receiverGuid, itemGuid);
    }

    void DeleteDroppedItem(uint32 itemGuid)
    {
        CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid`='%u'", itemGuid);
    }

    void QueryMailboxCount(uint32 receiverGuid, std::function<void(std::optional<uint32>)> callback)
    {
        CharacterDatabase.AsyncPQuery([callback = std::move(callback)](QueryResult* result)
                                      {
                                          callback(ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); }));
                                      },
                                      "SELECT COUNT(*) FROM `mail` WHERE `receiver` = '%u'", receiverGuid);
    }

    void QueryMassMailReceivers(char const* query, std::function<void(std::vector<uint32>)> callback)
    {
        CharacterDatabase.AsyncPQuery([callback = std::move(callback)](QueryResult* result)
                                      {
                                          callback(ReadRows<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); }));
                                      },
                                      "%s", query);
    }

    void DeleteEmptyExpiredMails(uint64 before)
    {
        CharacterDatabase.PExecute("DELETE FROM `mail` WHERE `expire_time` < '" UI64FMTD "' AND `has_items` = '0' AND `body` = ''", before);
    }

    void QueueExpiredMailReads(uint64 before, std::function<void(QueryResult*, SqlQueryHolder*)> callback)
    {
        SqlQueryHolder* holder = new SqlQueryHolder;
        holder->SetSize(EXPIRED_MAIL_COUNT);
        //                                       0     1              2         3           4            5              6      7          8
        holder->SetPQuery(EXPIRED_MAIL_ROWS, "SELECT `id`,`messageType`,`sender`,`receiver`,`has_items`,`expire_time`,`cod`,`checked`,`mailTemplateId` FROM `mail` WHERE `expire_time` < '" UI64FMTD "' ORDER BY `id`", before);
        //                                        0                        1                            2
        holder->SetPQuery(EXPIRED_MAIL_ITEMS, "SELECT `mail_items`.`item_guid`,`mail_items`.`item_template`,`mail_items`.`mail_id` "
                          "FROM `mail_items` JOIN `mail` ON `mail_items`.`mail_id` = `mail`.`id` "
                          "WHERE `mail`.`expire_time` < '" UI64FMTD "' AND `mail`.`has_items` <> 0 "
                          "ORDER BY `mail_items`.`mail_id`, `mail_items`.`item_guid`", before);

        if (!CharacterDatabase.DelayQueryHolder(std::move(callback), holder))
        {
            delete holder;                                  // the database is shutting down
        }
    }

    void DeleteExpiredMailItem(uint32 itemGuid)
    {
        CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid` = '%u'", itemGuid);
    }

    void ReturnExpiredMail(uint32 sender, uint32 receiver, uint64 expireTime, uint64 deliverTime, uint32 checked, uint32 mailId)
    {
        CharacterDatabase.PExecute("UPDATE `mail` SET `sender` = '%u', `receiver` = '%u', `expire_time` = '" UI64FMTD "', `deliver_time` = '" UI64FMTD "', `cod` = '0', `checked` = '%u' WHERE `id` = '%u'",
                                   sender, receiver, expireTime, deliverTime, checked, mailId);
    }

    void SetReturnedItemReceiver(uint32 receiver, uint32 itemGuid)
    {
        CharacterDatabase.PExecute("UPDATE `mail_items` SET `receiver` = %u WHERE `item_guid` = '%u'", receiver, itemGuid);
    }

    void SetReturnedItemOwner(uint32 owner, uint32 itemGuid)
    {
        CharacterDatabase.PExecute("UPDATE `item_instance` SET `owner_guid` = %u WHERE `guid` = '%u'", owner, itemGuid);
    }

    std::optional<uint32> QueryHighestId()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT MAX(`id`) FROM `mail`");
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    static ExpiredMailRow ReadExpiredMail(Field* fields)
    {
        ExpiredMailRow row;
        row.id = fields[0].GetUInt32();
        row.messageType = fields[1].GetUInt8();
        row.sender = fields[2].GetUInt32();
        row.receiver = fields[3].GetUInt32();
        row.hasItems = fields[4].GetBool();
        row.expireTime = fields[5].GetUInt64();
        row.cod = fields[6].GetUInt32();
        row.checked = fields[7].GetUInt32();
        row.mailTemplateId = fields[8].GetInt16();
        return row;
    }

    std::vector<ExpiredMailRow> ExpiredMails(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(EXPIRED_MAIL_ROWS);
        return ReadRows<ExpiredMailRow>(result, ReadExpiredMail);
    }

    static ExpiredMailItemRow ReadExpiredMailItem(Field* fields)
    {
        ExpiredMailItemRow row;
        row.itemGuid = fields[0].GetUInt32();
        row.itemTemplate = fields[1].GetUInt32();
        row.mailId = fields[2].GetUInt32();
        return row;
    }

    std::vector<ExpiredMailItemRow> ExpiredMailItems(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(EXPIRED_MAIL_ITEMS);
        return ReadRows<ExpiredMailItemRow>(result, ReadExpiredMailItem);
    }
}
