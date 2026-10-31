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

#include <atomic>
#include <string>
#include "SocialMgr.h"
#include "Utilities/Errors.h"
#include "Database/DatabaseEnv.h"
#include "Player.h"
#include "data/GameConfig.h"
#include "PlayerRegistry.h"
#include "persistence/characters/social/SocialRepository.h"
#include "GridMap.h"


namespace
{
    std::atomic<SocialMgr*> s_installed(NULL);
}

/**
 * @brief Tells the character's own client its contact list.
 *
 * The owner side of PlayerSocial::SendSocialList(fill, send), defined here because the social
 * manager finds the character by guid and fills each entry from the other characters.
 */
void PlayerSocial::SendSocialList()
{
    sSocialMgr.SendSocialList(*this);
}

/**
 * @brief Initializes the social manager.
 *
 * @param players The registry it finds the other characters in.
 */
SocialMgr::SocialMgr(PlayerRegistry& players) : m_players(players)
{
}

SocialMgr& SocialMgr::Installed()
{
    SocialMgr* mgr = s_installed.load(std::memory_order_acquire);
    MANGOS_ASSERT(mgr);
    return *mgr;
}

SocialMgr* SocialMgr::Install(SocialMgr* mgr)
{
    return s_installed.exchange(mgr, std::memory_order_acq_rel);
}

/**
 * @brief Tells the owner of a contact list its list, when the owner is online.
 *
 * Nothing is reported when the lookup finds nobody. The Player-less overload in SocialList.cpp
 * makes the fact.
 *
 * @param social The contact list to send.
 */
void SocialMgr::SendSocialList(PlayerSocial& social)
{
    Player* plr = m_players.Find(ObjectGuid(HIGHGUID_PLAYER, social.m_playerLowGuid));
    if (!plr)
    {
        return;
    }

    social.SendSocialList([this, plr](uint32 friendLowGuid, FriendInfo& friendInfo)
    {
        GetFriendInfo(plr, friendLowGuid, friendInfo);
    },
    [plr](ContactListFact const& fact)
    {
        plr->ReportContactList(fact);
    });
}

/**
 * @brief Destroys the social manager.
 */
SocialMgr::~SocialMgr()
{
}

/**
 * @brief Populates friend information for social notifications.
 *
 * @param player The player requesting friend information.
 * @param friend_lowguid The low GUID of the friend.
 * @param friendInfo The structure to populate.
 */
void SocialMgr::GetFriendInfo(Player* player, uint32 friend_lowguid, FriendInfo& friendInfo)
{
    if (!player)
    {
        return;
    }

    Player* pFriend = m_players.Find(ObjectGuid(HIGHGUID_PLAYER, friend_lowguid));

    Team team = player->GetTeam();
    AccountTypes security = player->GetSession()->GetSecurity();
    bool allowTwoSideWhoList = sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST);
    AccountTypes gmLevelInWhoList = AccountTypes(sGameConfig.getConfig(CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST));

    PlayerSocialMap::iterator itr = player->GetSocial()->m_playerSocialMap.find(friend_lowguid);
    if (itr != player->GetSocial()->m_playerSocialMap.end())
    {
        friendInfo.Note = itr->second.Note;
    }

    // The verdict (IsFriendVisibleOnline, SocialList.cpp) reads the requester as the viewer and
    // the friend as the seen character. The friend's facts are pure field reads (team, session
    // security, IsVisibleGloballyFor), read only when the friend is present.
    FriendVisibility facts;
    facts.present = pFriend && pFriend->GetName();
    facts.viewerSecurity = security;
    facts.viewerTeam = team;
    facts.allowTwoSideWhoList = allowTwoSideWhoList;
    facts.gmLevelInWhoList = gmLevelInWhoList;
    if (facts.present)
    {
        facts.seenTeam = pFriend->GetTeam();
        facts.seenSecurity = pFriend->GetSession()->GetSecurity();
        facts.seenVisibleGlobally = pFriend->IsVisibleGloballyFor(player);
    }

    // PLAYER see his team only and PLAYER can't see MODERATOR, GAME MASTER, ADMINISTRATOR characters
    // MODERATOR, GAME MASTER, ADMINISTRATOR can see all
    if (IsFriendVisibleOnline(facts))
    {
        friendInfo.Status = FRIEND_STATUS_ONLINE;
        if (pFriend->isAFK())
        {
            friendInfo.Status = FRIEND_STATUS_AFK;
        }
        if (pFriend->isDND())
        {
            friendInfo.Status = FRIEND_STATUS_DND;
        }
        friendInfo.Area = pFriend->GetTerrain()->GetZoneId(pFriend->Where().X(), pFriend->Where().Y(), pFriend->Where().Z());
        friendInfo.Level = pFriend->getLevel();
        friendInfo.Class = pFriend->getClass();
    }
    else
    {
        friendInfo.Status = FRIEND_STATUS_OFFLINE;
        friendInfo.Area = 0;
        friendInfo.Level = 0;
        friendInfo.Class = 0;
    }
}

/**
 * @brief Sends a friend status update to one player or all listers.
 *
 * @param player The player associated with the update.
 * @param result The friend status result code.
 * @param friend_guid The GUID of the related friend.
 * @param broadcast true to send the update to all friend listers; otherwise, false.
 */
void SocialMgr::SendFriendStatus(Player* player, FriendsResult result, ObjectGuid friend_guid, bool broadcast)
{
    uint32 friend_lowguid = friend_guid.GetCounter();

    FriendStatusFact fact = MakeFriendStatusFact(result, friend_lowguid, [this, player](uint32 lowguid, FriendInfo& fi)
    {
        GetFriendInfo(player, lowguid, fi);
    });

    if (broadcast)
    {
        BroadcastToFriendListers(player, fact);
    }
    else
    {
        player->ReportFriendStatus(fact);
    }
}

/**
 * @brief Reports a friend status fact to the own client of each player who lists this player as
 * a friend and may see it online.
 *
 * @param player The player whose status changed.
 * @param fact The fact to report.
 */
void SocialMgr::BroadcastToFriendListers(Player* player, FriendStatusFact const& fact)
{
    if (!player)
    {
        return;
    }

    Team team = player->GetTeam();
    AccountTypes security = player->GetSession()->GetSecurity();
    uint32 guid     = player->GetGUIDLow();
    AccountTypes gmLevelInWhoList = AccountTypes(sGameConfig.getConfig(CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST));
    bool allowTwoSideWhoList = sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST);

    for (SocialMap::const_iterator itr = m_socialMap.begin(); itr != m_socialMap.end(); ++itr)
    {
        PlayerSocialMap::const_iterator itr2 = itr->second.m_playerSocialMap.find(guid);
        if (itr2 != itr->second.m_playerSocialMap.end() && (itr2->second.Flags & SOCIAL_FLAG_FRIEND))
        {
            Player* pFriend = m_players.Find(ObjectGuid(HIGHGUID_PLAYER, itr->first));

            // The same verdict with the roles swapped: the lister is the viewer, the changed
            // character the seen one. The lister's facts are pure field reads (session security,
            // team, and the changed character's IsVisibleGloballyFor(lister)), read only when the
            // lister is present.
            FriendVisibility facts;
            facts.present = pFriend && pFriend->IsInWorld();
            facts.seenSecurity = security;
            facts.seenTeam = team;
            facts.allowTwoSideWhoList = allowTwoSideWhoList;
            facts.gmLevelInWhoList = gmLevelInWhoList;
            if (facts.present)
            {
                facts.viewerSecurity = pFriend->GetSession()->GetSecurity();
                facts.viewerTeam = pFriend->GetTeam();
                facts.seenVisibleGlobally = player->IsVisibleGloballyFor(pFriend);
            }

            // PLAYER see his team only and PLAYER can't see MODERATOR, GAME MASTER, ADMINISTRATOR characters
            // MODERATOR, GAME MASTER, ADMINISTRATOR can see all
            if (IsFriendVisibleOnline(facts))
            {
                pFriend->ReportFriendStatus(fact);
            }
        }
    }
}

/**
 * @brief Loads a player's social list from database rows.
 *
 * @param rows The social rows.
 * @param guid The player GUID owning the social data.
 * @return The loaded player social record.
 */
PlayerSocial* SocialMgr::LoadFromDB(std::vector<SocialRow> const& rows, ObjectGuid guid)
{
    PlayerSocial* social = &m_socialMap[guid.GetCounter()];
    social->SetPlayerGuid(guid);

    if (rows.empty())
    {
        return social;
    }

    // used to speed up check below. Using GetNumberOfSocialsWithFlag will cause unneeded iteration
    uint32 friendCounter = 0, ignoreCounter = 0;

    for (SocialRow const& row : rows)
    {
        social->LoadRow(row, friendCounter, ignoreCounter);
    }
    return social;
}
