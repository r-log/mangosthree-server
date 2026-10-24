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

    // The note is a bound parameter; the in-memory note below is the unescaped string.
    static SqlStatementID updFriendNote;
    SqlStatement update = CharacterDatabase.CreateStatement(updFriendNote,
                          "UPDATE `character_social` SET `note` = ? WHERE `guid` = ? AND `friend` = ?");
    update.addString(note);
    update.addUInt32(m_playerLowGuid);
    update.addUInt32(friend_guid.GetCounter());
    update.Execute();

    m_playerSocialMap[friend_guid.GetCounter()].Note = note;
}

void PlayerSocial::SendSocialList(FriendInfoFill const& fill, ContactListSink const& send)
{
    ContactListFact fact;
    fact.entries.reserve(m_playerSocialMap.size());

    for (PlayerSocialMap::iterator itr = m_playerSocialMap.begin(); itr != m_playerSocialMap.end(); ++itr)
    {
        FriendInfo& friendInfo = itr->second;
        fill(itr->first, friendInfo);

        ContactListFact::Entry entry;
        entry.lowGuid = itr->first;
        entry.info = friendInfo;
        fact.entries.push_back(entry);
    }

    send(fact);
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

FriendStatusFact MakeFriendStatusFact(FriendsResult result, uint32 friend_lowguid, FriendInfoFill const& fill)
{
    FriendStatusFact fact;
    fact.result = result;
    fact.friendLowGuid = friend_lowguid;
    fill(friend_lowguid, fact.info);
    return fact;
}
