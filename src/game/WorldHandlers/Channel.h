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

#ifndef MANGOS_H_CHANNEL
#define MANGOS_H_CHANNEL

#include "Platform/Define.h"
#include <list>
#include <string>
#include "ObjectGuid.h"
#include "Player.h"

#include <map>

enum ChatNotify
{
    CHAT_JOINED_NOTICE                = 0x00,               //+ "%s joined channel.";
    CHAT_LEFT_NOTICE                  = 0x01,               //+ "%s left channel.";
    // CHAT_SUSPENDED_NOTICE           = 0x01,            // "%s left channel.";
    CHAT_YOU_JOINED_NOTICE            = 0x02,               //+ "Joined Channel: [%s]"; -- You joined
    // CHAT_YOU_CHANGED_NOTICE         = 0x02,            // "Changed Channel: [%s]";
    CHAT_YOU_LEFT_NOTICE              = 0x03,               //+ "Left Channel: [%s]"; -- You left
    CHAT_WRONG_PASSWORD_NOTICE        = 0x04,               //+ "Wrong password for %s.";
    CHAT_NOT_MEMBER_NOTICE            = 0x05,               //+ "Not on channel %s.";
    CHAT_NOT_MODERATOR_NOTICE         = 0x06,               //+ "Not a moderator of %s.";
    CHAT_PASSWORD_CHANGED_NOTICE      = 0x07,               //+ "[%s] Password changed by %s.";
    CHAT_OWNER_CHANGED_NOTICE         = 0x08,               //+ "[%s] Owner changed to %s.";
    CHAT_PLAYER_NOT_FOUND_NOTICE      = 0x09,               //+ "[%s] Player %s was not found.";
    CHAT_NOT_OWNER_NOTICE             = 0x0A,               //+ "[%s] You are not the channel owner.";
    CHAT_CHANNEL_OWNER_NOTICE         = 0x0B,               //+ "[%s] Channel owner is %s.";
    CHAT_MODE_CHANGE_NOTICE           = 0x0C,               //?
    CHAT_ANNOUNCEMENTS_ON_NOTICE      = 0x0D,               //+ "[%s] Channel announcements enabled by %s.";
    CHAT_ANNOUNCEMENTS_OFF_NOTICE     = 0x0E,               //+ "[%s] Channel announcements disabled by %s.";
    CHAT_MODERATION_ON_NOTICE         = 0x0F,               //+ "[%s] Channel moderation enabled by %s.";
    CHAT_MODERATION_OFF_NOTICE        = 0x10,               //+ "[%s] Channel moderation disabled by %s.";
    CHAT_MUTED_NOTICE                 = 0x11,               //+ "[%s] You do not have permission to speak.";
    CHAT_PLAYER_KICKED_NOTICE         = 0x12,               //? "[%s] Player %s kicked by %s.";
    CHAT_BANNED_NOTICE                = 0x13,               //+ "[%s] You are banned from that channel.";
    CHAT_PLAYER_BANNED_NOTICE         = 0x14,               //? "[%s] Player %s banned by %s.";
    CHAT_PLAYER_UNBANNED_NOTICE       = 0x15,               //? "[%s] Player %s unbanned by %s.";
    CHAT_PLAYER_NOT_BANNED_NOTICE     = 0x16,               //+ "[%s] Player %s is not banned.";
    CHAT_PLAYER_ALREADY_MEMBER_NOTICE = 0x17,               //+ "[%s] Player %s is already on the channel.";
    CHAT_INVITE_NOTICE                = 0x18,               //+ "%2$s has invited you to join the channel '%1$s'.";
    CHAT_INVITE_WRONG_FACTION_NOTICE  = 0x19,               //+ "Target is in the wrong alliance for %s.";
    CHAT_WRONG_FACTION_NOTICE         = 0x1A,               //+ "Wrong alliance for %s.";
    CHAT_INVALID_NAME_NOTICE          = 0x1B,               //+ "Invalid channel name";
    CHAT_NOT_MODERATED_NOTICE         = 0x1C,               //+ "%s is not moderated";
    CHAT_PLAYER_INVITED_NOTICE        = 0x1D,               //+ "[%s] You invited %s to join the channel";
    CHAT_PLAYER_INVITE_BANNED_NOTICE  = 0x1E,               //+ "[%s] %s has been banned.";
    CHAT_THROTTLED_NOTICE             = 0x1F,               //+ "[%s] The number of messages that can be sent to this channel is limited, please wait to send another message.";
    CHAT_NOT_IN_AREA_NOTICE           = 0x20,               //+ "[%s] You are not in the correct area for this channel."; -- The user is trying to send a chat to a zone specific channel, and they're not physically in that zone.
    CHAT_NOT_IN_LFG_NOTICE            = 0x21,               //+ "[%s] You must be queued in looking for group before joining this channel."; -- The user must be in the looking for group system to join LFG chat channels.
    CHAT_VOICE_ON_NOTICE              = 0x22,               //+ "[%s] Channel voice enabled by %s.";
    CHAT_VOICE_OFF_NOTICE             = 0x23                //+ "[%s] Channel voice disabled by %s.";
    // 0x24 enable voice?
};

class WorldPacket;

/// One channel notice: its type and the fields that type carries. `guid` is the player the notice
/// names (the target of a kick, ban or unban), `source` who kicked, banned or unbanned, `name` a
/// player's name (the owner's for CHAT_CHANNEL_OWNER_NOTICE), `flags` the channel's flags
/// (CHAT_YOU_JOINED_NOTICE) or the player's new flags and `oldFlags` its old ones
/// (CHAT_MODE_CHANGE_NOTICE), and `channelId` the channel's id (CHAT_YOU_JOINED_NOTICE,
/// CHAT_YOU_LEFT_NOTICE).
struct ChannelNotice
{
    ChatNotify type = CHAT_JOINED_NOTICE;
    ObjectGuid guid;
    ObjectGuid source;
    std::string name;
    uint8 flags = 0;
    uint8 oldFlags = 0;
    uint32 channelId = 0;
};

class Channel
{
    enum ChannelFlags
    {
        CHANNEL_FLAG_NONE       = 0x00,
        CHANNEL_FLAG_CUSTOM     = 0x01,
        // 0x02
        CHANNEL_FLAG_TRADE      = 0x04,
        CHANNEL_FLAG_NOT_LFG    = 0x08,
        CHANNEL_FLAG_GENERAL    = 0x10,
        CHANNEL_FLAG_CITY       = 0x20,
        CHANNEL_FLAG_LFG        = 0x40,
        CHANNEL_FLAG_VOICE      = 0x80
                                  // General                  0x18 = 0x10 | 0x08
                                  // Trade                    0x3C = 0x20 | 0x10 | 0x08 | 0x04
                                  // LocalDefence             0x18 = 0x10 | 0x08
                                  // GuildRecruitment         0x38 = 0x20 | 0x10 | 0x08
                                  // LookingForGroup          0x50 = 0x40 | 0x10
    };

    enum ChannelDBCFlags
    {
        CHANNEL_DBC_FLAG_NONE       = 0x00000,
        CHANNEL_DBC_FLAG_INITIAL    = 0x00001,          // General, Trade, LocalDefense, LFG
        CHANNEL_DBC_FLAG_ZONE_DEP   = 0x00002,          // General, Trade, LocalDefense, GuildRecruitment
        CHANNEL_DBC_FLAG_GLOBAL     = 0x00004,          // WorldDefense
        CHANNEL_DBC_FLAG_TRADE      = 0x00008,          // Trade
        CHANNEL_DBC_FLAG_CITY_ONLY  = 0x00010,          // Trade, GuildRecruitment
        CHANNEL_DBC_FLAG_CITY_ONLY2 = 0x00020,          // Trade, GuildRecruitment
        CHANNEL_DBC_FLAG_DEFENSE    = 0x10000,          // LocalDefense, WorldDefense
        CHANNEL_DBC_FLAG_GUILD_REQ  = 0x20000,          // GuildRecruitment
        CHANNEL_DBC_FLAG_LFG        = 0x40000           // LookingForGroup
    };

    enum ChannelMemberFlags
    {
        MEMBER_FLAG_NONE        = 0x00,
        MEMBER_FLAG_OWNER       = 0x01,
        MEMBER_FLAG_MODERATOR   = 0x02,
        MEMBER_FLAG_VOICED      = 0x04,
        MEMBER_FLAG_MUTED       = 0x08,
        MEMBER_FLAG_CUSTOM      = 0x10,
        MEMBER_FLAG_MIC_MUTED   = 0x20,
        // 0x40
        // 0x80
    };

    struct PlayerInfo
    {
        ObjectGuid player;
        uint8 flags;

        bool HasFlag(uint8 flag) { return flags & flag; }
        void SetFlag(uint8 flag) { if (!HasFlag(flag)) { flags |= flag; } }
        bool IsOwner() { return flags & MEMBER_FLAG_OWNER; }
        void SetOwner(bool state)
        {
            if (state)
            {
                flags |= MEMBER_FLAG_OWNER;
            }
            else
            {
                flags &= ~MEMBER_FLAG_OWNER;
            }
        }
        bool IsModerator() { return flags & MEMBER_FLAG_MODERATOR; }
        void SetModerator(bool state)
        {
            if (state)
            {
                flags |= MEMBER_FLAG_MODERATOR;
            }
            else
            {
                flags &= ~MEMBER_FLAG_MODERATOR;
            }
        }
        bool IsMuted() { return flags & MEMBER_FLAG_MUTED; }
        void SetMuted(bool state)
        {
            if (state)
            {
                flags |= MEMBER_FLAG_MUTED;
            }
            else
            {
                flags &= ~MEMBER_FLAG_MUTED;
            }
        }
    };

public:
    Channel(const std::string& name, uint32 channel_id);
    std::string GetName() const { return m_name; }
    uint32 GetChannelId() const { return m_channelId; }
    bool IsConstant() const { return m_channelId != 0; }
    bool IsAnnounce() const { return m_announce; }
    bool IsLFG() const { return GetFlags() & CHANNEL_FLAG_LFG; }
    std::string GetPassword() const { return m_password; }
    void SetPassword(const std::string& npassword) { m_password = npassword; }
    void SetAnnounce(bool nannounce) { m_announce = nannounce; }
    uint32 GetNumPlayers() const { return m_players.size(); }
    uint8 GetFlags() const { return m_flags; }
    bool HasFlag(uint8 flag) { return m_flags & flag; }

    void Join(Player* player, const char* password);
    void Leave(Player* player, bool send = true);
    void KickOrBan(Player* player, const char* targetName, bool ban);
    void Kick(Player* player, const char* targetName) { KickOrBan(player, targetName, false); }
    void Ban(Player* player, const char* targetName) { KickOrBan(player, targetName, true); }
    void UnBan(Player* player, const char* targetName);
    void Password(Player* player, const char* password);
    void SetMode(Player* player, const char* targetName, bool moderator, bool set);
    void SetOwner(Player* player, const char* targetName);
    void SendWhoOwner(Player* player);
    void SetModerator(Player* player, const char* targetName) { SetMode(player, targetName, true, true); }
    void UnsetModerator(Player* player, const char* targetName) { SetMode(player, targetName, true, false); }
    void SetMute(Player* player, const char* targetName) { SetMode(player, targetName, false, true); }
    void UnsetMute(Player* player, const char* targetName) { SetMode(player, targetName, false, false); }
    void List(Player* player);
    void Announce(Player* player);
    void Moderate(Player* player);
    void Say(Player* player, const char* text, uint32 lang);
    void Invite(Player* player, const char* targetName);
    void Voice(ObjectGuid guid1, ObjectGuid guid2);
    void DeVoice(ObjectGuid guid1, ObjectGuid guid2);
    void JoinNotify(ObjectGuid guid);                   // invisible notify
    void LeaveNotify(ObjectGuid guid);                  // invisible notify

    /// Sends `data` to every member, but for a member who ignores `guid` when it is not empty.
    void SendToAll(WorldPacket* data, ObjectGuid guid = ObjectGuid());
    /// Sends `data` to the member `who`.
    void SendToOne(WorldPacket* data, ObjectGuid who);

    uint8 GetPlayerFlags(ObjectGuid guid) const
    {
        PlayerList::const_iterator p_itr = m_players.find(guid);
        if (p_itr == m_players.end())
        {
            return 0;
        }

        return p_itr->second.flags;
    }

private:
    // the notice of each type, with the fields it carries
    ChannelNotice MakeNotice(ChatNotify type) const;
    ChannelNotice MakeJoined(ObjectGuid guid) const;
    ChannelNotice MakeLeft(ObjectGuid guid) const;
    ChannelNotice MakeYouJoined() const;
    ChannelNotice MakeYouLeft() const;
    ChannelNotice MakeWrongPassword() const;
    ChannelNotice MakeNotMember() const;
    ChannelNotice MakeNotModerator() const;
    ChannelNotice MakePasswordChanged(ObjectGuid guid) const;
    ChannelNotice MakeOwnerChanged(ObjectGuid guid) const;
    ChannelNotice MakePlayerNotFound(const std::string& name) const;
    ChannelNotice MakeNotOwner() const;
    ChannelNotice MakeChannelOwner() const;
    ChannelNotice MakeModeChange(ObjectGuid guid, uint8 oldflags) const;
    ChannelNotice MakeAnnouncementsOn(ObjectGuid guid) const;
    ChannelNotice MakeAnnouncementsOff(ObjectGuid guid) const;
    ChannelNotice MakeModerationOn(ObjectGuid guid) const;
    ChannelNotice MakeModerationOff(ObjectGuid guid) const;
    ChannelNotice MakeMuted() const;
    ChannelNotice MakePlayerKicked(ObjectGuid target, ObjectGuid source) const;
    ChannelNotice MakeBanned() const;
    ChannelNotice MakePlayerBanned(ObjectGuid target, ObjectGuid source) const;
    ChannelNotice MakePlayerUnbanned(ObjectGuid target, ObjectGuid source) const;
    ChannelNotice MakePlayerNotBanned(const std::string& name) const;
    ChannelNotice MakePlayerAlreadyMember(ObjectGuid guid) const;
    ChannelNotice MakeInvite(ObjectGuid guid) const;
    ChannelNotice MakeInviteWrongFaction() const;
    ChannelNotice MakeWrongFaction() const;
    ChannelNotice MakeInvalidName() const;
    ChannelNotice MakeNotModerated() const;
    ChannelNotice MakePlayerInvited(const std::string& name) const;
    ChannelNotice MakePlayerInviteBanned(const std::string& name) const;
    ChannelNotice MakeThrottled() const;
    ChannelNotice MakeNotInArea() const;
    ChannelNotice MakeNotInLfg() const;
    ChannelNotice MakeVoiceOn(ObjectGuid guid) const;
    ChannelNotice MakeVoiceOff(ObjectGuid guid) const;

    /// Tells the member `who` `notice`.
    void NotifyOne(ChannelNotice const& notice, ObjectGuid who);
    /// Tells every member `notice`, but for a member who ignores `ignoring` when it is not empty.
    void NotifyAll(ChannelNotice const& notice, ObjectGuid ignoring = ObjectGuid());
    void SetOwner(ObjectGuid guid, bool exclaim = true);

    bool IsOn(ObjectGuid who) const { return m_players.find(who) != m_players.end(); }
    bool IsBanned(ObjectGuid guid) const { return m_banned.find(guid) != m_banned.end(); }

    void SetModerator(ObjectGuid guid, bool set)
    {
        if (m_players[guid].IsModerator() != set)
        {
            uint8 oldFlag = GetPlayerFlags(guid);
            m_players[guid].SetModerator(set);

            NotifyAll(MakeModeChange(guid, oldFlag));
        }
    }

    void SetMute(ObjectGuid guid, bool set)
    {
        if (m_players[guid].IsMuted() != set)
        {
            uint8 oldFlag = GetPlayerFlags(guid);
            m_players[guid].SetMuted(set);

            NotifyAll(MakeModeChange(guid, oldFlag));
        }
    }

private:
    bool        m_announce;
    bool        m_moderate;
    std::string m_name;
    std::string m_password;
    uint8       m_flags;
    uint32      m_channelId;
    ObjectGuid  m_ownerGuid;

    typedef     std::map<ObjectGuid, PlayerInfo> PlayerList;
    PlayerList  m_players;
    GuidSet m_banned;
};
#endif
