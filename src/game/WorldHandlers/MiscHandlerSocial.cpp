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

#include <zlib.h>
#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include <string>
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "World.h"
#include "CinematicFlyover.h"
#include "GuildMgr.h"
#include "ObjectMgr.h"
#include "WorldSession.h"
#include "Auth/BigNumber.h"
#include "Auth/Sha1.h"
#include "UpdateData.h"
#include "LootMgr.h"
#include "Chat.h"
#include "ScriptMgr.h"
#include "zlib.h"
#include "PlayerRegistry.h"
#include "CharacterCache.h"
#include "Object.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Guild.h"
#include "Pet.h"
#include "SocialMgr.h"
#include "DBCEnums.h"

/**
 * @file MiscHandlerSocial.cpp
 * @brief Cohesion split of MiscHandler.cpp -- social contact-list opcode handlers: friend/ignore add-remove (with async name lookups) and contact notes. Same WorldSession class; no behaviour change. CMake file(GLOB) picks this file up automatically; WorldSession.h is unchanged.
 */

/**
 * @brief Sends the player's friend list.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleContactListOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_CONTACT_LIST");
    uint32 unk;
    recv_data >> unk;
    DEBUG_LOG("unk value is %u", unk);
    _player->GetSocial()->SendSocialList();
}

/**
 * @brief Starts an asynchronous add-friend lookup.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleAddFriendOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ADD_FRIEND");

    std::string friendName = GetMangosString(LANG_FRIEND_IGNORE_UNKNOWN);
    std::string friendNote;

    recv_data >> friendName;

    recv_data >> friendNote;

    if (!normalizePlayerName(friendName))
    {
        return;
    }

    DEBUG_LOG("WORLD: %s asked to add friend : '%s'",
              GetPlayer()->GetName(), friendName.c_str());

    // Decoupling D7i: the escape and the `SELECT guid, race FROM characters WHERE name`
    // it protected are one CharacterCache lookup (D7c's fold table reproduces the column's
    // utf8_general_ci comparison), so the answer is here in the handler's own tick. No
    // cache entry is the old NULL result: the request is dropped with no reply at all,
    // exactly as before.
    CharacterCacheRef cached = sCharacterCache.GetByName(friendName);
    if (!cached)
    {
        return;
    }

    CompleteAddFriend(cached->guid, Player::TeamForRace(cached->race), friendNote);
}

/**
 * @brief Completes an add-friend request once the character has been resolved.
 *
 * Decoupling D7i: this is the old HandleAddFriendOpcodeCallBack's body, unchanged below
 * the lookup. What it used to do first -- decode the row and re-find the session by
 * account id -- the caller has already done: the row is a CharacterCache entry and the
 * session is this one, because no tick passes between the packet and this call.
 *
 * @param friendGuid The resolved character guid.
 * @param team The resolved character's team.
 * @param friendNote The note the client sent with the request.
 */
void WorldSession::CompleteAddFriend(ObjectGuid friendGuid, Team team, std::string const& friendNote)
{
    WorldSession* session = this;
    if (!session->GetPlayer())
    {
        return;
    }

    FriendsResult friendResult = FRIEND_NOT_FOUND;
    if (friendGuid)
    {
        if (friendGuid == session->GetPlayer()->GetObjectGuid())
        {
            friendResult = FRIEND_SELF;
        }
        else if (session->GetPlayer()->GetTeam() != team && !sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_ADD_FRIEND) && session->GetSecurity() < SEC_MODERATOR)
        {
            friendResult = FRIEND_ENEMY;
        }
        else if (session->GetPlayer()->GetSocial()->HasFriend(friendGuid))
        {
            friendResult = FRIEND_ALREADY;
        }
        else
        {
            Player* pFriend = sPlayerRegistry.Find(friendGuid);
            if (pFriend && pFriend->IsInWorld() && pFriend->IsVisibleGloballyFor(session->GetPlayer()))
            {
                friendResult = FRIEND_ADDED_ONLINE;
            }
            else
            {
                friendResult = FRIEND_ADDED_OFFLINE;
            }

            if (!session->GetPlayer()->GetSocial()->AddToSocialList(friendGuid, false))
            {
                friendResult = FRIEND_LIST_FULL;
                DEBUG_LOG("WORLD: %s's friend list is full.", session->GetPlayer()->GetName());
            }

            session->GetPlayer()->GetSocial()->SetFriendNote(friendGuid, friendNote);
        }
    }

    sSocialMgr.SendFriendStatus(session->GetPlayer(), friendResult, friendGuid, false);

    DEBUG_LOG("WORLD: Sent (SMSG_FRIEND_STATUS)");
}

/**
 * @brief Removes a friend from the player's social list.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleDelFriendOpcode(WorldPacket& recv_data)
{
    ObjectGuid friendGuid;

    DEBUG_LOG("WORLD: Received opcode CMSG_DEL_FRIEND");

    recv_data >> friendGuid;

    _player->GetSocial()->RemoveFromSocialList(friendGuid, false);

    sSocialMgr.SendFriendStatus(GetPlayer(), FRIEND_REMOVED, friendGuid, false);

    DEBUG_LOG("WORLD: Sent motd (SMSG_FRIEND_STATUS)");
}

/**
 * @brief Starts an asynchronous add-ignore lookup.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleAddIgnoreOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ADD_IGNORE");

    std::string IgnoreName = GetMangosString(LANG_FRIEND_IGNORE_UNKNOWN);

    recv_data >> IgnoreName;

    if (!normalizePlayerName(IgnoreName))
    {
        return;
    }

    DEBUG_LOG("WORLD: %s asked to Ignore: '%s'",
              GetPlayer()->GetName(), IgnoreName.c_str());

    // Decoupling D7i, the twin of HandleAddFriendOpcode above.
    CharacterCacheRef cached = sCharacterCache.GetByName(IgnoreName);
    if (!cached)
    {
        return;
    }

    CompleteAddIgnore(cached->guid);
}

/**
 * @brief Completes an add-ignore request once the character has been resolved.
 *
 * Decoupling D7i, the twin of CompleteAddFriend above.
 *
 * @param ignoreGuid The resolved character guid.
 */
void WorldSession::CompleteAddIgnore(ObjectGuid ignoreGuid)
{
    WorldSession* session = this;
    if (!session->GetPlayer())
    {
        return;
    }

    FriendsResult ignoreResult = FRIEND_IGNORE_NOT_FOUND;
    if (ignoreGuid)
    {
        if (ignoreGuid == session->GetPlayer()->GetObjectGuid())
        {
            ignoreResult = FRIEND_IGNORE_SELF;
        }
        else if (session->GetPlayer()->GetSocial()->HasIgnore(ignoreGuid))
        {
            ignoreResult = FRIEND_IGNORE_ALREADY;
        }
        else
        {
            ignoreResult = FRIEND_IGNORE_ADDED;

            // ignore list full
            if (!session->GetPlayer()->GetSocial()->AddToSocialList(ignoreGuid, true))
            {
                ignoreResult = FRIEND_IGNORE_FULL;
            }
        }
    }

    sSocialMgr.SendFriendStatus(session->GetPlayer(), ignoreResult, ignoreGuid, false);

    DEBUG_LOG("WORLD: Sent (SMSG_FRIEND_STATUS)");
}

/**
 * @brief Removes an ignored player from the social list.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleDelIgnoreOpcode(WorldPacket& recv_data)
{
    ObjectGuid ignoreGuid;

    DEBUG_LOG("WORLD: Received opcode CMSG_DEL_IGNORE");

    recv_data >> ignoreGuid;

    _player->GetSocial()->RemoveFromSocialList(ignoreGuid, true);

    sSocialMgr.SendFriendStatus(GetPlayer(), FRIEND_IGNORE_REMOVED, ignoreGuid, false);

    DEBUG_LOG("WORLD: Sent motd (SMSG_FRIEND_STATUS)");
}

void WorldSession::HandleSetContactNotesOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_CONTACT_NOTES");
    ObjectGuid guid;
    std::string note;
    recv_data >> guid >> note;
    _player->GetSocial()->SetFriendNote(guid, note);
}

