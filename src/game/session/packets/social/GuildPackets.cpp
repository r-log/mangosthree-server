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

#include "GuildPackets.h"
#include "GuildBankPackets.h"
#include "Calendar.h"
#include "GridMap.h"
#include "Log.h"
#include "Opcodes.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "Util.h"
#include "WorldPacket.h"
#include "session/packets/chat/MessageChatPacket.h"

#include <ctime>

void BuildGuildRosterPacket(WorldPacket& data, Guild const& guild)
{
    ByteBuffer buffer;
    Guild::MemberList const& members = guild.GetMembers();

    // we can only guess size
    data.Initialize(SMSG_GUILD_ROSTER, 4 + guild.GetMOTD().length() + 1 + guild.GetGINFO().length() + 1 + 4 + members.size() * 50);
    data.WriteBits(guild.GetMOTD().length(), 11);
    data.WriteBits(members.size(), 18);

    for (Guild::MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        MemberSlot const member = itr->second;
        Player* player = sPlayerRegistry.Find(ObjectGuid(HIGHGUID_PLAYER, itr->first));

        ObjectGuid guid = member.guid;
        data.WriteGuidMask<3, 4>(guid);
        data.WriteBit(false);                           // Has Authenticator
        data.WriteBit(false);                           // Can Scroll of Ressurect
        data.WriteBits(member.Pnote.length(), 8);
        data.WriteBits(member.OFFnote.length(), 8);
        data.WriteGuidMask<0>(guid);
        data.WriteBits(member.Name.length(), 7);
        data.WriteGuidMask<1, 2, 6, 5, 7>(guid);

        uint8 flags = GUILDMEMBER_STATUS_NONE;
        if (player)
        {
            flags |= GUILDMEMBER_STATUS_ONLINE;
            if (player->isAFK())
            {
                flags |= GUILDMEMBER_STATUS_AFK;
            }
            if (player->isDND())
            {
                flags |= GUILDMEMBER_STATUS_DND;
            }
        }

        buffer << uint8(member.Class);
        buffer << int32(0);                             // unk
        buffer.WriteGuidBytes<0>(guid);
        buffer << uint64(0);                            // weekly activity
        buffer << uint32(member.RankId);
        buffer << uint32(0);                            // achievement points

        // professions: id, value, rank
        buffer << uint32(0) << uint32(0) << uint32(0);
        buffer << uint32(0) << uint32(0) << uint32(0);

        buffer.WriteGuidBytes<2>(guid);
        buffer << uint8(flags);
        buffer << uint32(player ? player->GetTerrain()->GetZoneId(player->Where().X(), player->Where().Y(), player->Where().Z()) : member.ZoneId);
        buffer << uint64(0);                            // Total activity
        buffer.WriteGuidBytes<7>(guid);
        buffer << uint32(0);                            // Remaining guild week Rep

        buffer.WriteStringData(member.Pnote);

        buffer.WriteGuidBytes<3>(guid);
        buffer << uint8(player ? player->getLevel() : member.Level);
        buffer << int32(0);                             // unk
        buffer.WriteGuidBytes<5, 4>(guid);
        buffer << uint8(0);                             // unk
        buffer.WriteGuidBytes<1>(guid);
        buffer << float(player ? 0.0f : float(time(NULL) - itr->second.LogoutTime) / DAY);

        buffer.WriteStringData(member.OFFnote);

        buffer.WriteGuidBytes<6>(guid);
        buffer.WriteStringData(member.Name);
    }

    data.WriteBits(guild.GetGINFO().length(), 12);

    data.FlushBits();
    data.append(buffer);

    data.WriteStringData(guild.GetGINFO());
    data.WriteStringData(guild.GetMOTD());

    data << uint32(guild.GetCachedAccountsNumber());
    data << uint32(0);                                      // weekly rep cap
    data << secsToTimeBitFields(guild.GetCreatedDate());
    data << uint32(0);
}

void BuildGuildQueryPacket(WorldPacket& data, Guild const& guild)
{
    Guild::RankList const& ranks = guild.GetRanks();

    data.Initialize(SMSG_GUILD_QUERY_RESPONSE, 8 * 32 + 200); // we can only guess size

    data << guild.GetObjectGuid();
    data << guild.GetName();

    for (size_t i = 0 ; i < GUILD_RANKS_MAX_COUNT; ++i)     // show always 10 ranks
    {
        if (i < ranks.size())
        {
            data << ranks[i].Name;
        }
        else
        {
            data << uint8(0);                                // null string
        }
    }

    // Rank order of creation
    for (uint8 i = 0; i < GUILD_RANKS_MAX_COUNT; ++i)
    {
        if (i < ranks.size())
        {
            data << uint32(i);
        }
        else
        {
            data << uint32(0);
        }
    }

    // Rank order of "importance" (sorting by rights)
    for (uint8 i = 0; i < GUILD_RANKS_MAX_COUNT; ++i)
    {
        if (i < ranks.size())
        {
            data << uint32(i);
        }
        else
        {
            data << uint32(0);
        }
    }

    data << uint32(guild.GetEmblemStyle());
    data << uint32(guild.GetEmblemColor());
    data << uint32(guild.GetBorderStyle());
    data << uint32(guild.GetBorderColor());
    data << uint32(guild.GetBackgroundColor());
    data << uint32(ranks.size());
}

void BuildGuildRanksPacket(WorldPacket& data, Guild const& guild)
{
    Guild::RankList const& ranks = guild.GetRanks();

    data.Initialize(SMSG_GUILD_QUERY_RANKS_RESULT, (4 * 2 * 8 + 4 * 3 + 1) * ranks.size()); // we can only guess size

    data.WriteBits(ranks.size(), 18);

    for (uint8 i = 0; i < ranks.size(); ++i)
    {
        data.WriteBits(ranks[i].Name.length(), 7);
    }

    for (uint8 i = 0; i < ranks.size(); ++i)
    {
        data << uint32(i);

        for (uint8 j = 0; j < GUILD_BANK_MAX_TABS; ++j)
        {
            data << uint32(ranks[i].TabSlotPerDay[j]);
            data << uint32(ranks[i].TabRight[j]);
        }

        data << uint32(ranks[i].BankMoneyPerDay);
        data << uint32(ranks[i].Rights);

        data.WriteStringData(ranks[i].Name);

        data << uint32(i);  // rank id
    }
}

void WriteGuildEventLogEntry(WorldPacket& data, ByteBuffer& buffer, GuildEventLogEntry const& entry)
{
    // Player 1
    ObjectGuid guid1 = ObjectGuid(HIGHGUID_PLAYER, entry.PlayerGuid1);
    // Player 2 not for left/join guild events
    ObjectGuid guid2 = ObjectGuid(HIGHGUID_PLAYER, entry.PlayerGuid2);
    data.WriteGuidMask<2, 4>(guid1);
    data.WriteGuidMask<7, 6>(guid2);
    data.WriteGuidMask<3>(guid1);
    data.WriteGuidMask<3, 5>(guid2);
    data.WriteGuidMask<7, 5, 0>(guid1);
    data.WriteGuidMask<4, 2, 0, 1>(guid2);
    data.WriteGuidMask<1, 6>(guid1);

    buffer.WriteGuidBytes<3, 2, 5>(guid2);

    // New Rank - only for promote/demote guild events
    buffer << uint8(entry.NewRank);

    buffer.WriteGuidBytes<4>(guid2);
    buffer.WriteGuidBytes<0, 4>(guid1);

    // Event timestamp
    buffer << uint32(time(NULL) - entry.TimeStamp);

    buffer.WriteGuidBytes<7, 3>(guid1);
    buffer.WriteGuidBytes<0, 6, 7>(guid2);
    buffer.WriteGuidBytes<5>(guid1);

    // Event type
    buffer << uint8(entry.EventType);

    buffer.WriteGuidBytes<1>(guid2);
    buffer.WriteGuidBytes<2, 6, 1>(guid1);
}

void BuildGuildEventLogPacket(WorldPacket& data, Guild const& guild)
{
    std::list<GuildEventLogEntry> const& log = guild.GetEventLog();

    // Sending result
    ByteBuffer buffer;
    data.Initialize(SMSG_GUILD_EVENT_LOG, 0);
    // count, max count == 100
    data.WriteBits(log.size(), 23);
    for (std::list<GuildEventLogEntry>::const_iterator itr = log.begin(); itr != log.end(); ++itr)
    {
        WriteGuildEventLogEntry(data, buffer, *itr);
    }
    if (!buffer.empty())
    {
        data.FlushBits();
        data.append(buffer);
    }
}

bool BuildGuildCalendarInvitePacket(WorldPacket& data, Guild const& guild, ObjectGuid requester, uint32 minLevel, uint32 maxLevel, uint32 minRank)
{
    uint32 count = 0;

    data.Initialize(SMSG_CALENDAR_FILTER_GUILD);
    data << uint32(count); // count placeholder

    for (Guild::MemberList::const_iterator itr = guild.GetMembers().begin(); itr != guild.GetMembers().end(); ++itr)
    {
        // not sure if needed, maybe client checks it as well
        if (count >= CALENDAR_MAX_INVITES)
        {
            return false;
        }

        MemberSlot const& member = itr->second;
        uint32 level = Player::GetLevelFromDB(member.guid);

        if (member.guid != requester && level >= minLevel && level <= maxLevel && member.RankId <= minRank)
        {
            data << member.guid.WriteAsPacked();
            data << uint8(level);
            ++count;
        }
    }

    data.put<uint32>(0, count);
    return true;
}

void BuildGuildEventPacket(WorldPacket& data, GuildEvents event, ObjectGuid guid, char const* str1, char const* str2, char const* str3)
{
    uint8 strCount = !str1 ? 0 : (!str2 ? 1 : (!str3 ? 2 : 3));

    data.Initialize(SMSG_GUILD_EVENT, 1 + 1 + 1 * strCount + (!guid ? 0 : 8));
    data << uint8(event);
    data << uint8(strCount);

    if (str3)
    {
        data << str1;
        data << str2;
        data << str3;
    }
    else if (str2)
    {
        data << str1;
        data << str2;
    }
    else if (str1)
    {
        data << str1;
    }

    if (guid)
    {
        data << ObjectGuid(guid);
    }
}

void SendGuildRoster(PlayerSession& session, Guild const& guild)
{
    WorldPacket data;
    BuildGuildRosterPacket(data, guild);
    session.SendPacket(&data);
    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_ROSTER)");
}

void BroadcastGuildRoster(Guild& guild)
{
    WorldPacket data;
    BuildGuildRosterPacket(data, guild);
    guild.BroadcastPacket(&data);
    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_ROSTER)");
}

void SendGuildQuery(PlayerSession& session, Guild const& guild)
{
    WorldPacket data;
    BuildGuildQueryPacket(data, guild);
    session.SendPacket(&data);
    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_QUERY_RESPONSE)");
}

void SendGuildRanks(PlayerSession& session, Guild const& guild)
{
    WorldPacket data;
    BuildGuildRanksPacket(data, guild);
    session.SendPacket(&data);
}

void SendGuildEventLog(PlayerSession& session, Guild const& guild)
{
    WorldPacket data;
    BuildGuildEventLogPacket(data, guild);
    session.SendPacket(&data);
    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_EVENT_LOG)");
}

void SessionGuildNotices::Event(Guild& guild, GuildEvents event, ObjectGuid guid, char const* str1, char const* str2, char const* str3)
{
    WorldPacket data;
    BuildGuildEventPacket(data, event, guid, str1, str2, str3);
    guild.BroadcastPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_GUILD_EVENT");
}

void SessionGuildNotices::Chat(Guild& guild, Player& speaker, std::string const& msg, uint32 language, bool officers)
{
    WorldPacket data;
    BuildMessageChatPacket(data, officers ? CHAT_MSG_OFFICER : CHAT_MSG_GUILD, msg.c_str(), Language(language), speaker.GetChatTag(), speaker.GetObjectGuid(), speaker.GetName());
    guild.SendToChatListeners(&data, speaker, officers);
}

void SessionGuildNotices::AddonChat(Guild& guild, Player& speaker, std::string const& msg, std::string const& prefix, bool officers)
{
    WorldPacket data;
    BuildMessageChatPacket(data, officers ? CHAT_MSG_OFFICER : CHAT_MSG_GUILD, msg.c_str(), LANG_ADDON, CHAT_TAG_NONE, ObjectGuid(), NULL, ObjectGuid(), NULL, NULL, 0, prefix.c_str());
    guild.SendToChatListeners(&data, speaker, officers);
}

void SessionGuildNotices::BankSlotsChanged(Guild const& guild, Player& player, uint8 tabId, std::vector<int32> const& slots, uint32 withdrawRemaining)
{
    WorldPacket data;
    BuildGuildBankSlotsPacket(data, guild, tabId, slots, withdrawRemaining);
    player.GetSession()->SendPacket(&data);
}

void SessionGuildNotices::BankTabTextChanged(Guild& guild, uint8 tabId)
{
    WorldPacket data;
    BuildGuildBankTabTextPacket(data, guild, tabId);
    guild.BroadcastPacket(&data);
}
