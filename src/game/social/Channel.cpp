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

/**
 * @file Channel.cpp
 * @brief Chat channel implementation
 *
 * This file implements the Channel class which manages chat channels
 * including built-in channels (General, Trade, etc.) and custom player-created
 * channels. It handles:
 *
 * - Channel creation and initialization
 * - Player join/leave operations
 * - Channel moderation (kick, ban, mute)
 * - Message broadcasting
 * - Channel announcements
 * - Built-in vs custom channel behavior
 *
 * @see Channel for the channel class
 * @see ChannelMgr for channel management
 */

#include <string>
#include "Channel.h"
#include "ObjectMgr.h"
#include "DBCStores.h"
#include "data/GameConfig.h"
#include "SocialList.h"
#include "social/ChannelNotices.h"
#include "PlayerRegistry.h"

Channel::Channel(const std::string& name, uint32 channel_id)
    : m_announce(true), m_moderate(false), m_name(name), m_flags(0), m_channelId(channel_id)
{
    // set special flags if built-in channel
    ChatChannelsEntry const* ch = GetChannelEntryFor(channel_id);
    if (ch)                                                 // it's built-in channel
    {
        channel_id = ch->ID;                         // built-in channel
        m_announce = false;                                 // no join/leave announces

        m_flags |= CHANNEL_FLAG_GENERAL;                    // for all built-in channels

        if (ch->Flags & CHANNEL_DBC_FLAG_TRADE)             // for trade channel
        {
            m_flags |= CHANNEL_FLAG_TRADE;
        }

        if (ch->Flags & CHANNEL_DBC_FLAG_CITY_ONLY2)        // for city only channels
        {
            m_flags |= CHANNEL_FLAG_CITY;
        }

        if (ch->Flags & CHANNEL_DBC_FLAG_LFG)               // for LFG channel
        {
            m_flags |= CHANNEL_FLAG_LFG;
        }
        else                                                // for all other channels
        {
            m_flags |= CHANNEL_FLAG_NOT_LFG;
        }
    }
    else                                                    // it's custom channel
    {
        m_flags |= CHANNEL_FLAG_CUSTOM;
    }
}

/**
 * @brief Adds a player to the channel after validating membership, bans, and password.
 *
 * @param player The player attempting to join.
 * @param password The supplied channel password.
 */
void Channel::Join(Player* player, const char* password)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (IsOn(guid))
    {
        if (!IsConstant())                                  // non send error message for built-in channels
        {
            NotifyOne(MakePlayerAlreadyMember(guid), guid);
        }
        return;
    }

    if (IsBanned(guid))
    {
        NotifyOne(MakeBanned(), guid);
        return;
    }

    if (m_password.length() > 0 && strcmp(password, m_password.c_str()))
    {
        NotifyOne(MakeWrongPassword(), guid);
        return;
    }

    if (HasFlag(CHANNEL_FLAG_LFG) && sGameConfig.getConfig(CONFIG_BOOL_RESTRICTED_LFG_CHANNEL) && player->GetSession()->GetSecurity() == SEC_PLAYER)
    {
        NotifyOne(MakeNotInLfg(), guid);
        return;
    }

    if (player->GetGuildId() && (GetFlags() == 0x38))
    {
        return;
    }

    // join channel
    player->JoinedChannel(this);

    if (m_announce && (player->GetSession()->GetSecurity() < SEC_GAMEMASTER || !sGameConfig.getConfig(CONFIG_BOOL_SILENTLY_GM_JOIN_TO_CHANNEL)))
    {
        NotifyAll(MakeJoined(guid));
    }

    PlayerInfo& pinfo = m_players[guid];
    pinfo.player = guid;
    pinfo.flags = MEMBER_FLAG_NONE;

    NotifyOne(MakeYouJoined(), guid);

    JoinNotify(guid);

    // if no owner first logged will become
    if (!IsConstant() && !m_ownerGuid)
    {
        SetOwner(guid, (m_players.size() > 1 ? true : false));
        m_players[guid].SetModerator(true);
    }
}

/**
 * @brief Removes a player from the channel and updates ownership if needed.
 *
 * @param player The player leaving the channel.
 * @param send True to send leave notifications to the player.
 */
void Channel::Leave(Player* player, bool send)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        if (send)
        {
            NotifyOne(MakeNotMember(), guid);
        }
        return;
    }

    // leave channel
    if (send)
    {
        NotifyOne(MakeYouLeft(), guid);
        player->LeftChannel(this);
    }

    bool changeowner = m_players[guid].IsOwner();

    m_players.erase(guid);
    if (m_announce && (player->GetSession()->GetSecurity() < SEC_GAMEMASTER || !sGameConfig.getConfig(CONFIG_BOOL_SILENTLY_GM_JOIN_TO_CHANNEL)))
    {
        NotifyAll(MakeLeft(guid));
    }

    LeaveNotify(guid);

    if (changeowner)
    {
        ObjectGuid newowner = !m_players.empty() ? m_players.begin()->second.player : ObjectGuid();
        SetOwner(newowner);
    }
}

/**
 * @brief Kicks or bans a target player from the channel.
 *
 * @param player The moderator issuing the command.
 * @param targetName The target player name.
 * @param ban True to ban the player in addition to removing them.
 */
void Channel::KickOrBan(Player* player, const char* targetName, bool ban)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        NotifyOne(MakeNotMember(), guid);
        return;
    }

    if (!m_players[guid].IsModerator() && player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
    {
        NotifyOne(MakeNotModerator(), guid);
        return;
    }

    Player* target = sPlayerRegistry.FindByName(targetName);
    if (!target)
    {
        NotifyOne(MakePlayerNotFound(targetName), guid);
        return;
    }

    ObjectGuid targetGuid = target->GetObjectGuid();
    if (!IsOn(targetGuid))
    {
        NotifyOne(MakePlayerNotFound(targetName), guid);
        return;
    }

    bool changeowner = m_ownerGuid == targetGuid;

    if (player->GetSession()->GetSecurity() < SEC_GAMEMASTER && changeowner && guid != m_ownerGuid)
    {
        NotifyOne(MakeNotOwner(), guid);
        return;
    }

    // kick or ban player
    ChannelNotice notice;

    if (ban && !IsBanned(targetGuid))
    {
        m_banned.insert(targetGuid);
        notice = MakePlayerBanned(targetGuid, guid);
    }
    else
    {
        notice = MakePlayerKicked(targetGuid, guid);
    }

    NotifyAll(notice);
    m_players.erase(targetGuid);
    target->LeftChannel(this);

    if (changeowner)
    {
        ObjectGuid newowner = !m_players.empty() ? guid : ObjectGuid();
        SetOwner(newowner);
    }
}

/**
 * @brief Removes a ban from the specified player.
 *
 * @param player The moderator issuing the unban.
 * @param targetName The target player name.
 */
void Channel::UnBan(Player* player, const char* targetName)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        NotifyOne(MakeNotMember(), guid);
        return;
    }

    if (!m_players[guid].IsModerator() && player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
    {
        NotifyOne(MakeNotModerator(), guid);
        return;
    }

    Player* target = sPlayerRegistry.FindByName(targetName);
    if (!target)
    {
        NotifyOne(MakePlayerNotFound(targetName), guid);
        return;
    }

    ObjectGuid targetGuid = target->GetObjectGuid();
    if (!IsBanned(targetGuid))
    {
        NotifyOne(MakePlayerNotBanned(targetName), guid);
        return;
    }

    // unban player
    m_banned.erase(targetGuid);

    NotifyAll(MakePlayerUnbanned(targetGuid, guid));
}

/**
 * @brief Changes the channel password.
 *
 * @param player The moderator changing the password.
 * @param password The new password string.
 */
void Channel::Password(Player* player, const char* password)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        NotifyOne(MakeNotMember(), guid);
        return;
    }

    if (!m_players[guid].IsModerator() && player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
    {
        NotifyOne(MakeNotModerator(), guid);
        return;
    }

    // set channel password
    m_password = password;

    NotifyAll(MakePasswordChanged(guid));
}

/**
 * @brief Sets or clears moderator or mute mode for a channel member.
 *
 * @param player The moderator issuing the change.
 * @param targetName The target player name.
 * @param moderator True to change moderator state, false to change mute state.
 * @param set True to enable the mode, false to clear it.
 */
void Channel::SetMode(Player* player, const char* targetName, bool moderator, bool set)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        NotifyOne(MakeNotMember(), guid);
        return;
    }

    if (!m_players[guid].IsModerator() && player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
    {
        NotifyOne(MakeNotModerator(), guid);
        return;
    }

    Player* target = sPlayerRegistry.FindByName(targetName);
    if (!target)
    {
        NotifyOne(MakePlayerNotFound(targetName), guid);
        return;
    }

    ObjectGuid targetGuid = target->GetObjectGuid();
    if (moderator && guid == m_ownerGuid && targetGuid == m_ownerGuid)
    {
        return;
    }

    if (!IsOn(targetGuid))
    {
        NotifyOne(MakePlayerNotFound(targetName), guid);
        return;
    }

    // allow make moderator from another team only if both is GMs
    // at this moment this only way to show channel post for GM from another team
    if ((player->GetSession()->GetSecurity() < SEC_GAMEMASTER || target->GetSession()->GetSecurity() < SEC_GAMEMASTER) &&
            player->GetTeam() != target->GetTeam() && !sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL))
    {
        NotifyOne(MakePlayerNotFound(targetName), guid);
        return;
    }

    if (m_ownerGuid == targetGuid && m_ownerGuid != guid)
    {
        NotifyOne(MakeNotOwner(), guid);
        return;
    }

    // set channel moderator
    if (moderator)
    {
        SetModerator(targetGuid, set);
    }
    else
    {
        SetMute(targetGuid, set);
    }
}

void Channel::SetOwner(ObjectGuid guid, bool exclaim)
{
    if (m_ownerGuid)
    {
        // [] will re-add player after it possible removed
        PlayerList::iterator p_itr = m_players.find(m_ownerGuid);
        if (p_itr != m_players.end())
        {
            p_itr->second.SetOwner(false);
        }
    }

    m_ownerGuid = guid;

    if (m_ownerGuid)
    {
        uint8 oldFlag = GetPlayerFlags(m_ownerGuid);
        m_players[m_ownerGuid].SetOwner(true);

        NotifyAll(MakeModeChange(m_ownerGuid, oldFlag));

        if (exclaim)
        {
            NotifyAll(MakeOwnerChanged(m_ownerGuid));
        }
    }
}

/**
 * @brief Transfers channel ownership to another member.
 *
 * @param player The current owner or GM issuing the command.
 * @param targetName The target player name.
 */
void Channel::SetOwner(Player* player, const char* targetName)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        NotifyOne(MakeNotMember(), guid);
        return;
    }

    if (player->GetSession()->GetSecurity() < SEC_GAMEMASTER && guid != m_ownerGuid)
    {
        NotifyOne(MakeNotOwner(), guid);
        return;
    }

    Player* target = sPlayerRegistry.FindByName(targetName);
    if (!target)
    {
        NotifyOne(MakePlayerNotFound(targetName), guid);
        return;
    }

    ObjectGuid targetGuid = target->GetObjectGuid();
    if (!IsOn(targetGuid))
    {
        NotifyOne(MakePlayerNotFound(targetName), guid);
        return;
    }

    if (target->GetTeam() != player->GetTeam() && !sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL))
    {
        NotifyOne(MakePlayerNotFound(targetName), guid);
        return;
    }

    // set channel owner
    m_players[targetGuid].SetModerator(true);
    SetOwner(targetGuid);
}

/**
 * @brief Sends the current channel owner information to a member.
 *
 * @param player The player requesting the owner information.
 */
void Channel::SendWhoOwner(Player* player)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        NotifyOne(MakeNotMember(), guid);
        return;
    }

    // send channel owner
    NotifyOne(MakeChannelOwner(), guid);
}

/**
 * @brief Sends the visible channel member list to a player.
 *
 * @param player The player requesting the member list.
 */
void Channel::List(Player* player)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        NotifyOne(MakeNotMember(), guid);
        return;
    }

    // list players in channel
    AccountTypes gmLevelInWhoList = (AccountTypes)sGameConfig.getConfig(CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST);

    std::vector<std::pair<ObjectGuid, uint8> > visible;
    for (PlayerList::const_iterator i = m_players.begin(); i != m_players.end(); ++i)
    {
        Player* plr = sPlayerRegistry.Find(i->first);

        // PLAYER can't see MODERATOR, GAME MASTER, ADMINISTRATOR characters
        // MODERATOR, GAME MASTER, ADMINISTRATOR can see all
        if (plr && (player->GetSession()->GetSecurity() > SEC_PLAYER || plr->GetSession()->GetSecurity() <= gmLevelInWhoList) &&
                plr->IsVisibleGloballyFor(player))
        {
            visible.push_back(std::make_pair(ObjectGuid(i->first), i->second.flags));
        }
    }

    sChannelNotices.MemberList(*this, guid, visible);
}

/**
 * @brief Toggles join and leave announcements for the channel.
 *
 * @param player The moderator issuing the toggle.
 */
void Channel::Announce(Player* player)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        NotifyOne(MakeNotMember(), guid);
        return;
    }

    if (!m_players[guid].IsModerator() && player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
    {
        NotifyOne(MakeNotModerator(), guid);
        return;
    }

    // toggle channel announcement
    m_announce = !m_announce;

    ChannelNotice notice;
    if (m_announce)
    {
        notice = MakeAnnouncementsOn(guid);
    }
    else
    {
        notice = MakeAnnouncementsOff(guid);
    }

    NotifyAll(notice);
}

/**
 * @brief Toggles moderated mode for the channel.
 *
 * @param player The moderator issuing the toggle.
 */
void Channel::Moderate(Player* player)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        NotifyOne(MakeNotMember(), guid);
        return;
    }

    if (!m_players[guid].IsModerator() && player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
    {
        NotifyOne(MakeNotModerator(), guid);
        return;
    }

    // toggle channel moderation
    m_moderate = !m_moderate;

    ChannelNotice notice;
    if (m_moderate)
    {
        notice = MakeModerationOn(guid);
    }
    else
    {
        notice = MakeModerationOff(guid);
    }

    NotifyAll(notice);
}

/**
 * @brief Sends a chat message to all eligible channel members.
 *
 * @param player The player speaking in the channel.
 * @param text The message text.
 * @param lang The chat language identifier.
 */
void Channel::Say(Player* player, const char* text, uint32 lang)
{
    if (!text)
    {
        return;
    }

    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        NotifyOne(MakeNotMember(), guid);
        return;
    }

    if (m_players[guid].IsMuted())
    {
        NotifyOne(MakeMuted(), guid);
        return;
    }

    if (m_moderate && !m_players[guid].IsModerator() && player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
    {
        NotifyOne(MakeNotModerator(), guid);
        return;
    }

    // send channel message
    if (sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL))
    {
        lang = LANG_UNIVERSAL;
    }
    sChannelNotices.Said(*this, *player, text, lang, !m_players[guid].IsModerator() ? guid : ObjectGuid());
}

/**
 * @brief Invites another player to the channel.
 *
 * @param player The player sending the invitation.
 * @param targetName The target player name.
 */
void Channel::Invite(Player* player, const char* targetName)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        NotifyOne(MakeNotMember(), guid);
        return;
    }

    Player* target = sPlayerRegistry.FindByName(targetName);
    if (!target)
    {
        NotifyOne(MakePlayerNotFound(targetName), guid);
        return;
    }

    ObjectGuid targetGuid = target->GetObjectGuid();
    if (IsOn(targetGuid))
    {
        NotifyOne(MakePlayerAlreadyMember(targetGuid), guid);
        return;
    }

    if (IsBanned(targetGuid))
    {
        NotifyOne(MakePlayerInviteBanned(targetName), guid);
        return;
    }

    if (target->GetTeam() != player->GetTeam() && !sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL))
    {
        NotifyOne(MakeInviteWrongFaction(), guid);
        return;
    }

    // invite player
    if (!target->GetSocial()->HasIgnore(guid))
    {
        NotifyOne(MakeInvite(guid), targetGuid);
    }

    NotifyOne(MakePlayerInvited(targetName), guid);
}

/**
 * @brief Sends a packet to all channel members, respecting ignores for an optional sender.
 *
 * @param data The packet to send.
 * @param guid The optional sender GUID used for ignore filtering.
 */
void Channel::SendToAll(WorldPacket* data, ObjectGuid guid)
{
    for (PlayerList::const_iterator i = m_players.begin(); i != m_players.end(); ++i)
    {
        if (Player* plr = sPlayerRegistry.Find(i->first))
        {
            if (!guid || !plr->GetSocial()->HasIgnore(guid))
            {
                plr->GetSession()->SendPacket(data);
            }
        }
    }
}

/**
 * @brief Sends a packet to a specific player.
 *
 * @param data The packet to send.
 * @param who The recipient player GUID.
 */
void Channel::SendToOne(WorldPacket* data, ObjectGuid who)
{
    if (Player* plr = sPlayerRegistry.Find(who))
    {
        plr->GetSession()->SendPacket(data);
    }
}

void Channel::NotifyOne(ChannelNotice const& notice, ObjectGuid who)
{
    sChannelNotices.ToOne(*this, who, notice);
}

void Channel::NotifyAll(ChannelNotice const& notice, ObjectGuid ignoring)
{
    sChannelNotices.ToAll(*this, notice, ignoring);
}

/**
 * @brief Placeholder handler for granting channel voice.
 *
 * @param guid1 Unused source GUID.
 * @param guid2 Unused target GUID.
 */
void Channel::Voice(ObjectGuid /*guid1*/, ObjectGuid /*guid2*/)
{
}

/**
 * @brief Placeholder handler for removing channel voice.
 *
 * @param guid1 Unused source GUID.
 * @param guid2 Unused target GUID.
 */
void Channel::DeVoice(ObjectGuid /*guid1*/, ObjectGuid /*guid2*/)
{
}

ChannelNotice Channel::MakeNotice(ChatNotify type) const
{
    ChannelNotice notice;
    notice.type = type;
    return notice;
}

ChannelNotice Channel::MakeJoined(ObjectGuid guid) const
{
    ChannelNotice notice = MakeNotice(CHAT_JOINED_NOTICE);
    notice.guid = guid;
    return notice;
}

ChannelNotice Channel::MakeLeft(ObjectGuid guid) const
{
    ChannelNotice notice = MakeNotice(CHAT_LEFT_NOTICE);
    notice.guid = guid;
    return notice;
}

ChannelNotice Channel::MakeYouJoined() const
{
    ChannelNotice notice = MakeNotice(CHAT_YOU_JOINED_NOTICE);
    notice.flags = GetFlags();
    notice.channelId = GetChannelId();
    return notice;
}

ChannelNotice Channel::MakeYouLeft() const
{
    ChannelNotice notice = MakeNotice(CHAT_YOU_LEFT_NOTICE);
    notice.channelId = GetChannelId();
    return notice;
}

ChannelNotice Channel::MakeWrongPassword() const
{
    ChannelNotice notice = MakeNotice(CHAT_WRONG_PASSWORD_NOTICE);
    return notice;
}

ChannelNotice Channel::MakeNotMember() const
{
    ChannelNotice notice = MakeNotice(CHAT_NOT_MEMBER_NOTICE);
    return notice;
}

ChannelNotice Channel::MakeNotModerator() const
{
    ChannelNotice notice = MakeNotice(CHAT_NOT_MODERATOR_NOTICE);
    return notice;
}

ChannelNotice Channel::MakePasswordChanged(ObjectGuid guid) const
{
    ChannelNotice notice = MakeNotice(CHAT_PASSWORD_CHANGED_NOTICE);
    notice.guid = guid;
    return notice;
}

ChannelNotice Channel::MakeOwnerChanged(ObjectGuid guid) const
{
    ChannelNotice notice = MakeNotice(CHAT_OWNER_CHANGED_NOTICE);
    notice.guid = guid;
    return notice;
}

ChannelNotice Channel::MakePlayerNotFound(const std::string& name) const
{
    ChannelNotice notice = MakeNotice(CHAT_PLAYER_NOT_FOUND_NOTICE);
    notice.name = name;
    return notice;
}

ChannelNotice Channel::MakeNotOwner() const
{
    ChannelNotice notice = MakeNotice(CHAT_NOT_OWNER_NOTICE);
    return notice;
}

ChannelNotice Channel::MakeChannelOwner() const
{
    std::string name = "";
    if (!sObjectMgr.GetPlayerNameByGUID(m_ownerGuid, name) || name.empty())
    {
        name = "PLAYER_NOT_FOUND";
    }

    ChannelNotice notice = MakeNotice(CHAT_CHANNEL_OWNER_NOTICE);
    notice.name = (IsConstant() || !m_ownerGuid) ? "Nobody" : name;
    return notice;
}

ChannelNotice Channel::MakeModeChange(ObjectGuid guid, uint8 oldflags) const
{
    ChannelNotice notice = MakeNotice(CHAT_MODE_CHANGE_NOTICE);
    notice.guid = guid;
    notice.oldFlags = oldflags;
    notice.flags = GetPlayerFlags(guid);
    return notice;
}

ChannelNotice Channel::MakeAnnouncementsOn(ObjectGuid guid) const
{
    ChannelNotice notice = MakeNotice(CHAT_ANNOUNCEMENTS_ON_NOTICE);
    notice.guid = guid;
    return notice;
}

ChannelNotice Channel::MakeAnnouncementsOff(ObjectGuid guid) const
{
    ChannelNotice notice = MakeNotice(CHAT_ANNOUNCEMENTS_OFF_NOTICE);
    notice.guid = guid;
    return notice;
}

ChannelNotice Channel::MakeModerationOn(ObjectGuid guid) const
{
    ChannelNotice notice = MakeNotice(CHAT_MODERATION_ON_NOTICE);
    notice.guid = guid;
    return notice;
}

ChannelNotice Channel::MakeModerationOff(ObjectGuid guid) const
{
    ChannelNotice notice = MakeNotice(CHAT_MODERATION_OFF_NOTICE);
    notice.guid = guid;
    return notice;
}

ChannelNotice Channel::MakeMuted() const
{
    ChannelNotice notice = MakeNotice(CHAT_MUTED_NOTICE);
    return notice;
}

ChannelNotice Channel::MakePlayerKicked(ObjectGuid target, ObjectGuid source) const
{
    ChannelNotice notice = MakeNotice(CHAT_PLAYER_KICKED_NOTICE);
    notice.guid = target;
    notice.source = source;
    return notice;
}

ChannelNotice Channel::MakeBanned() const
{
    ChannelNotice notice = MakeNotice(CHAT_BANNED_NOTICE);
    return notice;
}

ChannelNotice Channel::MakePlayerBanned(ObjectGuid target, ObjectGuid source) const
{
    ChannelNotice notice = MakeNotice(CHAT_PLAYER_BANNED_NOTICE);
    notice.guid = target;
    notice.source = source;
    return notice;
}

ChannelNotice Channel::MakePlayerUnbanned(ObjectGuid target, ObjectGuid source) const
{
    ChannelNotice notice = MakeNotice(CHAT_PLAYER_UNBANNED_NOTICE);
    notice.guid = target;
    notice.source = source;
    return notice;
}

ChannelNotice Channel::MakePlayerNotBanned(const std::string& name) const
{
    ChannelNotice notice = MakeNotice(CHAT_PLAYER_NOT_BANNED_NOTICE);
    notice.name = name;
    return notice;
}

ChannelNotice Channel::MakePlayerAlreadyMember(ObjectGuid guid) const
{
    ChannelNotice notice = MakeNotice(CHAT_PLAYER_ALREADY_MEMBER_NOTICE);
    notice.guid = guid;
    return notice;
}

ChannelNotice Channel::MakeInvite(ObjectGuid guid) const
{
    ChannelNotice notice = MakeNotice(CHAT_INVITE_NOTICE);
    notice.guid = guid;
    return notice;
}

ChannelNotice Channel::MakeInviteWrongFaction() const
{
    ChannelNotice notice = MakeNotice(CHAT_INVITE_WRONG_FACTION_NOTICE);
    return notice;
}

ChannelNotice Channel::MakeWrongFaction() const
{
    ChannelNotice notice = MakeNotice(CHAT_WRONG_FACTION_NOTICE);
    return notice;
}

ChannelNotice Channel::MakeInvalidName() const
{
    ChannelNotice notice = MakeNotice(CHAT_INVALID_NAME_NOTICE);
    return notice;
}

ChannelNotice Channel::MakeNotModerated() const
{
    ChannelNotice notice = MakeNotice(CHAT_NOT_MODERATED_NOTICE);
    return notice;
}

ChannelNotice Channel::MakePlayerInvited(const std::string& name) const
{
    ChannelNotice notice = MakeNotice(CHAT_PLAYER_INVITED_NOTICE);
    notice.name = name;
    return notice;
}

ChannelNotice Channel::MakePlayerInviteBanned(const std::string& name) const
{
    ChannelNotice notice = MakeNotice(CHAT_PLAYER_INVITE_BANNED_NOTICE);
    notice.name = name;
    return notice;
}

ChannelNotice Channel::MakeThrottled() const
{
    ChannelNotice notice = MakeNotice(CHAT_THROTTLED_NOTICE);
    return notice;
}

ChannelNotice Channel::MakeNotInArea() const
{
    ChannelNotice notice = MakeNotice(CHAT_NOT_IN_AREA_NOTICE);
    return notice;
}

ChannelNotice Channel::MakeNotInLfg() const
{
    ChannelNotice notice = MakeNotice(CHAT_NOT_IN_LFG_NOTICE);
    return notice;
}

ChannelNotice Channel::MakeVoiceOn(ObjectGuid guid) const
{
    ChannelNotice notice = MakeNotice(CHAT_VOICE_ON_NOTICE);
    notice.guid = guid;
    return notice;
}

ChannelNotice Channel::MakeVoiceOff(ObjectGuid guid) const
{
    ChannelNotice notice = MakeNotice(CHAT_VOICE_OFF_NOTICE);
    notice.guid = guid;
    return notice;
}

/**
 * @brief Placeholder join notification hook for client versions that support it.
 *
 * @param guid Unused joining player GUID.
 */
void Channel::JoinNotify(ObjectGuid guid)
{
    sChannelNotices.UserListChanged(*this, guid);
}

/**
 * @brief Placeholder leave notification hook for client versions that support it.
 *
 * @param guid Unused leaving player GUID.
 */
void Channel::LeaveNotify(ObjectGuid guid)
{
    sChannelNotices.UserListRemoved(*this, guid);
}
