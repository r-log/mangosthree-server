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

#include "session/handlers/social/ContactHandlers.h"

#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include <string>
#include "data/Language.h"
#include "WorldPacket.h"
#include "Log/Log.h"
#include "entities/player/Player.h"
#include "data/GameConfig.h"
#include "Object/ObjectMgr.h"
#include "session/WorldSession.h"
#include "entities/player/PlayerRegistry.h"
#include "Object/CharacterCache.h"
#include "Server/DBCStores.h"
#include "entities/player/social/SocialMgr.h"

/**
 * @brief Sends the player's friend list.
 *
 * @param recv_data The received opcode packet.
 */
void ContactHandlers::HandleContactList(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_CONTACT_LIST");
    uint32 unk;
    recv_data >> unk;
    DEBUG_LOG("unk value is %u", unk);
    session.GetPlayer()->GetSocial()->SendSocialList();
}

/**
 * @brief Resolves the friend's name through CharacterCache and completes the add.
 *
 * @param recv_data The received opcode packet.
 */
void ContactHandlers::HandleAddFriend(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ADD_FRIEND");

    std::string friendName = session.GetMangosString(LANG_FRIEND_IGNORE_UNKNOWN);
    std::string friendNote;

    recv_data >> friendName;

    recv_data >> friendNote;

    if (!normalizePlayerName(friendName))
    {
        return;
    }

    DEBUG_LOG("WORLD: %s asked to add friend : '%s'",
              session.GetPlayer()->GetName(), friendName.c_str());

    // The name is resolved through CharacterCache, which compares names as the characters
    // table's utf8_general_ci column does, so the answer is here in the handler's own call;
    // a name with no cache entry drops the request with no reply.
    CharacterCacheRef cached = sCharacterCache.GetByName(friendName);
    if (!cached)
    {
        return;
    }

    CompleteAddFriend(session, cached->guid, TeamForRace(cached->race), friendNote);
}

/**
 * @brief Completes an add-friend request once the character has been resolved.
 *
 * The caller resolved the name through CharacterCache in the same call, so the session is
 * the one the request arrived on.
 *
 * @param friendGuid The resolved character guid.
 * @param team The resolved character's team.
 * @param friendNote The note the client sent with the request.
 */
void ContactHandlers::CompleteAddFriend(WorldSession& session, ObjectGuid friendGuid, Team team, std::string const& friendNote)
{
    WorldSession* requestSession = &session;
    if (!requestSession->GetPlayer())
    {
        return;
    }

    FriendsResult friendResult = FRIEND_NOT_FOUND;
    if (friendGuid)
    {
        if (friendGuid == requestSession->GetPlayer()->GetObjectGuid())
        {
            friendResult = FRIEND_SELF;
        }
        else if (requestSession->GetPlayer()->GetTeam() != team && !sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_ADD_FRIEND) && requestSession->GetSecurity() < SEC_MODERATOR)
        {
            friendResult = FRIEND_ENEMY;
        }
        else if (requestSession->GetPlayer()->GetSocial()->HasFriend(friendGuid))
        {
            friendResult = FRIEND_ALREADY;
        }
        else
        {
            Player* pFriend = sPlayerRegistry.Find(friendGuid);
            if (pFriend && pFriend->IsInWorld() && pFriend->IsVisibleGloballyFor(requestSession->GetPlayer()))
            {
                friendResult = FRIEND_ADDED_ONLINE;
            }
            else
            {
                friendResult = FRIEND_ADDED_OFFLINE;
            }

            if (!requestSession->GetPlayer()->GetSocial()->AddToSocialList(friendGuid, false))
            {
                friendResult = FRIEND_LIST_FULL;
                DEBUG_LOG("WORLD: %s's friend list is full.", requestSession->GetPlayer()->GetName());
            }

            requestSession->GetPlayer()->GetSocial()->SetFriendNote(friendGuid, friendNote);
        }
    }

    sSocialMgr.SendFriendStatus(requestSession->GetPlayer(), friendResult, friendGuid, false);

    DEBUG_LOG("WORLD: Sent (SMSG_FRIEND_STATUS)");
}

/**
 * @brief Removes a friend from the player's social list.
 *
 * @param recv_data The received opcode packet.
 */
void ContactHandlers::HandleDelFriend(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid friendGuid;

    DEBUG_LOG("WORLD: Received opcode CMSG_DEL_FRIEND");

    recv_data >> friendGuid;

    session.GetPlayer()->GetSocial()->RemoveFromSocialList(friendGuid, false);

    sSocialMgr.SendFriendStatus(session.GetPlayer(), FRIEND_REMOVED, friendGuid, false);

    DEBUG_LOG("WORLD: Sent motd (SMSG_FRIEND_STATUS)");
}

/**
 * @brief Resolves the ignored player's name through CharacterCache and completes the add.
 *
 * @param recv_data The received opcode packet.
 */
void ContactHandlers::HandleAddIgnore(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ADD_IGNORE");

    std::string IgnoreName = session.GetMangosString(LANG_FRIEND_IGNORE_UNKNOWN);

    recv_data >> IgnoreName;

    if (!normalizePlayerName(IgnoreName))
    {
        return;
    }

    DEBUG_LOG("WORLD: %s asked to Ignore: '%s'",
              session.GetPlayer()->GetName(), IgnoreName.c_str());

    // The name is resolved as the add-friend handler resolves it.
    CharacterCacheRef cached = sCharacterCache.GetByName(IgnoreName);
    if (!cached)
    {
        return;
    }

    CompleteAddIgnore(session, cached->guid);
}

/**
 * @brief Completes an add-ignore request once the character has been resolved.
 *
 * The ignore twin of CompleteAddFriend above.
 *
 * @param ignoreGuid The resolved character guid.
 */
void ContactHandlers::CompleteAddIgnore(WorldSession& session, ObjectGuid ignoreGuid)
{
    WorldSession* requestSession = &session;
    if (!requestSession->GetPlayer())
    {
        return;
    }

    FriendsResult ignoreResult = FRIEND_IGNORE_NOT_FOUND;
    if (ignoreGuid)
    {
        if (ignoreGuid == requestSession->GetPlayer()->GetObjectGuid())
        {
            ignoreResult = FRIEND_IGNORE_SELF;
        }
        else if (requestSession->GetPlayer()->GetSocial()->HasIgnore(ignoreGuid))
        {
            ignoreResult = FRIEND_IGNORE_ALREADY;
        }
        else
        {
            ignoreResult = FRIEND_IGNORE_ADDED;

            // ignore list full
            if (!requestSession->GetPlayer()->GetSocial()->AddToSocialList(ignoreGuid, true))
            {
                ignoreResult = FRIEND_IGNORE_FULL;
            }
        }
    }

    sSocialMgr.SendFriendStatus(requestSession->GetPlayer(), ignoreResult, ignoreGuid, false);

    DEBUG_LOG("WORLD: Sent (SMSG_FRIEND_STATUS)");
}

/**
 * @brief Removes an ignored player from the social list.
 *
 * @param recv_data The received opcode packet.
 */
void ContactHandlers::HandleDelIgnore(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid ignoreGuid;

    DEBUG_LOG("WORLD: Received opcode CMSG_DEL_IGNORE");

    recv_data >> ignoreGuid;

    session.GetPlayer()->GetSocial()->RemoveFromSocialList(ignoreGuid, true);

    sSocialMgr.SendFriendStatus(session.GetPlayer(), FRIEND_IGNORE_REMOVED, ignoreGuid, false);

    DEBUG_LOG("WORLD: Sent motd (SMSG_FRIEND_STATUS)");
}

void ContactHandlers::HandleSetContactNotes(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_CONTACT_NOTES");
    ObjectGuid guid;
    std::string note;
    recv_data >> guid >> note;
    session.GetPlayer()->GetSocial()->SetFriendNote(guid, note);
}
