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

#include <string>
#include "SocialList.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "Util.h"
#include "Log.h"


/**
 * @brief Creates an empty social list for a player.
 */
PlayerSocial::PlayerSocial(): m_playerLowGuid(0)
{
}

/**
 * @brief Destroys the social list container.
 */
PlayerSocial::~PlayerSocial()
{
    m_playerSocialMap.clear();
}

/* Called by PlayerSocial::SendFriendList */
/**
 * @brief Counts social entries matching a given flag.
 *
 * @param flag The social flag to count.
 * @return The number of matching social entries.
 */
uint32 PlayerSocial::GetNumberOfSocialsWithFlag(SocialFlag flag) const
{
    /* This is the value we return
     * It indicates the number of players that have the flag specified in arg1 */
    uint32 counter = 0;

    /* For each person on our player's social map
     * This includes both friends and enemies */
    for (PlayerSocialMap::const_iterator itr = m_playerSocialMap.begin(); itr != m_playerSocialMap.end(); ++itr)
    {
        if (itr->second.Flags & flag)
        {
            ++counter;
        }
    }
    /* We've done all the calculations we need to, return the counter */
    return counter;
}

/**
 * @brief Adds a friend or ignored player to the social list.
 *
 * @param friend_guid The GUID of the target player.
 * @param ignore true to add to the ignore list; false to add as a friend.
 * @return true if the entry was added or updated; otherwise, false.
 */
bool PlayerSocial::AddToSocialList(ObjectGuid friend_guid, bool ignore)
{
    // check client limits
    if (ignore)
    {
        if (GetNumberOfSocialsWithFlag(SOCIAL_FLAG_IGNORED) >= SOCIALMGR_IGNORE_LIMIT)
        {
            return false;
        }
    }
    else
    {
        if (GetNumberOfSocialsWithFlag(SOCIAL_FLAG_FRIEND) >= SOCIALMGR_FRIEND_LIMIT)
        {
            return false;
        }
    }

    uint32 flag = SOCIAL_FLAG_FRIEND;
    if (ignore)
    {
        flag = SOCIAL_FLAG_IGNORED;
    }

    PlayerSocialMap::const_iterator itr = m_playerSocialMap.find(friend_guid.GetCounter());
    if (itr != m_playerSocialMap.end())
    {
        CharacterDatabase.PExecute("UPDATE `character_social` SET `flags` = (`flags` | %u) WHERE `guid` = '%u' AND `friend` = '%u'", flag, m_playerLowGuid, friend_guid.GetCounter());
        m_playerSocialMap[friend_guid.GetCounter()].Flags |= flag;
    }
    else
    {
        CharacterDatabase.PExecute("INSERT INTO `character_social` (`guid`, `friend`, `flags`) VALUES ('%u', '%u', '%u')", m_playerLowGuid, friend_guid.GetCounter(), flag);
        FriendInfo fi;
        fi.Flags |= flag;
        m_playerSocialMap[friend_guid.GetCounter()] = fi;
    }
    return true;
}

/**
 * @brief Removes a friend or ignored player from the social list.
 *
 * @param friend_guid The GUID of the target player.
 * @param ignore true to remove from the ignore list; false to remove from friends.
 */
void PlayerSocial::RemoveFromSocialList(ObjectGuid friend_guid, bool ignore)
{
    PlayerSocialMap::iterator itr = m_playerSocialMap.find(friend_guid.GetCounter());
    if (itr == m_playerSocialMap.end())                     // not exist
    {
        return;
    }

    uint32 flag = SOCIAL_FLAG_FRIEND;
    if (ignore)
    {
        flag = SOCIAL_FLAG_IGNORED;
    }

    itr->second.Flags &= ~flag;
    if (itr->second.Flags == 0)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_social` WHERE `guid` = '%u' AND `friend` = '%u'", m_playerLowGuid, friend_guid.GetCounter());
        m_playerSocialMap.erase(itr);
    }
    else
    {
        CharacterDatabase.PExecute("UPDATE `character_social` SET `flags` = (`flags` & ~%u) WHERE `guid` = '%u' AND `friend` = '%u'", flag, m_playerLowGuid, friend_guid.GetCounter());
    }
}

void PlayerSocial::SetFriendNote(ObjectGuid friend_guid, std::string note)
{
    PlayerSocialMap::const_iterator itr = m_playerSocialMap.find(friend_guid.GetCounter());
    if (itr == m_playerSocialMap.end())                     // not exist
    {
        return;
    }

    utf8truncate(note, 48);                                 // DB and client size limitation

    // Decoupling D7i: the escaped copy is a bound parameter (C5). The in-memory note below
    // is, and always was, the unescaped string.
    static SqlStatementID updFriendNote;
    SqlStatement update = CharacterDatabase.CreateStatement(updFriendNote,
                          "UPDATE `character_social` SET `note` = ? WHERE `guid` = ? AND `friend` = ?");
    update.addString(note);
    update.addUInt32(m_playerLowGuid);
    update.addUInt32(friend_guid.GetCounter());
    update.Execute();

    m_playerSocialMap[friend_guid.GetCounter()].Note = note;
}

void PlayerSocial::SendSocialList(FriendInfoFill const& fill, ManagerPacketSink const& send)
{
    uint32 size = m_playerSocialMap.size();

    WorldPacket data(SMSG_CONTACT_LIST, (4 + 4 + size * 25)); // just can guess size
    data << uint32(7);                                      // unk flag (0x1, 0x2, 0x4), 0x7 if it include ignore list
    data << uint32(size);                                   // friends count

    for (PlayerSocialMap::iterator itr = m_playerSocialMap.begin(); itr != m_playerSocialMap.end(); ++itr)
    {
        FriendInfo& friendInfo = itr->second;
        fill(itr->first, friendInfo);

        data << ObjectGuid(HIGHGUID_PLAYER, itr->first);    // player guid
        data << uint32(friendInfo.Flags);                  // player flag (0x1-friend?, 0x2-ignored?, 0x4-muted?)
        data << friendInfo.Note;                           // string note
        if (friendInfo.Flags & SOCIAL_FLAG_FRIEND)         // if IsFriend()
        {
            data << uint8(friendInfo.Status);              // online/offline/etc?
            if (friendInfo.Status)                         // if online
            {
                data << uint32(friendInfo.Area);           // player area
                data << uint32(friendInfo.Level);          // player level
                data << uint32(friendInfo.Class);          // player class
            }
        }
    }

    send(&data);
    DEBUG_LOG("WORLD: Sent SMSG_CONTACT_LIST");
}

/**
 * @brief Checks whether a player is on the friend list.
 *
 * @param friend_guid The GUID of the target player.
 * @return true if the player is a friend; otherwise, false.
 */
bool PlayerSocial::HasFriend(ObjectGuid friend_guid) const
{
    PlayerSocialMap::const_iterator itr = m_playerSocialMap.find(friend_guid.GetCounter());
    if (itr != m_playerSocialMap.end())
    {
        return itr->second.Flags & SOCIAL_FLAG_FRIEND;
    }
    return false;
}

/**
 * @brief Checks whether a player is on the ignore list.
 *
 * @param ignore_guid The GUID of the target player.
 * @return true if the player is ignored; otherwise, false.
 */
bool PlayerSocial::HasIgnore(ObjectGuid ignore_guid) const
{
    PlayerSocialMap::const_iterator itr = m_playerSocialMap.find(ignore_guid.GetCounter());
    if (itr != m_playerSocialMap.end())
    {
        return itr->second.Flags & SOCIAL_FLAG_IGNORED;
    }
    return false;
}

void PlayerSocial::LoadRow(Field* fields, uint32& friendCounter, uint32& ignoreCounter)
{
    uint32 friend_guid = 0;
    uint32 flags = 0;
    std::string note = "";

    friend_guid = fields[0].GetUInt32();
    flags = fields[1].GetUInt32();
    note = fields[2].GetCppString();

    if ((flags & SOCIAL_FLAG_IGNORED) && ignoreCounter >= SOCIALMGR_IGNORE_LIMIT)
    {
        return;
    }
    if ((flags & SOCIAL_FLAG_FRIEND) && friendCounter >= SOCIALMGR_FRIEND_LIMIT)
    {
        return;
    }

    m_playerSocialMap[friend_guid] = FriendInfo(flags, note);

    if (flags & SOCIAL_FLAG_IGNORED)
    {
        ++ignoreCounter;
    }
    else
    {
        ++friendCounter;
    }
}

bool IsFriendVisibleOnline(FriendVisibility const& facts)
{
    // PLAYER see his team only and PLAYER can't see MODERATOR, GAME MASTER, ADMINISTRATOR characters
    // MODERATOR, GAME MASTER, ADMINISTRATOR can see all
    return facts.present &&
           (facts.viewerSecurity > SEC_PLAYER ||
            ((facts.seenTeam == facts.viewerTeam || facts.allowTwoSideWhoList) && (facts.seenSecurity <= facts.gmLevelInWhoList))) &&
           facts.seenVisibleGlobally;
}

/**
 * @brief Builds a friend status packet header.
 *
 * @param result The friend status result code.
 * @param guid The low GUID of the related player.
 * @param data The packet to initialize.
 */
void MakeFriendStatusPacket(FriendsResult result, uint32 guid, WorldPacket* data)
{
    data->Initialize(SMSG_FRIEND_STATUS, 5);
    *data << uint8(result);
    *data << ObjectGuid(HIGHGUID_PLAYER, guid);
}

void BuildFriendStatusPacket(FriendsResult result, uint32 friend_lowguid, FriendInfoFill const& fill, WorldPacket& data)
{
    FriendInfo fi;

    MakeFriendStatusPacket(result, friend_lowguid, &data);
    fill(friend_lowguid, fi);
    switch (result)
    {
        case FRIEND_ADDED_OFFLINE:
        case FRIEND_ADDED_ONLINE:
            data << fi.Note;
            break;
        default:
            break;
    }

    switch (result)
    {
        case FRIEND_ADDED_ONLINE:
        case FRIEND_ONLINE:
            data << uint8(fi.Status);
            data << uint32(fi.Area);
            data << uint32(fi.Level);
            data << uint32(fi.Class);
            break;
        default:
            break;
    }
}
