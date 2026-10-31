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

#ifndef MANGOS_H_GOSSIPPACKETS
#define MANGOS_H_GOSSIPPACKETS

#include "GossipDef.h"

/// The menus' notices, each sent to the menu's own client: the gossip ones are built in GossipPackets.cpp,
/// the quest-giver ones in session/packets/quests/QuestGiverPackets.cpp.
class SessionGossipNotices : public GossipNotices
{
    public:
        void SendGossipMenu(PlayerSession& session, GossipMenu const& gossipMenu, QuestMenu const& questMenu, uint32 TitleTextId, ObjectGuid objectGuid) override;
        void CloseGossip(PlayerSession& session) override;
        void SendPointOfInterest(PlayerSession& session, float X, float Y, uint32 Icon, uint32 Flags, uint32 Data, char const* locName) override;
        void SendPointOfInterest(PlayerSession& session, uint32 poi_id) override;
        void SendTalking(PlayerSession& session, uint32 textID) override;
        void SendTalking(PlayerSession& session, char const* title, char const* text) override;
        void SendQuestGiverQuestList(PlayerSession& session, QuestMenu const& questMenu, QEmote eEmote, const std::string& Title, ObjectGuid npcGUID) override;
        void SendQuestGiverStatus(PlayerSession& session, uint32 questStatus, ObjectGuid npcGUID) override;
        void SendQuestGiverQuestDetails(PlayerSession& session, Quest const* pQuest, ObjectGuid guid, bool ActivateAccept) override;
        void SendQuestQueryResponse(PlayerSession& session, Quest const* pQuest) override;
        void SendQuestGiverOfferReward(PlayerSession& session, Quest const* pQuest, ObjectGuid npcGUID, bool EnableNext) override;
        void SendQuestGiverRequestItems(PlayerSession& session, Quest const* pQuest, ObjectGuid npcGUID, bool Completable, bool CloseOnCancel) override;
};

#endif
