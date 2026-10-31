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

#include "session/packets/entities/GossipPackets.h"

#include "Utilities/Errors.h"
#include <string>
#include "QuestDef.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "Formulas.h"
#include "data/GameConfig.h"

void SessionGossipNotices::SendGossipMenu(PlayerSession& session, GossipMenu const& gossipMenu, QuestMenu const& questMenu, uint32 TitleTextId, ObjectGuid objectGuid)
{
    WorldPacket data(SMSG_GOSSIP_MESSAGE, (100)); // guess size
    data << ObjectGuid(objectGuid);
    data << uint32(gossipMenu.GetMenuId()); // new 2.4.0
    data << uint32(TitleTextId);
    data << uint32(gossipMenu.MenuItemCount()); // max count 0x20

    for (uint32 iI = 0; iI < gossipMenu.MenuItemCount(); ++iI)
    {
        GossipMenuItem const& gItem = gossipMenu.GetItem(iI);
        data << uint32(iI);
        data << uint8(gItem.m_gIcon);
        data << uint8(gItem.m_gCoded); // makes pop up box password
        data << uint32(gItem.m_gBoxMoney); // money required to open menu, 2.0.3
        data << gItem.m_gMessage; // text for gossip item, max 0x800
        data << gItem.m_gBoxMessage; // accept text (related to money) pop up box, 2.0.3, max 0x800
    }

    data << uint32(questMenu.MenuItemCount()); // max count 0x20

    for (uint32 iI = 0; iI < questMenu.MenuItemCount(); ++iI)
    {
        QuestMenuItem const& qItem = questMenu.GetItem(iI);
        uint32 questID = qItem.m_qId;
        Quest const* pQuest = sObjectMgr.GetQuestTemplate(questID);

        data << uint32(questID);
        data << uint32(qItem.m_qIcon);
        data << int32(pQuest->GetQuestLevel());
        data << uint32(pQuest->GetQuestFlags());            // 3.3.3 quest flags
        data << uint8(0);                                   // 3.3.3 changes icon: blue question or yellow exclamation

        int loc_idx = session.GetSessionDbLocaleIndex();
        std::string title = pQuest->GetTitle();
        sObjectMgr.GetQuestLocaleStrings(questID, loc_idx, &title);

        data << title; // max 0x200
    }

    session.SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_MESSAGE from %s", objectGuid.GetString().c_str());
}

void SessionGossipNotices::CloseGossip(PlayerSession& session)
{
    WorldPacket data(SMSG_GOSSIP_COMPLETE, 0);
    session.SendPacket(&data);

    // DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_COMPLETE");
}

void SessionGossipNotices::SendPointOfInterest(PlayerSession& session, float X, float Y, uint32 Icon, uint32 Flags, uint32 Data, char const* locName)
{
    WorldPacket data(SMSG_GOSSIP_POI, (4 + 4 + 4 + 4 + 4 + 10)); // guess size
    data << uint32(Flags);
    data << float(X);
    data << float(Y);
    data << uint32(Icon);
    data << uint32(Data);
    data << locName;

    session.SendPacket(&data);
    // DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_POI");
}

void SessionGossipNotices::SendPointOfInterest(PlayerSession& session, uint32 poi_id)
{
    PointOfInterest const* poi = sObjectMgr.GetPointOfInterest(poi_id);
    if (!poi)
    {
        sLog.outErrorDb("Requested send nonexistent POI (Id: %u), ignore.", poi_id);
        return;
    }

    std::string icon_name = poi->icon_name;

    int loc_idx = session.GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        if (PointOfInterestLocale const* pl = sObjectMgr.GetPointOfInterestLocale(poi_id))
        {
            if (pl->IconName.size() > size_t(loc_idx) && !pl->IconName[loc_idx].empty())
            {
                icon_name = pl->IconName[loc_idx];
            }
        }
    }

    WorldPacket data(SMSG_GOSSIP_POI, (4 + 4 + 4 + 4 + 4 + 10)); // guess size
    data << uint32(poi->flags);
    data << float(poi->x);
    data << float(poi->y);
    data << uint32(poi->icon);
    data << uint32(poi->data);
    data << icon_name;

    session.SendPacket(&data);
    // DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_POI");
}

void SessionGossipNotices::SendTalking(PlayerSession& session, uint32 textID)
{
    GossipText const* pGossip = sObjectMgr.GetGossipText(textID);

    WorldPacket data(SMSG_NPC_TEXT_UPDATE, 100); // guess size
    data << textID; // can be < 0

    if (!pGossip)
    {
        for (uint32 i = 0; i < 8; ++i)
        {
            data << float(0);
            data << "Greetings $N";
            data << "Greetings $N";
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
        }
    }
    else
    {
        std::string Text_0[MAX_GOSSIP_TEXT_OPTIONS], Text_1[MAX_GOSSIP_TEXT_OPTIONS];
        for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
        {
            Text_0[i] = pGossip->Options[i].Text_0;
            Text_1[i] = pGossip->Options[i].Text_1;
        }

        int loc_idx = session.GetSessionDbLocaleIndex();

        sObjectMgr.GetNpcTextLocaleStringsAll(textID, loc_idx, &Text_0, &Text_1);

        for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
        {
            data << pGossip->Options[i].Probability;

            if (Text_0[i].empty())
            {
                data << Text_1[i];
            }
            else
            {
                data << Text_0[i];
            }

            if (Text_1[i].empty())
            {
                data << Text_0[i];
            }
            else
            {
                data << Text_1[i];
            }

            data << pGossip->Options[i].Language;

            for (int j = 0; j < 3; ++j)
            {
                data << pGossip->Options[i].Emotes[j]._Delay;
                data << pGossip->Options[i].Emotes[j]._Emote;
            }
        }
    }
    session.SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_NPC_TEXT_UPDATE ");
}

void SessionGossipNotices::SendTalking(PlayerSession& session, char const* title, char const* text)
{
    WorldPacket data(SMSG_NPC_TEXT_UPDATE, 50); // guess size
    data << uint32(0);
    for (uint32 i = 0; i < 8; ++i)
    {
        data << float(0);
        data << title;
        data << text;
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
        data << uint32(0);
    }

    session.SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_NPC_TEXT_UPDATE ");
}
