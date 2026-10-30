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

#ifndef MANGOS_H_MANGOS_SOCIALMGR
#define MANGOS_H_MANGOS_SOCIALMGR

#include <string>
#include <map>
#include <vector>
#include "Database/DatabaseEnv.h"
#include "ObjectGuid.h"
#include "SocialList.h"                                     // PlayerSocial, FriendInfo, the enums and the facts

class Player;
class PlayerRegistry;
struct SocialRow;

typedef std::map<uint32, PlayerSocial> SocialMap;

/**
 * @brief The global over every loaded character's social list, the owner side of
 * `social/SocialList`.
 *
 * It is cross-character, so it is not a member of the character: it owns the map of lists
 * (created at login by LoadFromDB, which keeps the loop over the rows and hands each row to
 * PlayerSocial::LoadRow; erased at logout), finds other characters through the player registry it
 * is built with,
 * reads what the verdict and the list need from them, and reports each friend status fact to the
 * own client of each recipient. The rules it applies live in `SocialList`: the online-visibility
 * verdict (IsFriendVisibleOnline) and the friend status fact (MakeFriendStatusFact).
 *
 * PlayerSocial befriends this class (`friend class SocialMgr`), which grants write access to every
 * list's private map; it only reads through it (GetFriendInfo reads the requester's note,
 * BroadcastToFriendListers each lister's flags, SendSocialList the owner's guid; LoadFromDB goes
 * through LoadRow). The world builds the manager and installs it; the code reaches it through
 * sSocialMgr.
 */
class SocialMgr
{
    public:
        explicit SocialMgr(PlayerRegistry& players);
        ~SocialMgr();

        /// The installed social manager; asserts when none is installed.
        static SocialMgr& Installed();
        /// Installs `mgr` and answers the one it replaces (NULL for none).
        static SocialMgr* Install(SocialMgr* mgr);

        // Misc
        void RemovePlayerSocial(uint32 guid) { m_socialMap.erase(guid); }

        void GetFriendInfo(Player* player, uint32 friendGUID, FriendInfo& friendInfo);
        /// Tells the owner of `social`, when it is online, its contact list.
        void SendSocialList(PlayerSocial& social);
        // Packet management
        void SendFriendStatus(Player* player, FriendsResult result, ObjectGuid friend_guid, bool broadcast);
        void BroadcastToFriendListers(Player* player, FriendStatusFact const& fact);
        // Loading
        PlayerSocial* LoadFromDB(std::vector<SocialRow> const& rows, ObjectGuid guid);
    private:
        PlayerRegistry& m_players;
        SocialMap m_socialMap;
};

#define sSocialMgr SocialMgr::Installed()
#endif
