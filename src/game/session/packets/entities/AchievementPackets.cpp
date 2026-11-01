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

#include "AchievementPackets.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "Util.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "data/Language.h"
#include "session/packets/chat/LocalizedPackets.h"
#include "session/packets/chat/MessageChatPacket.h"

#include <cstring>
#include <ctime>

namespace MaNGOS
{
    class AchievementChatBuilder
    {
        public:
            AchievementChatBuilder(Player const& pl, ChatMsg msgtype, int32 textId, uint32 ach_id)
                : i_player(pl), i_msgtype(msgtype), i_textId(textId), i_achievementId(ach_id) {}
            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* text = sObjectMgr.GetMangosString(i_textId, loc_idx);

                BuildMessageChatPacket(data, i_msgtype, text, LANG_UNIVERSAL, i_player.GetChatTag(),  i_player.GetObjectGuid(), NULL, i_player.GetObjectGuid(), NULL, NULL,
                    i_achievementId);
            }

        private:
            Player const& i_player;
            ChatMsg i_msgtype;
            int32 i_textId;
            uint32 i_achievementId;
    };
}                                                           // namespace MaNGOS

void BuildAchievementEarnedPacket(WorldPacket& packet, Player const& earner, uint32 achievementId)
{
    packet.Initialize(SMSG_ACHIEVEMENT_EARNED, 8 + 4 + 8);
    packet << earner.GetPackGUID();
    packet << uint32(achievementId);
    packet << uint32(secsToTimeBitFields(time(NULL)));
    packet << uint32(0);
}

void BuildCriteriaUpdatePacket(WorldPacket& packet, Player const& player, uint32 criteriaId, CriteriaProgress const& progress)
{
    packet.Initialize(SMSG_CRITERIA_UPDATE, 8 + 4 + 8);
    packet << uint32(criteriaId);

    time_t now = time(NULL);
    // the counter is packed like a packed Guid
    packet.appendPackGUID(progress.counter);

    packet << player.GetPackGUID();
    packet << uint32(progress.timedCriteriaFailed ? 1 : 0);
    packet << uint32(secsToTimeBitFields(now));
    packet << uint32(now - progress.date);                   // timer 1
    packet << uint32(now - progress.date);                   // timer 2
}

void BuildAchievementDeletedPacket(WorldPacket& packet, uint32 achievementId)
{
    packet.Initialize(SMSG_ACHIEVEMENT_DELETED, 4);
    packet << uint32(achievementId);
}

void BuildCriteriaDeletedPacket(WorldPacket& packet, uint32 criteriaId)
{
    packet.Initialize(SMSG_CRITERIA_DELETED, 4);
    packet << uint32(criteriaId);
}

void BuildAllAchievementDataPacket(WorldPacket& packet, Player const& player)
{
    AchievementMgr const& achievements = player.GetAchievementMgr();
    // since we don't know the exact size of the packed GUIDs this is just an approximation
    packet.Initialize(SMSG_ALL_ACHIEVEMENT_DATA, 4 * 2 + achievements.GetCompletedAchievements().size() * 4 * 2 + achievements.GetCompletedAchievements().size() * 7 * 4);

    ObjectGuid guid = player.GetObjectGuid();

    packet.WriteBits(achievements.GetCriteriaProgress().size(), 21);

    for (CriteriaProgressMap::const_iterator iter = achievements.GetCriteriaProgress().begin(); iter!=achievements.GetCriteriaProgress().end(); ++iter)
    {
        ObjectGuid counter = ObjectGuid(uint64(iter->second.counter));

        packet.WriteGuidMask<4>(guid);
        packet.WriteGuidMask<3>(counter);
        packet.WriteGuidMask<5>(guid);
        packet.WriteGuidMask<0, 6>(counter);
        packet.WriteGuidMask<3, 0>(guid);

        packet.WriteGuidMask<4>(counter);
        packet.WriteGuidMask<2>(guid);
        packet.WriteGuidMask<7>(counter);
        packet.WriteGuidMask<7>(guid);
        uint8 flags = 0;                                        // Seems always 0
        packet.WriteBits(flags, 2);
        packet.WriteGuidMask<6>(guid);

        packet.WriteGuidMask<2, 1, 5>(counter);
        packet.WriteGuidMask<1>(guid);
    }

    packet.WriteBits(achievements.GetCompletedAchievements().size(), 23);

    time_t now = time(NULL);
    for (CriteriaProgressMap::const_iterator iter = achievements.GetCriteriaProgress().begin(); iter != achievements.GetCriteriaProgress().end(); ++iter)
    {
        ObjectGuid counter = ObjectGuid(uint64(iter->second.counter));

        packet.WriteGuidBytes<3>(guid);
        packet.WriteGuidBytes<5, 6>(counter);
        packet.WriteGuidBytes<4, 6>(guid);
        packet.WriteGuidBytes<2>(counter);

        packet << uint32(now - iter->second.date);               // Timer 2

        packet.WriteGuidBytes<2>(guid);

        packet << uint32(iter->first);

        packet.WriteGuidBytes<5>(guid);
        packet.WriteGuidBytes<0, 3, 1, 4>(counter);
        packet.WriteGuidBytes<0, 7>(guid);
        packet.WriteGuidBytes<7>(counter);

        packet << uint32(now - iter->second.date);               // Timer 1
        packet << uint32(secsToTimeBitFields(now));

        packet.WriteGuidBytes<1>(guid);
    }

    for (CompletedAchievementMap::const_iterator iter = achievements.GetCompletedAchievements().begin(); iter != achievements.GetCompletedAchievements().end(); ++iter)
    {
        packet << uint32(iter->first);
        packet << uint32(secsToTimeBitFields(iter->second.date));
    }
}

void BuildRespondInspectAchievementsPacket(WorldPacket& packet, Player const& inspected)
{
    AchievementMgr const& achievements = inspected.GetAchievementMgr();
    // since we don't know the exact size of the packed GUIDs this is just an approximation
    packet.Initialize(SMSG_RESPOND_INSPECT_ACHIEVEMENTS, 4 + 4 * 2 + achievements.GetCompletedAchievements().size() * 4 * 2 + achievements.GetCompletedAchievements().size() * 7 * 4);

    ObjectGuid targetGuid = inspected.GetObjectGuid();
    ObjectGuid guid = inspected.GetObjectGuid();

    packet.WriteGuidMask<7, 4, 1>(targetGuid);

    packet.WriteBits(achievements.GetCompletedAchievements().size(), 23);

    packet.WriteGuidMask<0, 3>(targetGuid);

    packet.WriteBits(achievements.GetCriteriaProgress().size(), 21);

    packet.WriteGuidMask<2>(targetGuid);

    for (CriteriaProgressMap::const_iterator iter = achievements.GetCriteriaProgress().begin(); iter != achievements.GetCriteriaProgress().end(); ++iter)
    {
        ObjectGuid counter = ObjectGuid(uint64(iter->second.counter));

        packet.WriteGuidMask<5, 3>(guid);
        packet.WriteGuidMask<1, 4, 2>(counter);
        packet.WriteGuidMask<6>(guid);
        packet.WriteGuidMask<0>(counter);
        packet.WriteGuidMask<4, 1, 2>(guid);
        packet.WriteGuidMask<3, 7>(counter);

        uint8 flags = 0;                                    // Seems always 0
        packet.WriteBits(flags, 2);

        packet.WriteGuidMask<0>(guid);
        packet.WriteGuidMask<5, 6>(counter);
        packet.WriteGuidMask<7>(guid);
    }

    packet.WriteGuidMask<6, 5>(targetGuid);

    time_t now = time(NULL);
    for (CriteriaProgressMap::const_iterator iter = achievements.GetCriteriaProgress().begin(); iter != achievements.GetCriteriaProgress().end(); ++iter)
    {
        ObjectGuid counter = ObjectGuid(uint64(iter->second.counter));

        packet.WriteGuidBytes<3>(counter);
        packet.WriteGuidBytes<4>(guid);

        packet << uint32(now - iter->second.date);            // Timer 1

        packet.WriteGuidBytes<1>(counter);

        packet << uint32(secsToTimeBitFields(now));

        packet.WriteGuidBytes<3, 7>(guid);
        packet.WriteGuidBytes<5>(counter);
        packet.WriteGuidBytes<0>(guid);
        packet.WriteGuidBytes<4, 2, 6, 7>(counter);
        packet.WriteGuidBytes<6>(guid);

        packet << uint32(iter->first);
        packet << uint32(now - iter->second.date);            // Timer 2

        packet.WriteGuidBytes<1, 5>(guid);
        packet.WriteGuidBytes<0>(counter);
        packet.WriteGuidBytes<2>(guid);
    }

    packet.WriteGuidBytes<1, 6, 3, 0, 2>(targetGuid);

    for (CompletedAchievementMap::const_iterator iter = achievements.GetCompletedAchievements().begin(); iter != achievements.GetCompletedAchievements().end(); ++iter)
    {
        packet << uint32(iter->first);
        packet << uint32(secsToTimeBitFields(iter->second.date));
    }

    packet.WriteGuidBytes<7, 4, 5>(targetGuid);
}

void BuildServerFirstAchievementPacket(WorldPacket& packet, char const* name, ObjectGuid guid, uint32 achievementId)
{
    packet.Initialize(SMSG_SERVER_FIRST_ACHIEVEMENT, strlen(name) + 1 + 8 + 4 + 4);
    packet << name;
    packet << guid;
    packet << uint32(achievementId);
    packet << uint32(0);                                      // 1=link supplied string as player name, 0=display plain string
}

void SendRespondInspectAchievements(Player& viewer, Player const& inspected)
{
    WorldPacket data;
    BuildRespondInspectAchievementsPacket(data, inspected);
    viewer.GetSession()->SendPacket(&data);
}

void SessionAchievementNotices::EarnedSaid(Player const& earner, ChatMsg msgtype, uint32 achievementId, std::vector<Player const*> const& listeners)
{
    MaNGOS::AchievementChatBuilder say_builder(earner, msgtype, LANG_ACHIEVEMENT_EARNED, achievementId);
    MaNGOS::LocalizedPacketDo<MaNGOS::AchievementChatBuilder> say_do(say_builder);
    for (std::vector<Player const*>::const_iterator itr = listeners.begin(); itr != listeners.end(); ++itr)
    {
        say_do(*itr);
    }
}

void SessionAchievementNotices::Earned(Player& earner, uint32 achievementId, float range)
{
    WorldPacket data;
    BuildAchievementEarnedPacket(data, earner, achievementId);
    earner.SendMessageToSetInRange(&data, range, true);
}

void SessionAchievementNotices::CriteriaUpdated(Player& player, uint32 criteriaId, CriteriaProgress const& progress)
{
    WorldPacket data;
    BuildCriteriaUpdatePacket(data, player, criteriaId, progress);
    player.SendDirectMessage(&data);
}

void SessionAchievementNotices::CriteriaDeleted(Player& player, uint32 criteriaId)
{
    WorldPacket data;
    BuildCriteriaDeletedPacket(data, criteriaId);
    player.SendDirectMessage(&data);
}

void SessionAchievementNotices::AchievementDeleted(Player& player, uint32 achievementId)
{
    WorldPacket data;
    BuildAchievementDeletedPacket(data, achievementId);
    player.SendDirectMessage(&data);
}

void SessionAchievementNotices::AllData(Player& player)
{
    WorldPacket data;
    BuildAllAchievementDataPacket(data, player);
    player.GetSession()->SendPacket(&data);
}
