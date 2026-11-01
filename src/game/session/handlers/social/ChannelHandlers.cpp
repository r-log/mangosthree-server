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

#include "session/handlers/social/ChannelHandlers.h"

#include <string>
#include "Object/ObjectMgr.h"
#include "WorldHandlers/ChannelMgr.h"
#include "session/WorldSession.h"
#include "session/OpcodeTable.h"

/**
 * @brief Handles a client's request to join a chat channel.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleJoinChannel(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());

    uint32 channel_id;
    std::string channelname, pass;

    recvPacket >> channel_id;
    recvPacket.ReadBit();       // has voice
    recvPacket.ReadBit();       // zone update

    uint8 channelLength = recvPacket.ReadBits(8);
    uint8 passwordLength = recvPacket.ReadBits(8);
    channelname = recvPacket.ReadString(channelLength);
    pass = recvPacket.ReadString(passwordLength);

    if (channelname.empty())
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
        if (Channel* chn = cMgr->GetJoinChannel(channelname, channel_id))
        {
            chn->Join(session.GetPlayer(), pass.c_str());
        }
}

/**
 * @brief Handles a client's request to leave a chat channel.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleLeaveChannel(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();

    uint32 unk;
    std::string channelname;
    recvPacket >> unk;                                      // channel id?
    channelname = recvPacket.ReadString(recvPacket.ReadBits(8));

    if (channelname.empty())
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
    {
        if (Channel* chn = cMgr->GetChannel(channelname, session.GetPlayer()))
        {
            chn->Leave(session.GetPlayer(), true);
        }
        cMgr->LeftChannel(channelname);
    }
}

/**
 * @brief Sends the member list for a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleChannelList(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname = recvPacket.ReadString(recvPacket.ReadBits(8));

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
        if (Channel* chn = cMgr->GetChannel(channelname, session.GetPlayer()))
        {
            chn->List(session.GetPlayer());
        }
}

/**
 * @brief Changes the password for a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleChannelPassword(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    uint32 nameLen, passLen;
    std::string channelname, pass;

    nameLen = recvPacket.ReadBits(8);
    passLen = recvPacket.ReadBits(7);
    channelname = recvPacket.ReadString(nameLen);
    pass = recvPacket.ReadString(passLen);

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
        if (Channel* chn = cMgr->GetChannel(channelname, session.GetPlayer()))
        {
            chn->Password(session.GetPlayer(), pass.c_str());
        }
}

/**
 * @brief Sets a new owner for a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleChannelSetOwner(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();

    uint32 channelLen, nameLen;
    std::string channelname, newp;

    channelLen = recvPacket.ReadBits(8);
    nameLen = recvPacket.ReadBits(7);
    newp = recvPacket.ReadString(nameLen);
    channelname = recvPacket.ReadString(channelLen);

    if (!normalizePlayerName(newp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
        if (Channel* chn = cMgr->GetChannel(channelname, session.GetPlayer()))
        {
            chn->SetOwner(session.GetPlayer(), newp.c_str());
        }
}

/**
 * @brief Requests the current owner of a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleChannelOwner(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname = recvPacket.ReadString(recvPacket.ReadBits(8));

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
        if (Channel* chn = cMgr->GetChannel(channelname, session.GetPlayer()))
        {
            chn->SendWhoOwner(session.GetPlayer());
        }
}

/**
 * @brief Grants moderator privileges to a channel member.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleChannelModerator(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    uint32 channelLen, nameLen;
    std::string channelname, otp;

    channelLen = recvPacket.ReadBits(8);
    nameLen = recvPacket.ReadBits(7);
    otp = recvPacket.ReadString(nameLen);
    channelname = recvPacket.ReadString(channelLen);

    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
        if (Channel* chn = cMgr->GetChannel(channelname, session.GetPlayer()))
        {
            chn->SetModerator(session.GetPlayer(), otp.c_str());
        }
}

/**
 * @brief Removes moderator privileges from a channel member.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleChannelUnmoderator(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    uint32 channelLen, nameLen;
    std::string channelname, otp;

    nameLen = recvPacket.ReadBits(7);
    channelLen = recvPacket.ReadBits(8);
    channelname = recvPacket.ReadString(channelLen);
    otp = recvPacket.ReadString(nameLen);

    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
        if (Channel* chn = cMgr->GetChannel(channelname, session.GetPlayer()))
        {
            chn->UnsetModerator(session.GetPlayer(), otp.c_str());
        }
}

/**
 * @brief Mutes a member in a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleChannelMute(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    uint32 channelLen, nameLen;
    std::string channelname, otp;

    channelLen = recvPacket.ReadBits(8);
    nameLen = recvPacket.ReadBits(7);
    channelname = recvPacket.ReadString(channelLen);
    otp = recvPacket.ReadString(nameLen);

    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
        if (Channel* chn = cMgr->GetChannel(channelname, session.GetPlayer()))
        {
            chn->SetMute(session.GetPlayer(), otp.c_str());
        }
}

/**
 * @brief Unmutes a member in a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleChannelUnmute(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    uint32 channelLen, nameLen;
    std::string channelname, otp;

    channelLen = recvPacket.ReadBits(8);
    nameLen = recvPacket.ReadBits(7);
    channelname = recvPacket.ReadString(channelLen);
    otp = recvPacket.ReadString(nameLen);

    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
        if (Channel* chn = cMgr->GetChannel(channelname, session.GetPlayer()))
        {
            chn->UnsetMute(session.GetPlayer(), otp.c_str());
        }
}

/**
 * @brief Invites another player to a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleChannelInvite(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    uint32 channelLen, nameLen;
    std::string channelname, otp;

    nameLen = recvPacket.ReadBits(7);
    channelLen = recvPacket.ReadBits(8);
    otp = recvPacket.ReadString(nameLen);
    channelname = recvPacket.ReadString(channelLen);

    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
        if (Channel* chn = cMgr->GetChannel(channelname, session.GetPlayer()))
        {
            chn->Invite(session.GetPlayer(), otp.c_str());
        }
}

/**
 * @brief Kicks a member from a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleChannelKick(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    uint32 channelLen, nameLen;
    std::string channelname, otp;

    channelLen = recvPacket.ReadBits(8);
    nameLen = recvPacket.ReadBits(7);
    channelname = recvPacket.ReadString(channelLen);
    otp = recvPacket.ReadString(nameLen);

    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
        if (Channel* chn = cMgr->GetChannel(channelname, session.GetPlayer()))
        {
            chn->Kick(session.GetPlayer(), otp.c_str());
        }
}

/**
 * @brief Bans a member from a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleChannelBan(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    uint32 channelLen, nameLen;
    std::string channelname, otp;

    channelLen = recvPacket.ReadBits(8);
    nameLen = recvPacket.ReadBits(7);
    otp = recvPacket.ReadString(nameLen);
    channelname = recvPacket.ReadString(channelLen);

    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
        if (Channel* chn = cMgr->GetChannel(channelname, session.GetPlayer()))
        {
            chn->Ban(session.GetPlayer(), otp.c_str());
        }
}

/**
 * @brief Removes a ban for a member in a channel.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleChannelUnban(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    uint32 channelLen, nameLen;
    std::string channelname, otp;

    nameLen = recvPacket.ReadBits(7);
    channelLen = recvPacket.ReadBits(8);
    channelname = recvPacket.ReadString(channelLen);
    otp = recvPacket.ReadString(nameLen);

    if (!normalizePlayerName(otp))
    {
        return;
    }

    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
        if (Channel* chn = cMgr->GetChannel(channelname, session.GetPlayer()))
        {
            chn->UnBan(session.GetPlayer(), otp.c_str());
        }
}

/**
 * @brief Toggles channel join and leave announcements.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleChannelAnnouncements(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    //recvPacket.hexlike();
    std::string channelname = recvPacket.ReadString(recvPacket.ReadBits(8));
    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
        if (Channel* chn = cMgr->GetChannel(channelname, session.GetPlayer()))
        {
            chn->Announce(session.GetPlayer());
        }
}

/**
 * @brief Handles a channel display list query.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleChannelDisplayListQuery(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname = recvPacket.ReadString(recvPacket.ReadBits(8));
    if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
        if (Channel* chn = cMgr->GetChannel(channelname, session.GetPlayer()))
        {
            chn->List(session.GetPlayer());
        }
}

/**
 * @brief Handles a channel watch request from the client.
 *
 * @param recvPacket The received opcode packet.
 */
void ChannelHandlers::HandleSetChannelWatch(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(recvPacket.GetOpcode()), recvPacket.GetOpcode(), recvPacket.GetOpcode());
    // recvPacket.hexlike();
    std::string channelname;
    recvPacket >> channelname;
    /*if (ChannelMgr* cMgr = channelMgr(_player->GetTeam()))
        if (Channel *chn = cMgr->GetChannel(channelname, _player))
        {
            chn->JoinNotify(_player->GetGUID());
        }*/
}
