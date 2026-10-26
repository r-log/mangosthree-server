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

#include "CalendarRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace CalendarRepository
{
    static CalendarEventRow ReadEvent(Field* fields)
    {
        CalendarEventRow row;
        row.eventId     = fields[0].GetUInt64();
        row.creatorGuid = fields[1].GetUInt32();
        row.guildId     = fields[2].GetUInt32();
        row.type        = fields[3].GetUInt8();
        row.flags       = fields[4].GetUInt32();
        row.dungeonId   = fields[5].GetInt32();
        row.eventTime   = fields[6].GetUInt32();
        row.title       = fields[7].GetCppString();
        row.description = fields[8].GetCppString();
        return row;
    }

    std::vector<CalendarEventRow> QueryEvents()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT `eventId`, `creatorGuid`, `guildId`, `type`, `flags`, `dungeonId`, `eventTime`, `title`, `description` FROM `calendar_events` ORDER BY `eventId`");
        return ReadRows<CalendarEventRow>(result, ReadEvent);
    }

    static CalendarInviteRow ReadInvite(Field* fields)
    {
        CalendarInviteRow row;
        row.inviteId       = fields[0].GetUInt64();
        row.eventId        = fields[1].GetUInt64();
        row.inviteeGuid    = fields[2].GetUInt32();
        row.senderGuid     = fields[3].GetUInt32();
        row.status         = fields[4].GetUInt8();
        row.lastUpdateTime = fields[5].GetUInt32();
        row.rank           = fields[6].GetUInt8();
        return row;
    }

    std::vector<CalendarInviteRow> QueryInvites()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT `inviteId`, `eventId`, `inviteeGuid`, `senderGuid`, `status`, `lastUpdateTime`, `rank` FROM `calendar_invites` ORDER BY `inviteId`");
        return ReadRows<CalendarInviteRow>(result, ReadInvite);
    }

    void TruncateEvents()
    {
        CharacterDatabase.DirectExecute("TRUNCATE TABLE calendar_events");
    }

    void TruncateInvites()
    {
        CharacterDatabase.DirectExecute("TRUNCATE TABLE calendar_invites");
    }

    void InsertEvent(CalendarEventRow const& row)
    {
        static SqlStatementID insEvent;
        SqlStatement insert = CharacterDatabase.CreateStatement(insEvent,
                              "INSERT INTO `calendar_events` (`eventId`,`creatorGuid`,`guildId`,`type`,`flags`,`dungeonId`,`eventTime`,`title`,`description`) "
                              "VALUES(?,?,?,?,?,?,?,?,?)");
        insert.addUInt64(row.eventId);
        insert.addUInt32(row.creatorGuid);
        insert.addUInt32(row.guildId);
        insert.addUInt32(row.type);
        insert.addUInt32(row.flags);
        insert.addInt32(row.dungeonId);
        insert.addUInt32(row.eventTime);
        insert.addString(row.title);
        insert.addString(row.description);
        insert.Execute();
    }

    void UpdateEvent(CalendarEventRow const& row)
    {
        static SqlStatementID updEvent;
        SqlStatement update = CharacterDatabase.CreateStatement(updEvent,
                              "UPDATE `calendar_events` SET "
                              "`type`=?, `flags`=?, `dungeonId`=?, `eventTime`=?, `title`=?, `description`=? "
                              "WHERE `eventId`=?");
        update.addUInt32(row.type);
        update.addUInt32(row.flags);
        update.addInt32(row.dungeonId);
        update.addUInt32(row.eventTime);
        update.addString(row.title);
        update.addString(row.description);
        update.addUInt64(row.eventId);
        update.Execute();
    }

    void DeleteEvent(uint64 eventId)
    {
        CharacterDatabase.PExecute("DELETE FROM `calendar_events` WHERE `eventId` = " UI64FMTD, eventId);
    }

    void InsertInvite(uint64 inviteId, uint64 eventId, uint32 inviteeGuid, uint32 senderGuid, uint32 status, uint32 statusTime, uint32 rank)
    {
        CharacterDatabase.PExecute("INSERT INTO `calendar_invites` VALUES (" UI64FMTD ", " UI64FMTD ", %u, %u, %u, %u, %u)",
                                   inviteId,
                                   eventId,
                                   inviteeGuid,
                                   senderGuid,
                                   status,
                                   statusTime,
                                   rank);
    }

    void DeleteInvite(uint64 inviteId)
    {
        CharacterDatabase.PExecute("DELETE FROM `calendar_invites` WHERE `inviteId` = " UI64FMTD, inviteId);
    }

    void SetInviteReply(uint64 inviteId, uint32 status, uint32 lastUpdateTime)
    {
        CharacterDatabase.PExecute("UPDATE `calendar_invites` SET `status`=%u, `lastUpdateTime`=%u WHERE `inviteId` = " UI64FMTD , status, lastUpdateTime, inviteId);
    }

    void SetInviteStatus(uint64 inviteId, uint32 status, uint32 lastUpdateTime)
    {
        CharacterDatabase.PExecute("UPDATE `calendar_invites` SET `status`=%u, `lastUpdateTime`=%u WHERE `inviteId`=" UI64FMTD, status, lastUpdateTime, inviteId);
    }

    void SetInviteRank(uint64 inviteId, uint32 rank)
    {
        CharacterDatabase.PExecute("UPDATE `calendar_invites` SET `rank` = %u WHERE `inviteId` = " UI64FMTD, rank, inviteId);
    }
}
