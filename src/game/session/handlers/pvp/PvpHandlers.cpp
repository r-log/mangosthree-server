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

#include "session/handlers/pvp/PvpHandlers.h"

#include <string>
#include "Platform/Define.h"
#include "WorldPacket.h"
#include "Server/WorldSession.h"
#include "Log/Log.h"
#include "entities/player/Player.h"
#include "Object/ObjectMgr.h"
#include "Object/ArenaTeam.h"
#include "WorldHandlers/World.h"
#include "entities/player/social/SocialMgr.h"
#include "entities/player/PlayerRegistry.h"

void PvpHandlers::HandleInspectArenaTeams(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("MSG_INSPECT_ARENA_TEAMS");

    ObjectGuid guid;
    recv_data >> guid;
    DEBUG_LOG("Inspect Arena stats %s", guid.GetString().c_str());

    Player* player = sObjectMgr.GetPlayer(guid);
    if (!player)
    {
        return;
    }

    if (!InReach(*session.GetPlayer(), *player, INSPECT_DISTANCE, false))
    {
        return;
    }

    if (session.GetPlayer()->IsHostileTo(player))
    {
        return;
    }

    for (uint8 i = 0; i < MAX_ARENA_SLOT; ++i)
    {
        if (uint32 a_id = player->GetArenaTeamId(i))
        {
            if (ArenaTeam* arenaTeam = sObjectMgr.GetArenaTeamById(a_id))
            {
                arenaTeam->InspectStats(&session, player->GetObjectGuid());
            }
        }
    }
}

void PvpHandlers::HandleArenaTeamQuery(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ARENA_TEAM_QUERY");

    uint32 ArenaTeamId;
    recv_data >> ArenaTeamId;

    if (ArenaTeam* arenateam = sObjectMgr.GetArenaTeamById(ArenaTeamId))
    {
        arenateam->Query(&session);
        arenateam->Stats(&session);
    }
}

void PvpHandlers::HandleArenaTeamRoster(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ARENA_TEAM_ROSTER");

    uint32 ArenaTeamId;                                     // arena team id
    recv_data >> ArenaTeamId;

    if (ArenaTeam* arenateam = sObjectMgr.GetArenaTeamById(ArenaTeamId))
    {
        arenateam->Roster(&session);
    }
}

void PvpHandlers::HandleArenaTeamCreate(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received CMSG_ARENA_TEAM_CREATE");

    uint32 slot, icon, iconcolor, border, bordercolor, background;
    std::string name;

    recv_data >> slot >> iconcolor >> bordercolor >> border >> background >> icon;
    name = recv_data.ReadString(recv_data.ReadBits(8));

    ArenaType type = ArenaTeam::GetTypeBySlot(slot);
    if (!IsArenaTypeValid(type))
    {
        return;
    }

    if (!IsArenaTypeValid(ArenaType(type)))
    {
        return;
    }

    if (session.GetPlayer()->GetArenaTeamId(slot))
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, name, "", ERR_ALREADY_IN_ARENA_TEAM);
        return;
    }

    if (sObjectMgr.IsReservedName(name) || !ObjectMgr::IsValidCharterName(name))
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, name, "", ERR_ARENA_TEAM_NAME_INVALID);
        return;
    }

    if (sObjectMgr.GetArenaTeamByName(name))
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, name, "", ERR_ARENA_TEAM_NAME_EXISTS_S);
        return;
    }

    ArenaTeam* at = new ArenaTeam;
    if (!at->Create(session.GetPlayer()->GetObjectGuid(), ArenaType(type), name))
    {
        sLog.outError("WorldSession::HandleArenaTeamCreateOpcode: arena team create failed.");
        delete at;
        return;
    }

    at->SetEmblem(background, icon, iconcolor, border, bordercolor);

    // register team and add captain
    sObjectMgr.AddArenaTeam(at);
    DEBUG_LOG("WorldSession::HandleArenaTeamCreateOpcode: arena team added to objmrg");
}

void PvpHandlers::HandleArenaTeamInvite(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_ARENA_TEAM_INVITE");

    uint32 ArenaTeamId;                                     // arena team id
    std::string Invitedname;

    Player* player = NULL;

    recv_data >> ArenaTeamId >> Invitedname;

    if (!Invitedname.empty())
    {
        if (!normalizePlayerName(Invitedname))
        {
            return;
        }

        player = sPlayerRegistry.FindByName(Invitedname.c_str());
    }

    if (!player)
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", Invitedname, ERR_ARENA_TEAM_PLAYER_NOT_FOUND_S);
        return;
    }

    if (player->getLevel() < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", player->GetName(), ERR_ARENA_TEAM_TARGET_TOO_LOW_S);
        return;
    }

    ArenaTeam* arenateam = sObjectMgr.GetArenaTeamById(ArenaTeamId);
    if (!arenateam)
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", "", ERR_ARENA_TEAM_PLAYER_NOT_IN_TEAM);
        return;
    }

    if (arenateam->GetCaptainGuid() != session.GetPlayer()->GetObjectGuid())
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", "", ERR_ARENA_TEAM_PERMISSIONS);
        return;
    }

    // OK result but not send invite
    if (player->GetSocial()->HasIgnore(session.GetPlayer()->GetObjectGuid()))
    {
        return;
    }

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) && player->GetTeam() != session.GetPlayer()->GetTeam())
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_INVITE_SS, "", "", ERR_ARENA_TEAM_NOT_ALLIED);
        return;
    }

    if (player->GetArenaTeamId(arenateam->GetSlot()))
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_INVITE_SS, "", player->GetName(), ERR_ALREADY_IN_ARENA_TEAM_S);
        return;
    }

    if (player->GetArenaTeamIdInvited())
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_INVITE_SS, "", player->GetName(), ERR_ALREADY_INVITED_TO_ARENA_TEAM_S);
        return;
    }

    if (arenateam->GetMembersSize() >= arenateam->GetMaxMembersSize())
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, arenateam->GetName(), "", ERR_ARENA_TEAM_TOO_MANY_MEMBERS_S);
        return;
    }

    DEBUG_LOG("Player %s Invited %s to Join his ArenaTeam", session.GetPlayer()->GetName(), Invitedname.c_str());

    player->SetArenaTeamIdInvited(arenateam->GetId());

    WorldPacket data(SMSG_ARENA_TEAM_INVITE, (8 + 10));
    data << session.GetPlayer()->GetName();
    data << arenateam->GetName();
    player->GetSession()->SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_ARENA_TEAM_INVITE");
}

void PvpHandlers::HandleArenaTeamAccept(WorldSession& session, WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("CMSG_ARENA_TEAM_ACCEPT");                    // empty opcode

    ArenaTeam* at = sObjectMgr.GetArenaTeamById(session.GetPlayer()->GetArenaTeamIdInvited());
    if (!at)
    {
        return;
    }

    if (session.GetPlayer()->GetArenaTeamId(at->GetSlot()))
    {
        // already in arena team that size
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", "", ERR_ALREADY_IN_ARENA_TEAM);
        return;
    }

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) &&
            session.GetPlayer()->GetTeam() != sObjectMgr.GetPlayerTeamByGUID(at->GetCaptainGuid()))
    {
        // not let enemies sign petition
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", "", ERR_ARENA_TEAM_NOT_ALLIED);
        return;
    }

    if (!at->AddMember(session.GetPlayer()->GetObjectGuid()))
    {
        // arena team not found
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", "", ERR_ARENA_TEAM_INTERNAL);
        return;
    }

    // event
    at->BroadcastEvent(ERR_ARENA_TEAM_JOIN_SS, session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName(), at->GetName().c_str());
}

void PvpHandlers::HandleArenaTeamDecline(WorldSession& session, WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("CMSG_ARENA_TEAM_DECLINE");                   // empty opcode

    session.GetPlayer()->SetArenaTeamIdInvited(0);                      // no more invited
}

void PvpHandlers::HandleArenaTeamLeave(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_ARENA_TEAM_LEAVE");

    uint32 ArenaTeamId;                                     // arena team id
    recv_data >> ArenaTeamId;

    ArenaTeam* at = sObjectMgr.GetArenaTeamById(ArenaTeamId);
    if (!at)
    {
        return;
    }

    if (session.GetPlayer()->GetObjectGuid() == at->GetCaptainGuid() && at->GetMembersSize() > 1)
    {
        // check for correctness
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_QUIT_S, "", "", ERR_ARENA_TEAM_LEADER_LEAVE_S);
        return;
    }

    // arena team has only one member (=captain)
    if (session.GetPlayer()->GetObjectGuid() == at->GetCaptainGuid())
    {
        at->Disband(&session);
        delete at;
        return;
    }

    at->DelMember(session.GetPlayer()->GetObjectGuid());

    // event
    at->BroadcastEvent(ERR_ARENA_TEAM_LEAVE_SS, session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName(), at->GetName().c_str());

    // send you are no longer member of team
    session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_QUIT_S, at->GetName(), "", 0);
}

void PvpHandlers::HandleArenaTeamDisband(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_ARENA_TEAM_DISBAND");

    uint32 ArenaTeamId;                                     // arena team id
    recv_data >> ArenaTeamId;

    if (ArenaTeam* at = sObjectMgr.GetArenaTeamById(ArenaTeamId))
    {
        if (at->GetCaptainGuid() != session.GetPlayer()->GetObjectGuid())
        {
            return;
        }

        if (at->IsFighting())
        {
            return;
        }

        at->Disband(&session);
        delete at;
    }
}

void PvpHandlers::HandleArenaTeamRemove(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_ARENA_TEAM_REMOVE");

    uint32 ArenaTeamId;
    std::string name;

    recv_data >> ArenaTeamId;
    recv_data >> name;

    ArenaTeam* at = sObjectMgr.GetArenaTeamById(ArenaTeamId);
    if (!at)                                                // arena team not found
    {
        return;
    }

    if (at->GetCaptainGuid() != session.GetPlayer()->GetObjectGuid())
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", "", ERR_ARENA_TEAM_PERMISSIONS);
        return;
    }

    if (!normalizePlayerName(name))
    {
        return;
    }

    ArenaTeamMember* member = at->GetMember(name);
    if (!member)                                            // member not found
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", name, ERR_ARENA_TEAM_PLAYER_NOT_FOUND_S);
        return;
    }

    if (at->GetCaptainGuid() == member->guid)
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_QUIT_S, "", "", ERR_ARENA_TEAM_LEADER_LEAVE_S);
        return;
    }

    at->DelMember(member->guid);

    // event
    at->BroadcastEvent(ERR_ARENA_TEAM_REMOVE_SSS, name.c_str(), at->GetName().c_str(), session.GetPlayer()->GetName());
}

void PvpHandlers::HandleArenaTeamLeader(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_ARENA_TEAM_LEADER");

    uint32 ArenaTeamId;
    std::string name;

    recv_data >> ArenaTeamId;
    recv_data >> name;

    ArenaTeam* at = sObjectMgr.GetArenaTeamById(ArenaTeamId);
    if (!at)                                                // arena team not found
    {
        return;
    }

    if (at->GetCaptainGuid() != session.GetPlayer()->GetObjectGuid())
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", "", ERR_ARENA_TEAM_PERMISSIONS);
        return;
    }

    if (!normalizePlayerName(name))
    {
        return;
    }

    ArenaTeamMember* member = at->GetMember(name);
    if (!member)                                            // member not found
    {
        session.SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", name, ERR_ARENA_TEAM_PLAYER_NOT_FOUND_S);
        return;
    }

    if (at->GetCaptainGuid() == member->guid)               // target player already captain
    {
        return;
    }

    at->SetCaptain(member->guid);

    // event
    at->BroadcastEvent(ERR_ARENA_TEAM_LEADER_CHANGED_SSS, session.GetPlayer()->GetName(), name.c_str(), at->GetName().c_str());
}
