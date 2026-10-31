/*
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

#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include "Utilities/PackedValues.h"
#include <ctime>
#include <string>
#include <set>
#include "Log.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "MapPersistentStateMgr.h"
#include "Calendar.h"
#include "CharacterCache.h"
#include "ObjectMgr.h"
#include "SocialMgr.h"
#include "data/GameConfig.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "ArenaTeam.h"
#include "PlayerRegistry.h"
#include "persistence/characters/social/CalendarRepository.h"
#include "persistence/characters/social/SocialRepository.h"

//////////////////////////////////////////////////////////////////////////
// Send function
//////////////////////////////////////////////////////////////////////////

void CalendarMgr::SendCalendarEventInviteAlert(CalendarInvite const* invite)
{
    DEBUG_LOG("WORLD: SMSG_CALENDAR_EVENT_INVITE_ALERT");

    CalendarEvent const* event = invite->GetCalendarEvent();
    if (!event)
    {
        return;
    }

    WorldPacket data(SMSG_CALENDAR_EVENT_INVITE_ALERT);
    data << uint64(event->EventId);
    data << event->Title;
    data << secsToTimeBitFields(event->EventTime);
    data << uint32(event->Flags);
    data << uint32(event->Type);
    data << int32(event->DungeonId);
    data << uint64(invite->InviteId);

    // Client reads a fixed uint64 here before Status (event->GuildId is only the guild's
    // uint32 id, not the GUID the client wants). This field is read unconditionally on every
    // invite alert, guild event or not; omitting it desyncs every field that follows and hangs
    // the client the same way the missing guild GUID did in HandleCalendarGetCalendar.
    Guild* guild = sGuildMgr.GetGuildById(event->GuildId);
    data << uint64(guild ? guild->GetObjectGuid().GetRawValue() : 0);

    data << uint8(invite->Status);
    data << uint8(invite->Rank);
    data << event->CreatorGuid.WriteAsPacked();
    data << invite->SenderGuid.WriteAsPacked();
    //data.hexlike();

    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SendCalendarInviteAlert> senderGuid[%s], inviteeGuid[%s], EventId[" UI64FMTD "], Status[%u], InviteId[" UI64FMTD "]",
                     invite->SenderGuid.GetString().c_str(), invite->InviteeGuid.GetString().c_str(), event->EventId, uint32(invite->Status), invite->InviteId);

    if (event->IsGuildEvent() || event->IsGuildAnnouncement())
    {
        if (Guild* guild = sGuildMgr.GetGuildById(event->GuildId))
        {
            guild->BroadcastPacket(&data);
        }
    }
    else if (Player* player = sPlayerRegistry.Find(invite->InviteeGuid))
    {
        player->SendDirectMessage(&data);
    }
}

void CalendarMgr::SendCalendarEventInvite(CalendarInvite const* invite)
{
    CalendarEvent const* event = invite->GetCalendarEvent();

    time_t statusTime = invite->LastUpdateTime;
    bool preInvite = true;
    uint64 eventId = 0;
    if (event != NULL)
    {
        preInvite = false;
        eventId = event->EventId;
    }

    Player* player = sPlayerRegistry.Find(invite->InviteeGuid);

    uint8 level = player ? player->getLevel() : Player::GetLevelFromDB(invite->InviteeGuid);
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_EVENT_INVITE");
    WorldPacket data(SMSG_CALENDAR_EVENT_INVITE, 8 + 8 + 8 + 1 + 1 + 1 + (preInvite ? 0 : 4) + 1);
    data << invite->InviteeGuid.WriteAsPacked();
    data << uint64(eventId);
    data << uint64(invite->InviteId);
    data << uint8(level);
    data << uint8(invite->Status);
    data << uint8(!preInvite);
    if (!preInvite)
    {
        data << secsToTimeBitFields(statusTime);
    }
    data << uint8(invite->SenderGuid != invite->InviteeGuid); // false only if the invite is sign-up (invitee create himself his invite)

    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SendCalendarInvit> %s senderGuid[%s], inviteeGuid[%s], EventId[" UI64FMTD "], Status[%u], InviteId[" UI64FMTD "]",
                     preInvite ? "is PreInvite," : "", invite->SenderGuid.GetString().c_str(), invite->InviteeGuid.GetString().c_str(), eventId, uint32(invite->Status), invite->InviteId);

    //data.hexlike();
    if (preInvite)
    {
        if (Player* sender = sPlayerRegistry.Find(invite->SenderGuid))
        {
            sender->SendDirectMessage(&data);
        }
    }
    else
    {
        SendPacketToAllEventRelatives(data, event);
    }
}

void CalendarMgr::SendCalendarCommandResult(Player* player, CalendarError err, char const* param /*= NULL*/)
{
    if (!player)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_COMMAND_RESULT (%u)", err);
    WorldPacket data(SMSG_CALENDAR_COMMAND_RESULT, 0);
    data << uint32(0);
    data << uint8(0);
    switch (err)
    {
        case CALENDAR_ERROR_OTHER_INVITES_EXCEEDED_S:
        case CALENDAR_ERROR_ALREADY_INVITED_TO_EVENT_S:
        case CALENDAR_ERROR_IGNORING_YOU_S:
            data << param;
            break;
        default:
            data << uint8(0);
            break;
    }

    data << uint32(err);
    //data.hexlike();
    player->SendDirectMessage(&data);
}

void CalendarMgr::SendCalendarEventRemovedAlert(CalendarEvent const* event)
{
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_EVENT_REMOVED_ALERT");
    WorldPacket data(SMSG_CALENDAR_EVENT_REMOVED_ALERT, 1 + 8 + 1);
    data << uint8(1);       // show pending alert?
    data << uint64(event->EventId);
    data << secsToTimeBitFields(event->EventTime);
    //data.hexlike();
    SendPacketToAllEventRelatives(data, event);
}

void CalendarMgr::SendCalendarEvent(Player* player, CalendarEvent const* event, uint32 sendType)
{
    if (!player || !event)
    {
        return;
    }

    std::string timeStr = TimeToTimestampStr(event->EventTime);
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SendCalendarEvent> sendType[%u], CreatorGuid[%s], EventId[" UI64FMTD "], Type[%u], Flags[%u], Title[%s]",
                     sendType, event->CreatorGuid.GetString().c_str(), event->EventId, uint32(event->Type), event->Flags, event->Title.c_str());

    WorldPacket data(SMSG_CALENDAR_SEND_EVENT);
    data << uint8(sendType);
    data << event->CreatorGuid.WriteAsPacked();
    data << uint64(event->EventId);
    data << event->Title;
    data << event->Description;
    data << uint8(event->Type);
    data << uint8(event->Repeatable);
    data << uint32(CALENDAR_MAX_INVITES);
    data << event->DungeonId;
    data << event->Flags;
    data << secsToTimeBitFields(event->EventTime);
    data << secsToTimeBitFields(event->UnknownTime);

    // Client reads a fixed uint64 here before the per-invite list (event->GuildId is only
    // the guild's uint32 id, not the GUID the client wants). Same field, same fix as the
    // already-applied HandleCalendarGetCalendar and SendCalendarEventInviteAlert corrections.
    Guild* guild = sGuildMgr.GetGuildById(event->GuildId);
    data << uint64(guild ? guild->GetObjectGuid().GetRawValue() : 0);

    CalendarInviteMap const* cInvMap = event->GetInviteMap();
    data << (uint32)cInvMap->size();
    for (CalendarInviteMap::const_iterator itr = cInvMap->begin(); itr != cInvMap->end(); ++itr)
    {
        CalendarInvite const* invite = itr->second;
        ObjectGuid inviteeGuid = invite->InviteeGuid;
        Player* invitee = sPlayerRegistry.Find(inviteeGuid);

        uint8 inviteeLevel = invitee ? invitee->getLevel() : Player::GetLevelFromDB(inviteeGuid);
        uint32 inviteeGuildId = invitee ? invitee->GetGuildId() : Player::GetGuildIdFromDB(inviteeGuid);

        data << inviteeGuid.WriteAsPacked();
        data << uint8(inviteeLevel);
        data << uint8(invite->Status);
        data << uint8(invite->Rank);
        data << uint8(event->IsGuildEvent() && event->GuildId == inviteeGuildId);
        data << uint64(itr->first);
        data << secsToTimeBitFields(invite->LastUpdateTime);
        data << invite->Text;

        DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Invite> InviteId[" UI64FMTD "], InviteLvl[%u], Status[%u], Rank[%u],  GuildEvent[%s], Text[%s]",
                         invite->InviteId, uint32(inviteeLevel), uint32(invite->Status), uint32(invite->Rank),
                         (event->IsGuildEvent() && event->GuildId == inviteeGuildId) ? "true" : "false", invite->Text.c_str());
    }
    //data.hexlike();
    player->SendDirectMessage(&data);
}

void CalendarMgr::SendCalendarEventInviteRemove(CalendarInvite const* invite, uint32 flags)
{
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_EVENT_INVITE_REMOVED");

    CalendarEvent const* event = invite->GetCalendarEvent();

    WorldPacket data(SMSG_CALENDAR_EVENT_INVITE_REMOVED, 8 + 4 + 4 + 1);
    data.appendPackGUID(invite->InviteeGuid);
    data << uint64(event->EventId);
    data << uint32(flags);
    data << uint8(1);       // show pending alert?
    //data.hexlike();
    SendPacketToAllEventRelatives(data, event);
}

void CalendarMgr::SendCalendarEventInviteRemoveAlert(Player* player, CalendarEvent const* event, CalendarInviteStatus status)
{
    if (player)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_EVENT_INVITE_REMOVED_ALERT");
        WorldPacket data(SMSG_CALENDAR_EVENT_INVITE_REMOVED_ALERT, 8 + 4 + 4 + 1);
        data << uint64(event->EventId);
        data << secsToTimeBitFields(event->EventTime);
        data << uint32(event->Flags);
        data << uint8(status);
        //data.hexlike();
        player->SendDirectMessage(&data);
    }
}

void CalendarMgr::SendCalendarEventStatus(CalendarInvite const* invite)
{
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_EVENT_STATUS");
    WorldPacket data(SMSG_CALENDAR_EVENT_STATUS, 8 + 8 + 4 + 4 + 1 + 1 + 4);
    CalendarEvent const* event = invite->GetCalendarEvent();

    data << invite->InviteeGuid.WriteAsPacked();
    data << uint64(event->EventId);
    data << secsToTimeBitFields(event->EventTime);
    data << uint32(event->Flags);
    data << uint8(invite->Status);
    data << uint8(invite->Rank);
    data << secsToTimeBitFields(invite->LastUpdateTime);
    //data.hexlike();
    SendPacketToAllEventRelatives(data, event);
}

void CalendarMgr::SendCalendarClearPendingAction(Player* player)
{
    if (player)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_CLEAR_PENDING_ACTION TO [%s]", player->GetObjectGuid().GetString().c_str());
        WorldPacket data(SMSG_CALENDAR_CLEAR_PENDING_ACTION, 0);
        player->SendDirectMessage(&data);
    }
}

void CalendarMgr::SendCalendarEventModeratorStatusAlert(CalendarInvite const* invite)
{
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_EVENT_MODERATOR_STATUS_ALERT");
    CalendarEvent const* event = invite->GetCalendarEvent();
    WorldPacket data(SMSG_CALENDAR_EVENT_MODERATOR_STATUS_ALERT, 8 + 8 + 1 + 1);
    data << invite->InviteeGuid.WriteAsPacked();
    data << uint64(event->EventId);
    data << uint8(invite->Rank);
    data << uint8(1); // Display pending action to client?
    //data.hexlike();
    SendPacketToAllEventRelatives(data, event);
}

void CalendarMgr::SendCalendarEventUpdateAlert(CalendarEvent const* event, time_t oldEventTime)
{
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_EVENT_UPDATED_ALERT");
    WorldPacket data(SMSG_CALENDAR_EVENT_UPDATED_ALERT, 1 + 8 + 4 + 4 + 4 + 1 + 4 + event->Title.size() + 1); // +1 for the string's NUL terminator
    data << uint8(1);       // show pending alert?
    data << uint64(event->EventId);
    data << secsToTimeBitFields(oldEventTime);
    data << uint32(event->Flags);
    data << secsToTimeBitFields(event->EventTime);
    data << uint8(event->Type);
    data << int32(event->DungeonId);
    data << event->Title;
    // Client's SMSG_CALENDAR_EVENT_UPDATED_ALERT reader stops after Title -- it never reads a
    // Description, Repeatable, MaxInvites, or UnknownTime field for this opcode (those exist on
    // SMSG_CALENDAR_SEND_EVENT and SMSG_CALENDAR_UPDATE_EVENT, not here). Writing them was inert
    // bandwidth, not a desync risk, but it's dead code that misleads a reader into thinking the
    // client uses them. Dropped rather than kept "just in case".
    //data.hexlike();

    SendPacketToAllEventRelatives(data, event);
}

void CalendarMgr::SendPacketToAllEventRelatives(WorldPacket packet, CalendarEvent const* event)
{
    // Send packet to all guild members
    if (event->IsGuildEvent() || event->IsGuildAnnouncement())
        if (Guild* guild = sGuildMgr.GetGuildById(event->GuildId))
        {
            guild->BroadcastPacket(&packet);
        }

    // Send packet to all invitees if event is non-guild, in other case only to non-guild invitees (packet was broadcasted for them)
    CalendarInviteMap const* cInvMap = event->GetInviteMap();
    for (CalendarInviteMap::const_iterator itr = cInvMap->begin(); itr != cInvMap->end(); ++itr)
        if (Player* player = sPlayerRegistry.Find(itr->second->InviteeGuid))
            if (!event->IsGuildEvent() || (event->IsGuildEvent() && player->GetGuildId() != event->GuildId))
            {
                player->SendDirectMessage(&packet);
            }
}

void CalendarMgr::SendCalendarRaidLockoutRemove(Player* player, DungeonPersistentState const* save)
{
    if (!save || !player)
    {
        return;
    }

    DEBUG_LOG("SMSG_CALENDAR_RAID_LOCKOUT_REMOVED [%s]", player->GetObjectGuid().GetString().c_str());
    time_t currTime = time(NULL);

    WorldPacket data(SMSG_CALENDAR_RAID_LOCKOUT_REMOVED, 4 + 4 + 4 + 8);
    data << uint32(save->GetMapId());
    data << uint32(save->GetDifficulty());
    data << uint32(save->GetResetTime() - currTime);
    data << uint64(save->GetInstanceId());
    //data.hexlike();
    player->SendDirectMessage(&data);
}

void CalendarMgr::SendCalendarRaidLockoutAdd(Player* player, DungeonPersistentState const* save)
{
    if (!save || !player)
    {
        return;
    }

    DEBUG_LOG("SMSG_CALENDAR_RAID_LOCKOUT_ADDED [%s]", player->GetObjectGuid().GetString().c_str());
    time_t currTime = time(NULL);

    WorldPacket data(SMSG_CALENDAR_RAID_LOCKOUT_ADDED, 4 + 4 + 4 + 4 + 8);
    data << secsToTimeBitFields(currTime);
    data << uint32(save->GetMapId());
    data << uint32(save->GetDifficulty());
    data << uint32(save->GetResetTime() - currTime);
    data << uint64(save->GetInstanceId());
    //data.hexlike();
    player->SendDirectMessage(&data);
}
