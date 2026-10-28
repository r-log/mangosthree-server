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

#include "session/handlers/entities/QueryHandlers.h"

#include "Platform/Define.h"
#include <string>
#include <vector>
#include "WorldPacket.h"
#include "Server/WorldSession.h"
#include "Opcodes.h"
#include "Log/Log.h"
#include "Object/ObjectMgr.h"
#include "Object/Corpse.h"
#include "Server/DBCStores.h"
#include "entities/player/Player.h"
#include "data/GossipRecords.h"
#include "WorldHandlers/MapManager.h"

/**
 * @brief Handles a client request to query another player's name.
 *
 * @param recv_data The incoming name query packet.
 */
void QueryHandlers::HandleNameQuery(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;

    Player* pChar = sObjectMgr.GetPlayer(guid);

    if (pChar)
    {
        session.SendNameQueryOpcode(pChar);
    }
    else
    {
        session.SendNameQueryOpcodeFromDB(guid);
    }
}

/**
 * @brief Handles a client request for the current server time.
 *
 * @param recv_data The unused incoming packet.
 */
void QueryHandlers::HandleQueryTime(WorldSession& session, WorldPacket & /*recv_data*/)
{
    session.SendQueryTimeResponse();
}

/// Only _static_ data send in this packet !!!
void QueryHandlers::HandleCreatureQuery(WorldSession& session, WorldPacket& recv_data)
{
    uint32 entry;
    recv_data >> entry;
    ObjectGuid guid;
    recv_data >> guid;

    CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(entry);
    if (ci)
    {
        int loc_idx = session.GetSessionDbLocaleIndex();

        char const* name = ci->Name;
        char const* subName = ci->SubName;
        sObjectMgr.GetCreatureLocaleStrings(entry, loc_idx, &name, &subName);

        DETAIL_LOG("WORLD: CMSG_CREATURE_QUERY '%s' - Entry: %u.", ci->Name, entry);
        // guess size
        WorldPacket data(SMSG_CREATURE_QUERY_RESPONSE, 100);
        data << uint32(entry);                              // creature entry
        data << name;

        for (uint8 i = 0; i < 7; ++i)
        {
            data << uint8(0);            // name2, name3, name4, always empty
        }

        data << subName;
        data << ci->IconName;                               // "Directions" for guard, string for Icons 2.3.0
        data << uint32(ci->CreatureTypeFlags);                     // flags
        data << uint32(0);                                  // unk
        data << uint32(ci->CreatureType);                   // CreatureType.dbc
        data << uint32(ci->Family);                         // CreatureFamily.dbc
        data << uint32(ci->Rank);                           // Creature Rank (elite, boss, etc)
        data << uint32(ci->KillCredit[0]);                  // new in 3.1, kill credit
        data << uint32(ci->KillCredit[1]);                  // new in 3.1, kill credit

        for (int i = 0; i < MAX_CREATURE_MODEL; ++i)
        {
            data << uint32(ci->ModelId[i]);
        }

        data << float(ci->HealthMultiplier);                  // health modifier
        data << float(ci->PowerMultiplier);                   // power modifier
        data << uint8(ci->RacialLeader);
        for (uint32 i = 0; i < 6; ++i)
        {
            data << uint32(ci->QuestItems[i]);              // itemId[6], quest drop
        }
        data << uint32(ci->MovementTemplateId);                     // CreatureMovementInfo.dbc
        data << uint32(0);                                  //unk
        session.SendPacket(&data);
        DEBUG_LOG("WORLD: Sent SMSG_CREATURE_QUERY_RESPONSE");
    }
    else
    {
        DEBUG_LOG("WORLD: CMSG_CREATURE_QUERY - Guid: %s Entry: %u NO CREATURE INFO!",
                  guid.GetString().c_str(), entry);
        WorldPacket data(SMSG_CREATURE_QUERY_RESPONSE, 4);
        data << uint32(entry | 0x80000000);
        session.SendPacket(&data);
        DEBUG_LOG("WORLD: Sent SMSG_CREATURE_QUERY_RESPONSE");
    }
}

/// Only _static_ data send in this packet !!!
void QueryHandlers::HandleGameObjectQuery(WorldSession& session, WorldPacket& recv_data)
{
    uint32 entryID;
    recv_data >> entryID;
    ObjectGuid guid;
    recv_data >> guid;

    const GameObjectInfo* info = ObjectMgr::GetGameObjectInfo(entryID);
    if (info)
    {
        std::string Name;
        std::string IconName;
        std::string CastBarCaption;

        Name = info->name;
        IconName = info->IconName;
        CastBarCaption = info->castBarCaption;

        int loc_idx = session.GetSessionDbLocaleIndex();
        if (loc_idx >= 0)
        {
            GameObjectLocale const* gl = sObjectMgr.GetGameObjectLocale(entryID);
            if (gl)
            {
                if (gl->Name.size() > size_t(loc_idx) && !gl->Name[loc_idx].empty())
                {
                    Name = gl->Name[loc_idx];
                }
                if (gl->CastBarCaption.size() > size_t(loc_idx) && !gl->CastBarCaption[loc_idx].empty())
                {
                    CastBarCaption = gl->CastBarCaption[loc_idx];
                }
            }
        }
        DETAIL_LOG("WORLD: CMSG_GAMEOBJECT_QUERY '%s' - Entry: %u. ", info->name, entryID);
        WorldPacket data(SMSG_GAMEOBJECT_QUERY_RESPONSE, 150);
        data << uint32(entryID);
        data << uint32(info->type);
        data << uint32(info->displayId);
        data << Name;
        data << uint8(0) << uint8(0) << uint8(0);           // name2, name3, name4
        data << IconName;                                   // 2.0.3, string. Icon name to use instead of default icon for go's (ex: "Attack" makes sword)
        data << CastBarCaption;                             // 2.0.3, string. Text will appear in Cast Bar when using GO (ex: "Collecting")
        data << info->unk1;                                 // 2.0.3, string
        data.append(info->raw.data, 24);
        data << float(info->size);                          // go size
        for (uint32 i = 0; i < 6; ++i)
        {
            data << uint32(info->questItems[i]);            // itemId[6], quest drop
        }
        session.SendPacket(&data);
        DEBUG_LOG("WORLD: Sent SMSG_GAMEOBJECT_QUERY_RESPONSE");
    }
    else
    {
        DEBUG_LOG("WORLD: CMSG_GAMEOBJECT_QUERY - Guid: %s Entry: %u Missing gameobject info!",
                  guid.GetString().c_str(), entryID);
        WorldPacket data(SMSG_GAMEOBJECT_QUERY_RESPONSE, 4);
        data << uint32(entryID | 0x80000000);
        session.SendPacket(&data);
        DEBUG_LOG("WORLD: Sent SMSG_GAMEOBJECT_QUERY_RESPONSE");
    }
}

/**
 * @brief Handles a corpse query and returns corpse or graveyard location data.
 *
 * @param recv_data The unused incoming packet.
 */
void QueryHandlers::HandleCorpseQuery(WorldSession& session, WorldPacket & /*recv_data*/)
{
    DETAIL_LOG("WORLD: Received opcode MSG_CORPSE_QUERY");

    Corpse* corpse = session.GetPlayer()->GetCorpse();

    if (!corpse)
    {
        WorldPacket data(MSG_CORPSE_QUERY, 1);
        data << uint8(0);                                   // corpse not found
        session.SendPacket(&data);
        return;
    }

    uint32 corpsemapid = corpse->GetMapId();
    float x = corpse->Where().X();
    float y = corpse->Where().Y();
    float z = corpse->Where().Z();
    int32 mapid = corpsemapid;

    // if corpse at different map
    if (corpsemapid != session.GetPlayer()->GetMapId())
    {
        // search entrance map for proper show entrance
        if (MapEntry const* corpseMapEntry = sMapStore.LookupEntry(corpsemapid))
        {
            if (corpseMapEntry->IsDungeon() && corpseMapEntry->CorpseMapID >= 0)
            {
                // if corpse map have entrance
                if (TerrainInfo const* entranceMap = sTerrainMgr.LoadTerrain(corpseMapEntry->CorpseMapID))
                {
                    mapid = corpseMapEntry->CorpseMapID;
                    x = corpseMapEntry->Corpse_0;
                    y = corpseMapEntry->Corpse_1;
                    const auto entranceFloor = entranceMap->StaticFloor(x, y, MAX_HEIGHT);
                    z = entranceFloor ? *entranceFloor : INVALID_HEIGHT;
                }
            }
        }
    }

    WorldPacket data(MSG_CORPSE_QUERY, 1 + (6 * 4));
    data << uint8(1);                                       // corpse found
    data << int32(mapid);
    data << float(x);
    data << float(y);
    data << float(z);
    data << uint32(corpsemapid);
    data << uint32(0);                                      // unknown
    session.SendPacket(&data);
}

/**
 * @brief Handles an NPC text query and sends localized gossip text options.
 *
 * @param recv_data The incoming NPC text query packet.
 */
void QueryHandlers::HandleNpcTextQuery(WorldSession& session, WorldPacket& recv_data)
{
    uint32 textID;
    ObjectGuid guid;

    recv_data >> textID;
    recv_data >> guid;

    DETAIL_LOG("WORLD: CMSG_NPC_TEXT_QUERY ID '%u'", textID);

    session.GetPlayer()->SetTargetGuid(guid);

    GossipText const* pGossip = sObjectMgr.GetGossipText(textID);

    WorldPacket data(SMSG_NPC_TEXT_UPDATE, 100);            // guess size
    data << textID;

    if (!pGossip)
    {
        for (uint32 i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
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

    DEBUG_LOG("WORLD: Sent SMSG_NPC_TEXT_UPDATE");
}

/**
 * @brief Handles an item page text query and sends all linked pages.
 *
 * @param recv_data The incoming page text query packet.
 */
void QueryHandlers::HandlePageTextQuery(WorldSession& session, WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_PAGE_TEXT_QUERY");
    recv_data.hexlike();

    uint32 pageID;
    recv_data >> pageID;
    recv_data.read_skip<uint64>();                          // guid

    while (pageID)
    {
        PageText const* pPage = sObjectMgr.GetPageText(pageID);
        // guess size
        WorldPacket data(SMSG_PAGE_TEXT_QUERY_RESPONSE, 50);
        data << pageID;

        if (!pPage)
        {
            data << "Item page missing.";
            data << uint32(0);
            pageID = 0;
        }
        else
        {
            std::string Text = pPage->Text;

            int loc_idx = session.GetSessionDbLocaleIndex();
            if (loc_idx >= 0)
            {
                PageTextLocale const* pl = sObjectMgr.GetPageTextLocale(pageID);
                if (pl)
                {
                    if (pl->Text.size() > size_t(loc_idx) && !pl->Text[loc_idx].empty())
                    {
                        Text = pl->Text[loc_idx];
                    }
                }
            }

            data << Text;
            data << uint32(pPage->Next_Page);
            pageID = pPage->Next_Page;
        }
        session.SendPacket(&data);

        DEBUG_LOG("WORLD: Sent SMSG_PAGE_TEXT_QUERY_RESPONSE");
    }
}

void QueryHandlers::HandleCorpseMapPositionQuery(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recv CMSG_CORPSE_TRANSPORT_QUERY");

    uint32 unk;
    recv_data >> unk;

    WorldPacket data(SMSG_CORPSE_TRANSPORT_QUERY, 4 + 4 + 4 + 4);
    data << float(0);
    data << float(0);
    data << float(0);
    data << float(0);
    session.SendPacket(&data);
}

void QueryHandlers::HandleQueryQuestsCompleted(WorldSession& session, WorldPacket& /*recv_data */)
{
    uint32 count = 0;

    WorldPacket data(SMSG_ALL_QUESTS_COMPLETED, 4 + 4 * count);
    data << uint32(count);

    for (QuestStatusMap::const_iterator itr = session.GetPlayer()->GetQuestStatusMgr().Map().begin(); itr != session.GetPlayer()->GetQuestStatusMgr().Map().end(); ++itr)
    {
        if (itr->second.m_rewarded)
        {
            data << uint32(itr->first);
            ++count;
        }
    }
    data.put<uint32>(0, count);
    session.SendPacket(&data);
}

void QueryHandlers::HandleQuestPOIQuery(WorldSession& session, WorldPacket& recv_data)
{
    uint32 count;
    recv_data >> count;                                     // quest count, max=25

    if (count > MAX_QUEST_LOG_SIZE)
    {
        recv_data.rpos(recv_data.wpos());                   // set to end to avoid warnings spam
        return;
    }

    WorldPacket data(SMSG_QUEST_POI_QUERY_RESPONSE, 4 + (4 + 4)*count);
    data << uint32(count);                                  // count

    for (uint32 i = 0; i < count; ++i)
    {
        uint32 questId;
        recv_data >> questId;                               // quest id

        bool questOk = false;

        uint16 questSlot = session.GetPlayer()->FindQuestSlot(questId);

        if (questSlot != MAX_QUEST_LOG_SIZE)
        {
            questOk = session.GetPlayer()->GetQuestSlotQuestId(questSlot) == questId;
        }

        if (questOk)
        {
            QuestPOIVector const* POI = sObjectMgr.GetQuestPOIVector(questId);

            if (POI)
            {
                data << uint32(questId);                    // quest ID
                data << uint32(POI->size());                // POI count

                for (QuestPOIVector::const_iterator itr = POI->begin(); itr != POI->end(); ++itr)
                {
                    data << uint32(itr->PoiId);             // POI index
                    data << int32(itr->ObjectiveIndex);     // objective index
                    data << uint32(itr->MapId);             // mapid
                    data << uint32(itr->MapAreaId);         // world map area id
                    data << uint32(itr->FloorId);           // floor id
                    data << uint32(itr->Unk3);              // unknown
                    data << uint32(itr->Unk4);              // unknown
                    data << uint32(itr->points.size());     // POI points count

                    for (std::vector<QuestPOIPoint>::const_iterator itr2 = itr->points.begin(); itr2 != itr->points.end(); ++itr2)
                    {
                        data << int32(itr2->x);             // POI point x
                        data << int32(itr2->y);             // POI point y
                    }
                }
            }
            else
            {
                data << uint32(questId);                    // quest ID
                data << uint32(0);                          // POI count
            }
        }
        else
        {
            data << uint32(questId);                        // quest ID
            data << uint32(0);                              // POI count
        }
    }

    session.SendPacket(&data);
}
