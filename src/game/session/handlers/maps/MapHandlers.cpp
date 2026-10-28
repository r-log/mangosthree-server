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

#include "session/handlers/maps/MapHandlers.h"

#include "Platform/Define.h"
#include "Language.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "Corpse.h"
#include "DBCStores.h"
#include "DBCEnums.h"
#include "Group.h"
#include "Map.h"
#include "WorldSession.h"
#include "data/ScriptHooks.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"

/**
 * @brief Updates the player's cached zone and area.
 *
 * @param recv_data The received opcode packet.
 */
void MapHandlers::HandleZoneUpdate(WorldSession& session, WorldPacket& recv_data)
{
    uint32 newZone;
    recv_data >> newZone;

    DETAIL_LOG("WORLD: Received opcode CMSG_ZONEUPDATE: newzone is %u", newZone);

    // use server side data
    uint32 newzone, newarea;
    session.GetPlayer()->GetTerrain()->GetZoneAndAreaId(newzone, newarea, session.GetPlayer()->Where().X(), session.GetPlayer()->Where().Y(), session.GetPlayer()->Where().Z());
    session.GetPlayer()->UpdateZone(newzone, newarea);
}

/**
 * @brief Processes an area trigger activation.
 *
 * @param recv_data The received opcode packet.
 */
void MapHandlers::HandleAreaTrigger(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_AREATRIGGER");

    uint32 Trigger_ID;

    recv_data >> Trigger_ID;
    DEBUG_LOG("Trigger ID: %u", Trigger_ID);
    Player* player = session.GetPlayer();

    if (player->IsTaxiFlying())
    {
        DEBUG_LOG("Player '%s' (GUID: %u) in flight, ignore Area Trigger ID: %u", player->GetName(), player->GetGUIDLow(), Trigger_ID);
        return;
    }

    AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(Trigger_ID);
    if (!atEntry)
    {
        DEBUG_LOG("Player '%s' (GUID: %u) send unknown (by DBC) Area Trigger ID: %u", player->GetName(), player->GetGUIDLow(), Trigger_ID);
        return;
    }

    // delta is safe radius
    const float delta = 5.0f;

    // check if player in the range of areatrigger
    if (!IsPointInAreaTriggerZone(atEntry, player->GetMapId(), player->Where().X(), player->Where().Y(), player->Where().Z(), delta))
    {
        DEBUG_LOG("Player '%s' (GUID: %u) too far, ignore Area Trigger ID: %u", player->GetName(), player->GetGUIDLow(), Trigger_ID);
        return;
    }

    if (sScriptHooks.OnAreaTrigger(player, atEntry))
    {
        return;
    }

    uint32 quest_id = sObjectMgr.GetQuestForAreaTrigger(Trigger_ID);
    if (quest_id && player->IsAlive() && player->GetQuestStatusMgr().IsActiveQuest(quest_id))
    {
        Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);
        if (pQuest)
        {
            if (player->GetQuestStatusMgr().GetQuestStatus(quest_id) == QUEST_STATUS_INCOMPLETE)
            {
                player->AreaExploredOrEventHappens(quest_id);
            }
        }
    }

    // enter to tavern, not overwrite city rest
    if (sObjectMgr.IsTavernAreaTrigger(Trigger_ID))
    {
        // set resting flag we are in the inn
        if (player->GetRestType() != REST_TYPE_IN_CITY)
        {
            player->SetRestType(REST_TYPE_IN_TAVERN, Trigger_ID);
        }
        return;
    }

    if (BattleGround* bg = player->GetBattleGround())
    {
        if (bg->HandleAreaTrigger(player, Trigger_ID))
        {
            return;
        }
    }
    else if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(player->GetCachedZoneId()))
    {
        if (outdoorPvP->HandleAreaTrigger(player, Trigger_ID))
        {
            return;
        }
    }

    // NULL if all values default (non teleport trigger)
    AreaTrigger const* at = sObjectMgr.GetAreaTrigger(Trigger_ID);
    if (!at)
    {
        return;
    }

    MapEntry const* targetMapEntry = sMapStore.LookupEntry(at->target_mapId);
    if (!targetMapEntry)
    {
        return;
    }

    // ghost resurrected at enter attempt to dungeon with corpse (including fail enter cases)
    if (!player->IsAlive() && targetMapEntry->IsDungeon())
    {
        int32 corpseMapId = 0;
        if (Corpse* corpse = player->GetCorpse())
        {
            corpseMapId = corpse->GetMapId();
        }

        // check back way from corpse to entrance
        uint32 instance_map = corpseMapId;
        do
        {
            // most often fast case
            if (instance_map == targetMapEntry->ID)
            {
                break;
            }

            InstanceTemplate const* instance = ObjectMgr::GetInstanceTemplate(instance_map);
            instance_map = instance ? instance->parent : 0;
        }
        while (instance_map);

        // corpse not in dungeon or some linked deep dungeons
        if (!instance_map)
        {
            WorldPacket data(SMSG_AREA_TRIGGER_NO_CORPSE);
            player->GetSession()->SendPacket(&data);
            return;
        }

        // need find areatrigger to inner dungeon for landing point
        if (at->target_mapId != corpseMapId)
        {
            if (AreaTrigger const* corpseAt = sObjectMgr.GetMapEntranceTrigger(corpseMapId))
            {
                at = corpseAt;
                targetMapEntry = sMapStore.LookupEntry(at->target_mapId);
                if (!targetMapEntry)
                {
                    return;
                }
            }
        }

        // now we can resurrect player, and then check teleport requirements
        player->ResurrectPlayer(0.5f);
        player->SpawnCorpseBones();
    }

    // teleport player (trigger requirement will be checked on TeleportTo)
    player->TeleportTo(at->target_mapId, at->target_X, at->target_Y, at->target_Z, at->target_Orientation, TELE_TO_NOT_LEAVE_TRANSPORT, at);
}

/**
 * @brief Teleports an administrator to explicit world coordinates.
 *
 * @param recv_data The received opcode packet.
 */
void MapHandlers::HandleWorldTeleport(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_WORLD_TELEPORT from %s", session.GetPlayer()->GetGuidStr().c_str());

    // write in client console: worldport 469 452 6454 2536 180 or /console worldport 469 452 6454 2536 180
    // Received opcode CMSG_WORLD_TELEPORT
    // Time is ***, map=469, x=452.000000, y=6454.000000, z=2536.000000, orient=3.141593

    uint32 time;
    uint32 mapid;
    float PositionX;
    float PositionY;
    float PositionZ;
    float Orientation;

    recv_data >> time;                                      // time in m.sec.
    recv_data >> mapid;
    recv_data >> PositionX;
    recv_data >> PositionY;
    recv_data >> PositionZ;
    recv_data >> Orientation;                               // o (3.141593 = 180 degrees)

    // DEBUG_LOG("Received opcode CMSG_WORLD_TELEPORT");

    if (session.GetPlayer()->IsTaxiFlying())
    {
        DEBUG_LOG("Player '%s' (GUID: %u) in flight, ignore worldport command.", session.GetPlayer()->GetName(), session.GetPlayer()->GetGUIDLow());
        return;
    }

    DEBUG_LOG("Time %u sec, map=%u, x=%f, y=%f, z=%f, orient=%f", time / 1000, mapid, PositionX, PositionY, PositionZ, Orientation);

    if (session.GetSecurity() >= SEC_ADMINISTRATOR)
    {
        session.GetPlayer()->TeleportTo(mapid, PositionX, PositionY, PositionZ, Orientation);
    }
    else
    {
        session.SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
    }
}

/**
 * @brief Resets the player's or group's saved instances.
 *
 * @param recv_data The received opcode packet.
 */
void MapHandlers::HandleResetInstances(WorldSession& session, WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_RESET_INSTANCES");

    if (Group* pGroup = session.GetPlayer()->GetGroup())
    {
        if (pGroup->IsLeader(session.GetPlayer()->GetObjectGuid()))
        {
            pGroup->ResetInstances(INSTANCE_RESET_ALL, false, session.GetPlayer());
        }
    }
    else
    {
        session.GetPlayer()->ResetInstances(INSTANCE_RESET_ALL, false);
    }
}

void MapHandlers::HandleSetDungeonDifficulty(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode MSG_SET_DUNGEON_DIFFICULTY");

    uint32 mode;
    recv_data >> mode;

    if (mode >= MAX_DUNGEON_DIFFICULTY)
    {
        sLog.outError("MapHandlers::HandleSetDungeonDifficulty: player %d sent an invalid instance mode %d!", session.GetPlayer()->GetGUIDLow(), mode);
        return;
    }

    if (Difficulty(mode) == session.GetPlayer()->GetDungeonDifficulty())
    {
        return;
    }

    // cannot reset while in an instance
    Map* map = session.GetPlayer()->FindMap();
    if (map && map->IsDungeon())
    {
        sLog.outError("MapHandlers::HandleSetDungeonDifficulty: player %d tried to reset the instance while inside!", session.GetPlayer()->GetGUIDLow());
        return;
    }

    // Exception to set mode to normal for low-level players
    if (session.GetPlayer()->getLevel() < LEVELREQUIREMENT_HEROIC && mode > REGULAR_DIFFICULTY)
    {
        return;
    }

    if (Group* pGroup = session.GetPlayer()->GetGroup())
    {
        if (pGroup->IsLeader(session.GetPlayer()->GetObjectGuid()))
        {
            // the difficulty is set even if the instances can't be reset
            //session.GetPlayer()->SendDungeonDifficulty(true);
            pGroup->ResetInstances(INSTANCE_RESET_CHANGE_DIFFICULTY, false, session.GetPlayer());
            pGroup->SetDungeonDifficulty(Difficulty(mode));
        }
    }
    else
    {
        session.GetPlayer()->ResetInstances(INSTANCE_RESET_CHANGE_DIFFICULTY, false);
        session.GetPlayer()->SetDungeonDifficulty(Difficulty(mode));
    }
}

void MapHandlers::HandleSetRaidDifficulty(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode MSG_SET_RAID_DIFFICULTY");

    uint32 mode;
    recv_data >> mode;

    if (mode >= MAX_RAID_DIFFICULTY)
    {
        sLog.outError("MapHandlers::HandleSetRaidDifficulty: player %d sent an invalid instance mode %d!", session.GetPlayer()->GetGUIDLow(), mode);
        return;
    }

    if (Difficulty(mode) == session.GetPlayer()->GetRaidDifficulty())
    {
        return;
    }

    // cannot reset while in an instance
    Map* map = session.GetPlayer()->FindMap();
    if (map && map->IsDungeon())
    {
        sLog.outError("MapHandlers::HandleSetRaidDifficulty: player %d tried to reset the instance while inside!", session.GetPlayer()->GetGUIDLow());
        return;
    }

    // Exception to set mode to normal for low-level players
    if (session.GetPlayer()->getLevel() < LEVELREQUIREMENT_HEROIC && mode > REGULAR_DIFFICULTY)
    {
        return;
    }

    if (Group* pGroup = session.GetPlayer()->GetGroup())
    {
        if (pGroup->IsLeader(session.GetPlayer()->GetObjectGuid()))
        {
            // the difficulty is set even if the instances can't be reset
            session.GetPlayer()->SendDungeonDifficulty(true);
            pGroup->ResetInstances(INSTANCE_RESET_CHANGE_DIFFICULTY, true, session.GetPlayer());
            pGroup->SetRaidDifficulty(Difficulty(mode));
        }
    }
    else
    {
        session.GetPlayer()->ResetInstances(INSTANCE_RESET_CHANGE_DIFFICULTY, true);
        session.GetPlayer()->SetRaidDifficulty(Difficulty(mode));
    }
}
