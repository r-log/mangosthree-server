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

#include "CharacterDeleteRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"
#include "Database/SqlOperations.h"

#include <utility>

namespace CharacterDeleteRepository
{
    void StageDeleteReads(SqlQueryHolder* holder, uint32 guid, bool fullDelete)
    {
        holder->SetPQuery(PLAYER_DELETE_READ_GROUP,
                          "SELECT `groupId` FROM `group_member` WHERE `memberGuid`='%u'", guid);

        holder->SetPQuery(PLAYER_DELETE_READ_PETITION_SIGNS,
                          "SELECT `ownerguid`,`petitionguid` FROM `petition_sign` WHERE `playerguid` = '%u'", guid);

        if (!fullDelete)
        {
            return;
        }

        //                                                 0    1             2                3        4         5      6       7
        holder->SetPQuery(PLAYER_DELETE_READ_COD_MAIL,
                          "SELECT `id`,`messageType`,`mailTemplateId`,`sender`,`subject`,`body`,`money`,`has_items` FROM `mail` WHERE `receiver`='%u' AND `has_items`<>0 AND `cod`<>0 ORDER BY `id`", guid);

        // The items of all those mails in one statement, ordered by mail and then by item, so the
        // delete walks it as a cursor beside the mails.
        //                                                 0                        1                          2                         3                              4
        holder->SetPQuery(PLAYER_DELETE_READ_COD_MAIL_ITEMS,
                          "SELECT `item_instance`.`data`,`item_instance`.`text`,`mail_items`.`item_guid`,`mail_items`.`item_template`,`mail_items`.`mail_id` "
                          "FROM `mail_items` JOIN `item_instance` ON `mail_items`.`item_guid` = `item_instance`.`guid` "
                          "JOIN `mail` ON `mail_items`.`mail_id` = `mail`.`id` "
                          "WHERE `mail`.`receiver`='%u' AND `mail`.`has_items`<>0 AND `mail`.`cod`<>0 "
                          "ORDER BY `mail_items`.`mail_id`, `mail_items`.`item_guid`", guid);

        holder->SetPQuery(PLAYER_DELETE_READ_PETS,
                          "SELECT `id` FROM `character_pet` WHERE `owner` = '%u'", guid);

        holder->SetPQuery(PLAYER_DELETE_READ_FRIENDS,
                          "SELECT DISTINCT `guid` FROM `character_social` WHERE `friend` = '%u'", guid);
    }

    void QueueDeleteReads(uint32 guid, bool fullDelete, bool withOwner,
                          std::function<void(QueryResult*, SqlQueryHolder*)> callback)
    {
        SqlQueryHolder* holder = new SqlQueryHolder;
        holder->SetSize(withOwner ? PLAYER_DELETE_READ_WITH_OWNER_COUNT : PLAYER_DELETE_READ_COUNT);
        if (withOwner)
        {
            holder->SetPQuery(PLAYER_DELETE_READ_OWNER,
                              "SELECT `account`,`name` FROM `characters` WHERE `guid`='%u'", guid);
        }
        StageDeleteReads(holder, guid, fullDelete);

        if (!CharacterDatabase.DelayQueryHolder(std::move(callback), holder))
        {
            delete holder;                                  // delete all unprocessed queries
        }
    }

    void DeleteEveryRowOf(uint32 guid)
    {
        CharacterDatabase.PExecute("DELETE FROM `characters` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_account_data` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_declinedname` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_action` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_aura` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_battleground_data` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_gifts` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_glyphs` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_homebind` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `group_instance` WHERE `leaderGuid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_queststatus` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_queststatus_daily` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_queststatus_weekly` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_reputation` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_skills` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_spell` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_spell_cooldown` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_talent` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_ticket` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `owner_guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_social` WHERE `guid` = '%u' OR `friend`='%u'", guid, guid);
        CharacterDatabase.PExecute("DELETE FROM `mail` WHERE `receiver` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `receiver` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_pet` WHERE `owner` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_pet_declinedname` WHERE `owner` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_achievement` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_achievement_progress` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_equipmentsets` WHERE `guid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `guild_eventlog` WHERE `PlayerGuid1` = '%u' OR `PlayerGuid2` = '%u'", guid, guid);
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_eventlog` WHERE `PlayerGuid` = '%u'", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_currencies` WHERE `guid` = '%u'", guid);
    }

    void MarkDeleted(uint32 guid, uint64 deleteTime)
    {
        CharacterDatabase.PExecute("UPDATE `characters` SET `deleteInfos_Name`=`name`, `deleteInfos_Account`=`account`, `deleteDate`='" UI64FMTD "', `name`='', `account`=0 WHERE `guid`=%u", deleteTime, guid);
    }

    void QueryDeletedBefore(uint64 time, std::function<void(QueryResult*)> callback)
    {
        CharacterDatabase.AsyncPQuery(std::move(callback),
                                      "SELECT `guid`, `deleteInfos_Account` FROM `characters` WHERE `deleteDate` IS NOT NULL AND `deleteDate` < '" UI64FMTD "'",
                                      time);
    }

    std::optional<uint32> DeleteGroupId(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_DELETE_READ_GROUP);
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    std::vector<uint32> DeletePetIds(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_DELETE_READ_PETS);
        return ReadRows<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    std::vector<uint32> DeleteFriendGuids(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_DELETE_READ_FRIENDS);
        return ReadRows<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    static DeleteCodMailRow ReadDeleteCodMail(Field* fields)
    {
        DeleteCodMailRow row;
        row.id = fields[0].GetUInt32();
        row.messageType = fields[1].GetUInt16();
        row.mailTemplateId = fields[2].GetUInt16();
        row.sender = fields[3].GetUInt32();
        row.subject = fields[4].GetCppString();
        row.body = fields[5].GetCppString();
        row.money = fields[6].GetUInt32();
        row.hasItems = fields[7].GetBool();
        return row;
    }

    std::vector<DeleteCodMailRow> DeleteCodMails(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_DELETE_READ_COD_MAIL);
        return ReadRows<DeleteCodMailRow>(result, ReadDeleteCodMail);
    }

    static DeleteCodMailItemRow ReadDeleteCodMailItem(Field* fields)
    {
        DeleteCodMailItemRow row;
        row.instance = ItemRepository::ReadInstance(fields);
        row.itemGuid = fields[2].GetUInt32();
        row.itemTemplate = fields[3].GetUInt32();
        row.mailId = fields[4].GetUInt32();
        return row;
    }

    std::vector<DeleteCodMailItemRow> DeleteCodMailItems(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PLAYER_DELETE_READ_COD_MAIL_ITEMS);
        return ReadRows<DeleteCodMailItemRow>(result, ReadDeleteCodMailItem);
    }
}
