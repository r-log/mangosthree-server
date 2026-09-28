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

#ifndef MANGOS_H_MANGOS_SOCIALLIST
#define MANGOS_H_MANGOS_SOCIALLIST

#include <string>
#include <map>
#include <functional>
#include "Platform/Define.h"
#include "Common/ServerDefines.h"                           // AccountTypes
#include "SharedDefines.h"                                  // Team
#include "ObjectGuid.h"
#include "ManagerPacketSink.h"

class SocialMgr;
class WorldPacket;
class Field;

enum FriendStatus
{
    FRIEND_STATUS_OFFLINE   = 0,
    FRIEND_STATUS_ONLINE    = 1,
    FRIEND_STATUS_AFK       = 2,
    FRIEND_STATUS_UNK3      = 3,
    FRIEND_STATUS_DND       = 4
};

enum SocialFlag
{
    SOCIAL_FLAG_FRIEND      = 0x01,
    SOCIAL_FLAG_IGNORED     = 0x02,
    SOCIAL_FLAG_MUTED       = 0x04,                         // guessed
    SOCIAL_FLAG_RAF         = 0x08                          // Recruit-A-Friend
};

struct FriendInfo
{
    FriendStatus Status;
    uint32 Flags;
    uint32 Area;
    uint32 Level;
    uint32 Class;
    std::string Note;

    FriendInfo() :
        Status(FRIEND_STATUS_OFFLINE),
        Flags(0),
        Area(0),
        Level(0),
        Class(0)
    {}

    FriendInfo(uint32 flags, const std::string& note) :
        Status(FRIEND_STATUS_OFFLINE),
        Flags(flags),
        Area(0),
        Level(0),
        Class(0),
        Note(note)
    {}
};

typedef std::map<uint32, FriendInfo> PlayerSocialMap;

/// Results of friend related commands
enum FriendsResult
{
    FRIEND_DB_ERROR         = 0x00,                         // ERR_FRIEND_NOT_FOUND
    FRIEND_LIST_FULL        = 0x01,
    FRIEND_ONLINE           = 0x02,
    FRIEND_OFFLINE          = 0x03,
    FRIEND_NOT_FOUND        = 0x04,                         // ERR_FRIEND_NOT_FOUND
    FRIEND_REMOVED          = 0x05,
    FRIEND_ADDED_ONLINE     = 0x06,                         // ERR_FRIEND_ADDED_S
    FRIEND_ADDED_OFFLINE    = 0x07,
    FRIEND_ALREADY          = 0x08,
    FRIEND_SELF             = 0x09,
    FRIEND_ENEMY            = 0x0A,
    FRIEND_IGNORE_FULL      = 0x0B,
    FRIEND_IGNORE_SELF      = 0x0C,
    FRIEND_IGNORE_NOT_FOUND = 0x0D,
    FRIEND_IGNORE_ALREADY   = 0x0E,
    FRIEND_IGNORE_ADDED     = 0x0F,
    FRIEND_IGNORE_REMOVED   = 0x10,
    FRIEND_IGNORE_AMBIGUOUS = 0x11,                         // That name is ambiguous, type more of the player's server name
    FRIEND_MUTE_FULL        = 0x12,
    FRIEND_MUTE_SELF        = 0x13,
    FRIEND_MUTE_NOT_FOUND   = 0x14,
    FRIEND_MUTE_ALREADY     = 0x15,
    FRIEND_MUTE_ADDED       = 0x16,
    FRIEND_MUTE_REMOVED     = 0x17,
    FRIEND_MUTE_AMBIGUOUS   = 0x18,                         // ERR_VOICE_IGNORE_AMBIGUOUS
    FRIEND_UNK7             = 0x19,                         // ERR_MAX_VALUE (nothing is showed)
    FRIEND_UNKNOWN          = 0x1A                          // Unknown friend response from server
};

#define SOCIALMGR_FRIEND_LIMIT  50
#define SOCIALMGR_IGNORE_LIMIT  50

/// Fills one entry with what the requester may see of the listed character: the note from the
/// requester's own list, and the status, area, level and class (or offline and zeros). The owner
/// side passes `SocialMgr::GetFriendInfo`, which reads the other character; a test passes a table.
typedef std::function<void(uint32 friendLowGuid, FriendInfo& friendInfo)> FriendInfoFill;

/**
 * @brief Decoupling D4k: one character's friend and ignore list, and the rules over it, held
 * apart from the object that plays the character.
 *
 * The state is the list (friend low guid -> flags, note, and the last status the list packet
 * filled in) and the owner's low guid, which every statement names. The rules: the 50 friend and
 * 50 ignore limits; a second flag on a listed character is ORed into its entry (an UPDATE), a new
 * one is an INSERT; removing a flag keeps the entry while another flag is left (an UPDATE) and
 * deletes it with the last one (a DELETE); a note is set only on a listed character, cut to 48
 * characters; the login rows are taken one at a time (LoadRow) under the same two limits. The
 * statements go to the character database asynchronously, as before. It builds the contact list
 * packet (`SMSG_CONTACT_LIST`); the friend status packet and the online-visibility verdict below
 * are free functions in this file.
 *
 * The object carries no owner and reads no other character. What the list packet needs from the
 * other characters comes in through a FriendInfoFill called for each entry at the old point, and
 * the packet goes to a ManagerPacketSink. So `mangos_tests` builds one from nothing.
 *
 * What stays global and on the owner side, and why (`SocialMgr`, in `SocialMgr.h/.cpp`): the map
 * of every loaded character's list (it is cross-character: a status change is sent to every
 * character whose list names the changed one, so the broadcast walks every list); the login loop
 * over the rows; the registry lookups of other characters and every read of them (team, session
 * security, AFK/DND, zone, level, class); the recipients of each packet (the requester's own
 * session, or each lister's session in turn); and the no-argument SendSocialList below, which is
 * declared here for its callers but defined in `SocialMgr.cpp`: it finds the character by guid
 * (and sends nothing when the lookup finds nobody) and hands this class the fill and the sink.
 *
 * KEPT SEMANTICS, stated rather than fixed (backlog): the list packet writes each entry's status,
 * area, level and class back into the stored entry (the fill receives the stored entry itself);
 * a loaded row with both flags counts as an ignore only, while each limit check reads its own
 * flag; the destructor clears a map that is about to be destroyed anyway.
 */
class PlayerSocial
{
        friend class SocialMgr;
    public:
        PlayerSocial();
        ~PlayerSocial();
        // adding/removing
        bool AddToSocialList(ObjectGuid friend_guid, bool ignore);
        void RemoveFromSocialList(ObjectGuid friend_guid, bool ignore);
        void SetFriendNote(ObjectGuid friend_guid, std::string note);
        // Packet send's
        /// Defined in SocialMgr.cpp, on the owner side, because two callers that may not change call it
        /// without arguments (the contact list handler in MiscHandlerSocial.cpp, and the character's
        /// SendInitialPacketsBeforeAddToMap). Not for tests: it finds the character by guid, finds none in
        /// mangos_tests and sends nothing -- call SendSocialList(fill, send) instead. The one entry in
        /// CheckManagerIsolation.cmake's OWNER_DEFINED allowlist.
        void SendSocialList();
        /**
         * @brief Builds `SMSG_CONTACT_LIST` from the list and hands it to `send`.
         *
         * @param fill Fills each entry, in the list's order, before the entry is written.
         * @param send Where the packet goes: the owner's session.
         */
        void SendSocialList(FriendInfoFill const& fill, ManagerPacketSink const& send);
        // Misc
        bool HasFriend(ObjectGuid friend_guid) const;
        bool HasIgnore(ObjectGuid ignore_guid) const;
        void SetPlayerGuid(ObjectGuid guid) { m_playerLowGuid = guid.GetCounter(); }
        uint32 GetNumberOfSocialsWithFlag(SocialFlag flag) const;
        // Loading
        /**
         * @brief Loads one row of the login holder's social result (friend, flags, note).
         *
         * A row over its limit is skipped. The owner's loop keeps the two counters across the rows.
         *
         * @param fields        The row.
         * @param friendCounter The friend rows taken so far.
         * @param ignoreCounter The ignore rows taken so far.
         */
        void LoadRow(Field* fields, uint32& friendCounter, uint32& ignoreCounter);
    private:
        PlayerSocialMap m_playerSocialMap;
        uint32 m_playerLowGuid;
};

/**
 * @brief What the online-visibility verdict reads: whether a viewer may see a listed character
 * online. The owner side reads them from the two characters; `present` false means nothing else
 * was read (the seen fields keep these defaults).
 */
struct FriendVisibility
{
    bool present = false;                                   ///< the character was found online (the caller's own presence test)
    AccountTypes viewerSecurity = SEC_PLAYER;               ///< the viewer's session security
    Team viewerTeam = TEAM_NONE;                            ///< the viewer's team
    AccountTypes seenSecurity = SEC_PLAYER;                 ///< the seen character's session security
    Team seenTeam = TEAM_NONE;                              ///< the seen character's team
    bool allowTwoSideWhoList = false;                       ///< CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST
    AccountTypes gmLevelInWhoList = SEC_PLAYER;             ///< CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST
    bool seenVisibleGlobally = false;                       ///< the seen character's IsVisibleGloballyFor(viewer)
};

/**
 * @brief The online-visibility verdict. A viewer above SEC_PLAYER sees everyone; any other viewer
 * sees a character of the same team (or any team with the two-side config) whose security is at
 * most the GM-in-who-list level; and the character must be present and globally visible to the
 * viewer either way.
 */
bool IsFriendVisibleOnline(FriendVisibility const& facts);

/**
 * @brief Builds a friend status packet header.
 *
 * @param result The friend status result code.
 * @param guid The low GUID of the related player.
 * @param data The packet to initialize.
 */
void MakeFriendStatusPacket(FriendsResult result, uint32 guid, WorldPacket* data);

/**
 * @brief Builds the whole `SMSG_FRIEND_STATUS`: the header, then the note for the two ADDED
 * results, then status, area, level and class for ADDED_ONLINE and ONLINE.
 *
 * @param result The friend status result code.
 * @param friend_lowguid The low GUID of the related character.
 * @param fill Fills the related character's entry once, after the header.
 * @param data The packet to build.
 */
void BuildFriendStatusPacket(FriendsResult result, uint32 friend_lowguid, FriendInfoFill const& fill, WorldPacket& data);

#endif
