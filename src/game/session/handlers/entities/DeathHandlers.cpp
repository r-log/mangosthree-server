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

#include "session/handlers/entities/DeathHandlers.h"

#include "Platform/Define.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "Corpse.h"
#include "DBCStores.h"
#include "DBCEnums.h"
#include "GridMap.h"
#include "WorldSession.h"
#include "BattleGround/BattleGround.h"

void DeathHandlers::HandleRepopRequest(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_REPOP_REQUEST");

    recv_data.read_skip<uint8>();

    if (session.GetPlayer()->IsAlive() || session.GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
    {
        return;
    }

    if (session.GetPlayer()->HasAuraType(SPELL_AURA_PREVENT_RESURRECTION))
    {
        return;
    }

    // the world update order is sessions, players, creatures
    // the netcode runs in parallel with all of these
    // creatures can kill players
    // so if the server is lagging enough the player can
    // release spirit after he's killed but before he is updated
    if (session.GetPlayer()->GetDeathState() == JUST_DIED)
    {
        DEBUG_LOG("DeathHandlers::HandleRepopRequest: got request after player %s(%d) was killed and before he was updated", session.GetPlayer()->GetName(), session.GetPlayer()->GetGUIDLow());
        session.GetPlayer()->KillPlayer();
    }

    // this is spirit release confirm?
    session.GetPlayer()->RemovePet(PET_SAVE_REAGENTS);
    session.GetPlayer()->BuildPlayerRepop();
    session.GetPlayer()->RepopAtGraveyard();
}

/**
 * @brief Attempts to reclaim the player's corpse.
 *
 * @param recv_data The received opcode packet.
 */
void DeathHandlers::HandleReclaimCorpse(WorldSession& session, WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_RECLAIM_CORPSE");

    ObjectGuid guid;
    recv_data >> guid;

    if (session.GetPlayer()->IsAlive())
    {
        return;
    }

    // do not allow corpse reclaim in arena
    if (session.GetPlayer()->InArena())
    {
        return;
    }

    // body not released yet
    if (!session.GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
    {
        return;
    }

    Corpse* corpse = session.GetPlayer()->GetCorpse();

    if (!corpse)
    {
        return;
    }

    // prevent resurrect before 30-sec delay after body release not finished
    if (corpse->GetGhostTime() + session.GetPlayer()->GetCorpseReclaimDelay(corpse->GetType() == CORPSE_RESURRECTABLE_PVP) > time(NULL))
    {
        return;
    }

    if (!InReach(*corpse, *(session.GetPlayer()), CORPSE_RECLAIM_RADIUS, true))
    {
        return;
    }

    // resurrect
    session.GetPlayer()->ResurrectPlayer(session.GetPlayer()->InBattleGround() ? 1.0f : 0.5f);

    // spawn bones
    session.GetPlayer()->SpawnCorpseBones();
}

/**
 * @brief Accepts or rejects a pending resurrection request.
 *
 * @param recv_data The received opcode packet.
 */
void DeathHandlers::HandleResurrectResponse(WorldSession& session, WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_RESURRECT_RESPONSE");

    ObjectGuid guid;
    uint8 status;
    recv_data >> guid;
    recv_data >> status;

    if (session.GetPlayer()->IsAlive())
    {
        return;
    }

    if (status == 0)
    {
        session.GetPlayer()->clearResurrectRequestData();           // reject
        return;
    }

    if (!session.GetPlayer()->isRessurectRequestedBy(guid))
    {
        return;
    }

    session.GetPlayer()->ResurectUsingRequestData();                // will call spawncorpsebones
}

void DeathHandlers::HandleReturnToGraveyard(WorldSession& session, WorldPacket& /*recvPacket*/)
{
    Player* pPlayer = session.GetPlayer();
    if (pPlayer->IsAlive() || !pPlayer->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
    {
        return;
    }

    WorldSafeLocsEntry const* ClosestGrave = NULL;

    // Special handle for battleground maps
    if (BattleGround* bg = pPlayer->GetBattleGround())
    {
        ClosestGrave = bg->GetClosestGraveYard(pPlayer);
    }
    else
    {
        ClosestGrave = sObjectMgr.GetClosestGraveYard(pPlayer->GetCorpse()->Where().X(), pPlayer->GetCorpse()->Where().Y(), pPlayer->GetCorpse()->Where().Z(), pPlayer->GetCorpse()->GetMapId(), pPlayer->GetTeam());
    }

    // if no grave found, stay at the current location
    // and don't show spirit healer location
    if (ClosestGrave)
    {
        bool updateVisibility = pPlayer->IsInWorld() && pPlayer->GetCorpse()->GetMapId() == ClosestGrave->Continent;
        pPlayer->TeleportTo(ClosestGrave->Continent, ClosestGrave->Loc_0, ClosestGrave->Loc_1, ClosestGrave->Loc_2, pPlayer->Where().Facing());
        if (pPlayer->IsDead())                                       // not send if alive, because it used in TeleportTo()
        {
            WorldPacket data(SMSG_DEATH_RELEASE_LOC, 4 * 4);// show spirit healer position on minimap
            data << ClosestGrave->Continent;
            data << ClosestGrave->Loc_0;
            data << ClosestGrave->Loc_1;
            data << ClosestGrave->Loc_2;
            pPlayer->GetSession()->SendPacket(&data);
        }

        if (updateVisibility && pPlayer->IsInWorld())
        {
            pPlayer->UpdateVisibilityAndView();
        }
    }
}

void DeathHandlers::HandleHearthandResurrect(WorldSession& session, WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_HEARTH_AND_RESURRECT");

    AreaTableEntry const* atEntry = sAreaStore.LookupEntry(session.GetPlayer()->GetTerrain()->GetAreaId(session.GetPlayer()->Where().X(), session.GetPlayer()->Where().Y(), session.GetPlayer()->Where().Z()));
    if (!atEntry || !(atEntry->Flags & AREA_FLAG_CAN_HEARTH_AND_RES))
    {
        return;
    }

    // Can't use in flight
    if (session.GetPlayer()->IsTaxiFlying())
    {
        return;
    }

    // Send Everytime
    session.GetPlayer()->BuildPlayerRepop();
    session.GetPlayer()->ResurrectPlayer(100);
    session.GetPlayer()->TeleportToHomebind();
}
