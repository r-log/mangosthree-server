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

#include "QuestPackets.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "QuestDef.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <string>

void SessionQuestNotices::QuestLogFull(Player const& player)
{
    WorldPacket data(SMSG_QUESTLOG_FULL, 0);
    player.SendDirectMessage(&data);
}

void SessionQuestNotices::QuestCompleteEvent(Player const& player, uint32 questId)
{
    WorldPacket data(SMSG_QUESTUPDATE_COMPLETE, 4);
    data << uint32(questId);
    player.SendDirectMessage(&data);
}

void SessionQuestNotices::QuestFailed(Player const& player, uint32 questId, InventoryResult reason)
{
    WorldPacket data(SMSG_QUESTGIVER_QUEST_FAILED, 4 + 4);
    data << uint32(questId);
    data << uint32(reason);                                 // failed reason (valid reasons: 4, 16, 50, 17, 74, other values show default message)
    player.SendDirectMessage(&data);
}

void SessionQuestNotices::QuestTimerFailed(Player const& player, uint32 questId)
{
    WorldPacket data(SMSG_QUESTUPDATE_FAILEDTIMER, 4);
    data << uint32(questId);
    player.SendDirectMessage(&data);
}

void SessionQuestNotices::QuestInvalid(Player const& player, uint32 reason)
{
    WorldPacket data(SMSG_QUESTGIVER_QUEST_INVALID, 4);
    data << uint32(reason);
    player.SendDirectMessage(&data);
}

void SessionQuestNotices::QuestConfirmAccept(Player const& receiver, Quest const& quest, ObjectGuid sharer)
{
    int loc_idx = receiver.GetSession()->GetSessionDbLocaleIndex();
    std::string title = quest.GetTitle();
    sObjectMgr.GetQuestLocaleStrings(quest.GetQuestId(), loc_idx, &title);

    WorldPacket data(SMSG_QUEST_CONFIRM_ACCEPT, (4 + title.size() + 8));
    data << uint32(quest.GetQuestId());
    data << title;
    data << sharer;
    receiver.SendDirectMessage(&data);
}

void SessionQuestNotices::QuestPushResult(Player const& player, ObjectGuid target, uint8 result)
{
    WorldPacket data(MSG_QUEST_PUSH_RESULT, (8 + 1));
    data << target;
    data << uint8(result);                                  // valid values: 0-8
    player.SendDirectMessage(&data);
}

void SessionQuestNotices::QuestKillCredited(Player const& player, uint32 questId, uint32 entry, uint32 count, uint32 required, ObjectGuid guid)
{
    WorldPacket data(SMSG_QUESTUPDATE_ADD_KILL, (4 * 4 + 8));
    data << uint32(questId);
    data << uint32(entry);
    data << uint32(count);
    data << uint32(required);
    data << guid;
    player.SendDirectMessage(&data);
}

void SessionQuestNotices::QuestGiverStatuses(Player const& player, std::vector<QuestGiverStatusEntry> const& statuses)
{
    WorldPacket data(SMSG_QUESTGIVER_STATUS_MULTIPLE, 4);
    data << uint32(statuses.size());
    for (std::vector<QuestGiverStatusEntry>::const_iterator itr = statuses.begin(); itr != statuses.end(); ++itr)
    {
        data << itr->guid;
        data << uint32(itr->status);
    }
    player.SendDirectMessage(&data);
}
