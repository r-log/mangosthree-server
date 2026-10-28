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

#include "session/handlers/social/CalendarHandlers.h"

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

void CalendarHandlers::HandleCalendarGetCalendar(WorldSession& session, WorldPacket& /*recv_data*/)
{
    ObjectGuid guid = session.GetPlayer()->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_GET_CALENDAR [%s]", guid.GetString().c_str());

    time_t currTime = time(NULL);

    WorldPacket data(SMSG_CALENDAR_SEND_CALENDAR);

    CalendarInvitesList invites;
    sCalendarMgr.GetPlayerInvitesList(guid, invites);

    data << uint32(invites.size());
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Sending > %zu invites", invites.size());

    for (CalendarInvitesList::const_iterator itr = invites.begin(); itr != invites.end(); ++itr)
    {
        CalendarEvent const* event = (*itr)->GetCalendarEvent();
        MANGOS_ASSERT(event);                           // TODO: be sure no way to have a null event

        data << uint64(event->EventId);
        data << uint64((*itr)->InviteId);
        data << uint8((*itr)->Status);
        data << uint8((*itr)->Rank);

        data << uint8(event->IsGuildEvent());
        data << event->CreatorGuid.WriteAsPacked();
        DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "invite> EventId[" UI64FMTD "], InviteId[" UI64FMTD "], status[%u], rank[%u]",
                         event->EventId, (*itr)->InviteId, uint32((*itr)->Status), uint32((*itr)->Rank));
    }

    CalendarEventsList events;
    sCalendarMgr.GetPlayerEventsList(guid, events);

    data << uint32(events.size());
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Sending > %zu events", events.size());

    for (CalendarEventsList::const_iterator itr = events.begin(); itr != events.end(); ++itr)
    {
        CalendarEvent const* event = *itr;

        data << uint64(event->EventId);
        data << event->Title;
        data << uint32(event->Type);
        data << secsToTimeBitFields(event->EventTime);
        data << uint32(event->Flags);
        data << int32(event->DungeonId);

        // Client reads a fixed uint64 here before the packed creator GUID (event->GuildId
        // is only the guild's uint32 id, not the GUID the client wants). Without it the first
        // event shifts every field after it and the client hangs parsing the packet; an empty
        // calendar never enters this loop.
        Guild* guild = sGuildMgr.GetGuildById(event->GuildId);
        data << uint64(guild ? guild->GetObjectGuid().GetRawValue() : 0);

        data << event->CreatorGuid.WriteAsPacked();

        std::string timeStr = TimeToTimestampStr(event->EventTime);
        DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Events> EventId[" UI64FMTD "], Title[%s], Time[%s], Type[%u],  Flag[%u], DungeonId[%d], CreatorGuid[%s]",
                         event->EventId, event->Title.c_str(), timeStr.c_str(), uint32(event->Type),
                         uint32(event->Flags), event->DungeonId, event->CreatorGuid.GetString().c_str());
    }

    data << uint32(currTime);                               // server time
    data << secsToTimeBitFields(currTime);                  // zone time ??

    ByteBuffer dataBuffer;
    uint32 boundCounter = 0;
    for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
    {
        Player::BoundInstancesMap boundInstances = session.GetPlayer()->GetBoundInstances(Difficulty(i));
        for (Player::BoundInstancesMap::const_iterator itr = boundInstances.begin(); itr != boundInstances.end(); ++itr)
        {
            if (itr->second.perm)
            {
                DungeonPersistentState const* state = itr->second.state;
                dataBuffer << uint32(state->GetMapId());
                dataBuffer << uint32(state->GetDifficulty());
                dataBuffer << uint32(state->GetResetTime() - currTime);
                dataBuffer << uint64(state->GetInstanceId());   // instance save id as unique instance copy id
                ++boundCounter;
            }
        }
    }

    data << uint32(boundCounter);
    data.append(dataBuffer);

    data << uint32(1135753200);                             // Constant date, unk (28.12.2005 07:00)

    // Reuse variables
    boundCounter = 0;
    std::set<uint32> sentMaps;
    dataBuffer.clear();

    for (MapDifficultyMap::const_iterator itr = sMapDifficultyMap.begin(); itr != sMapDifficultyMap.end(); ++itr)
    {
        uint32 map_diff_pair = itr->first;
        uint32 mapId = PAIR32_LOPART(map_diff_pair);
        Difficulty difficulty = Difficulty(PAIR32_HIPART(map_diff_pair));
        MapDifficultyEntry const* mapDiff = itr->second;

        // skip mapDiff without global reset time
        if (!mapDiff->RaidDuration)
        {
            continue;
        }

        // skip non raid map
        MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
        if (!mapEntry || !mapEntry->IsRaid())
        {
            continue;
        }

        // skip already sent map (not same difficulty?)
        if (sentMaps.find(mapId) != sentMaps.end())
        {
            continue;
        }

        uint32 resetTime = sMapPersistentStateMgr.GetScheduler().GetMaxResetTimeFor(mapDiff);

        sentMaps.insert(mapId);
        dataBuffer << mapId;
        dataBuffer << resetTime;

        DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "MapId [%u] -> Reset Time: %u", mapId, resetTime);
        dataBuffer << int32(0); // showed 68400 on map 509 must investigate more
        ++boundCounter;
    }
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Map sent [%u]", boundCounter);

    data << uint32(boundCounter);
    data.append(dataBuffer);

    // TODO: Fix this, how we do know how many and what holidays to send?
    uint32 holidayCount = 0;
    data << uint32(holidayCount);
    /*for (uint32 i = 0; i < holidayCount; ++i)
    {
        HolidaysEntry const* holiday = sHolidaysStore.LookupEntry(666);

        data << uint32(holiday->Id);                        // m_ID
        data << uint32(holiday->Region);                    // m_region, might be looping
        data << uint32(holiday->Looping);                   // m_looping, might be region
        data << uint32(holiday->Priority);                  // m_priority
        data << uint32(holiday->CalendarFilterType);        // m_calendarFilterType

        for (uint8 j = 0; j < MAX_HOLIDAY_DATES; ++j)
        {
            data << uint32(holiday->Date[j]);               // 26 * m_date -- WritePackedTime ?
        }

        for (uint8 j = 0; j < MAX_HOLIDAY_DURATIONS; ++j)
        {
            data << uint32(holiday->Duration[j]);           // 10 * m_duration
        }

        for (uint8 j = 0; j < MAX_HOLIDAY_FLAGS; ++j)
        {
            data << uint32(holiday->CalendarFlags[j]);      // 10 * m_calendarFlags
        }

        data << holiday->TextureFilename;                   // m_textureFilename (holiday name)
    }*/

    session.SendPacket(&data);
}

void CalendarHandlers::HandleCalendarGetEvent(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid = session.GetPlayer()->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_GET_EVENT [%s]", guid.GetString().c_str());

    uint64 eventId;
    recv_data >> eventId;

    if (CalendarEvent* event = sCalendarMgr.GetEventById(eventId))
    {
        // The reply carries the title, the description, the times and the full
        // invitee list. Event ids are sequential, so without this any player
        // could read every event on the realm by counting upwards. Answer
        // exactly as for an id that does not exist -- telling a stranger that
        // one is real is already half the disclosure.
        if (!event->IsVisibleTo(guid, session.GetPlayer()->GetGuildId()))
        {
            sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_EVENT_INVALID);
            return;
        }

        sCalendarMgr.SendCalendarEvent(session.GetPlayer(), event, CALENDAR_SENDTYPE_GET);
    }
    else
    {
        sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_EVENT_INVALID);
    }
}

void CalendarHandlers::HandleCalendarGuildFilter(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_GUILD_FILTER [%s]", session.GetPlayer()->GetGuidStr().c_str());

    uint32 minLevel;
    uint32 maxLevel;
    uint32 minRank;

    recv_data >> minLevel >> maxLevel >> minRank;

    if (Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId()))
    {
        guild->MassInviteToEvent(&session, minLevel, maxLevel, minRank);
    }

    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Min level [%u], Max level [%u], Min rank [%u]", minLevel, maxLevel, minRank);
}

void CalendarHandlers::HandleCalendarEventSignup(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid = session.GetPlayer()->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_EVENT_SIGNUP [%s]", guid.GetString().c_str());

    uint64 eventId;
    bool tentative;

    recv_data >> eventId;
    recv_data >> tentative; // uint8 == bool size in all compilator???
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "] Tentative %u", eventId, uint32(tentative));

    if (CalendarEvent* event = sCalendarMgr.GetEventById(eventId))
    {
        if (event->IsGuildEvent() && event->GuildId != session.GetPlayer()->GetGuildId())
        {
            sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_GUILD_PLAYER_NOT_IN_GUILD);
            return;
        }

        CalendarInviteStatus status = tentative ? CALENDAR_STATUS_TENTATIVE : CALENDAR_STATUS_SIGNED_UP;
        sCalendarMgr.AddInvite(event, guid, guid, CalendarInviteStatus(status), CALENDAR_RANK_PLAYER, "", time(NULL));
        sCalendarMgr.SendCalendarClearPendingAction(session.GetPlayer());
    }
    else
    {
        sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_EVENT_INVALID);
    }
}

void CalendarHandlers::HandleCalendarArenaTeam(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_ARENA_TEAM [%s]", session.GetPlayer()->GetGuidStr().c_str());
    uint32 areanTeamId;
    recv_data >> areanTeamId;

    if (ArenaTeam* team = sObjectMgr.GetArenaTeamById(areanTeamId))
    {
        // The reply is the team's roster -- every member's guid and level. The
        // id comes from the packet, so answering one the sender does not belong
        // to hands out the membership of every arena team on the realm. Say
        // nothing at all: the client only asks this for a team it is on.
        if (!team->HaveMember(session.GetPlayer()->GetObjectGuid()))
        {
            return;
        }

        team->MassInviteToEvent(&session);
    }
}

void CalendarHandlers::HandleCalendarAddEvent(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid = session.GetPlayer()->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_ADD_EVENT [%s]", guid.GetString().c_str());

    std::string title;
    std::string description;
    uint8 type;
    uint8 repeatable;
    uint32 maxInvites;
    int32 dungeonId;
    uint32 eventPackedTime;
    uint32 unkPackedTime;
    uint32 flags;

    recv_data >> title;
    recv_data >> description;
    recv_data >> type;
    recv_data >> repeatable;
    recv_data >> maxInvites;
    recv_data >> dungeonId;
    recv_data >> eventPackedTime;
    recv_data >> unkPackedTime;
    recv_data >> flags;

    // prevent events in the past
    //
    // CalendarPackedTimeIsPast() unpacks the time before it compares: compared still
    // packed against GameTime::GetGameTime(), every possible date is "in the past".
    if (CalendarPackedTimeIsPast(eventPackedTime, GameTime::GetGameTime(), time_t(86400L)))
    {
        recv_data.rfinish();
        sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_EVENT_PASSED);
        return;
    }

    time_t const eventTime = CalendarPackedTimeToTimestamp(eventPackedTime);

    // 946684800 is 01/01/2000 00:00:00 - default response time
    CalendarEvent* cal =  sCalendarMgr.AddEvent(session.GetPlayer()->GetObjectGuid(), title, description, type, repeatable, maxInvites, dungeonId, eventTime, timeBitFieldsToSecs(unkPackedTime), flags);

    if (cal)
    {
        if (cal->IsGuildAnnouncement())
        {
            sCalendarMgr.AddInvite(cal, guid, ObjectGuid(uint64(0)),  CALENDAR_STATUS_NOT_SIGNED_UP, CALENDAR_RANK_PLAYER, "", time(NULL));
        }
        else
        {
            uint32 inviteCount;
            recv_data >> inviteCount;

            for (uint32 i = 0; i < inviteCount; ++i)
            {
                ObjectGuid invitee;
                uint8 status = 0;
                uint8 rank = 0;
                recv_data >> invitee.ReadAsPacked();
                recv_data >> status;
                recv_data >> rank;

                sCalendarMgr.AddInvite(cal, guid, invitee, CalendarInviteStatus(status), CalendarModerationRank(rank), "", time(NULL));
            }
        }
        sCalendarMgr.SendCalendarEvent(session.GetPlayer(), cal, CALENDAR_SENDTYPE_ADD);
    }
}

void CalendarHandlers::HandleCalendarUpdateEvent(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid = session.GetPlayer()->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_UPDATE_EVENT [%s]", guid.GetString().c_str());

    time_t oldEventTime;
    uint64 eventId;
    uint64 inviteId;
    std::string title;
    std::string description;
    uint8 type;
    uint8 repetitionType;
    uint32 maxInvites;
    int32 dungeonId;
    uint32 eventPackedTime;
    uint32 UnknownPackedTime;
    uint32 flags;

    recv_data >> eventId >> inviteId >> title >> description >> type >> repetitionType >> maxInvites >> dungeonId;
    recv_data >> eventPackedTime;
    recv_data >> UnknownPackedTime;
    recv_data >> flags;

    // prevent events in the past -- see HandleCalendarAddEvent /
    // CalendarPackedTimeIsPast() for why this has to unpack before comparing.
    if (CalendarPackedTimeIsPast(eventPackedTime, GameTime::GetGameTime(), time_t(86400L)))
    {
        recv_data.rfinish();
        sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_EVENT_PASSED);
        return;
    }

    time_t const eventTime = CalendarPackedTimeToTimestamp(eventPackedTime);

    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "], InviteId [" UI64FMTD "] Title %s, Description %s, type %u "
                     "Repeatable %u, MaxInvites %u, Dungeon ID %d, Flags %u", eventId, inviteId, title.c_str(),
                     description.c_str(), uint32(type), uint32(repetitionType), maxInvites, dungeonId, flags);

    if (CalendarEvent* event = sCalendarMgr.GetEventById(eventId))
    {
        if (guid != event->CreatorGuid)
        {
            CalendarInvite* updaterInvite = event->GetInviteByGuid(guid);
            if (updaterInvite == NULL)
            {
                sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_NOT_INVITED);
                return ;
            }

            if (updaterInvite->Rank != CALENDAR_RANK_MODERATOR)
            {
                // remover have not enough right to change invite status
                sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_PERMISSIONS);
                return;
            }
        }

        oldEventTime = event->EventTime;

        event->Type = CalendarEventType(type);
        event->Flags = flags;
        event->EventTime = eventTime;
        event->UnknownTime = timeBitFieldsToSecs(UnknownPackedTime);
        event->DungeonId = dungeonId;
        event->Title = title;
        event->Description = description;

        sCalendarMgr.SendCalendarEventUpdateAlert(event, oldEventTime);

        // The title and the description are bound, not escaped, so the write takes no
        // connection lock on the world thread. It writes the six columns this handler
        // assigned above.
        CalendarMgr::WriteEventUpdateToDB(*event);
    }
    else
    {
        sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_EVENT_INVALID);
    }
}

void CalendarHandlers::HandleCalendarRemoveEvent(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_REMOVE_EVENT [%s]", session.GetPlayer()->GetGuidStr().c_str());

    uint64 eventId;
    uint64 inviteId;
    uint32 Flags;

    recv_data >> eventId;
    recv_data >> inviteId;
    recv_data >> Flags;
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Remove event (eventId=" UI64FMTD ", remover inviteId =" UI64FMTD ")", eventId, inviteId);
    sCalendarMgr.RemoveEvent(eventId, session.GetPlayer());
}

void CalendarHandlers::HandleCalendarCopyEvent(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid = session.GetPlayer()->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_COPY_EVENT [%s]", guid.GetString().c_str());

    uint64 eventId;
    uint64 inviteId;
    uint32 packedTime;

    recv_data >> eventId >> inviteId;
    recv_data >> packedTime;
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "] inviteId [" UI64FMTD "]",
                     eventId, inviteId);

    // prevent events in the past -- see HandleCalendarAddEvent /
    // CalendarPackedTimeIsPast() for why this has to unpack before comparing.
    if (CalendarPackedTimeIsPast(packedTime, GameTime::GetGameTime(), time_t(86400L)))
    {
        recv_data.rfinish();
        sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_EVENT_PASSED);
        return;
    }

    time_t const eventTime = CalendarPackedTimeToTimestamp(packedTime);

    sCalendarMgr.CopyEvent(eventId, eventTime, guid);
}

void CalendarHandlers::HandleCalendarEventInvite(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid playerGuid = session.GetPlayer()->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_EVENT_INVITE [%s]", playerGuid.GetString().c_str());

    // TODO it seem its not inviteID but event->CreatorGuid
    uint64 inviteId;

    CalendarInviteRequest request;
    request.playerGuid = playerGuid;

    recv_data >> request.eventId >> inviteId >> request.name >> request.isPreInvite >> request.isGuildEvent;

    if (Player* player = sPlayerRegistry.FindByName(request.name.c_str()))
    {
        // Invitee is online: everything below is memory, so this half of the handler still
        // answers in the tick it arrived in.
        request.inviteeGuid = player->GetObjectGuid();
        request.inviteeTeam = player->GetTeam();
        request.inviteeGuildId = player->GetGuildId();

        FinishCalendarEventInvite(session.GetPlayer(), request, player->GetSocial()->HasIgnore(playerGuid));
        return;
    }

    // Invitee offline: the character cache holds the guid, the race and the guild id, and
    // GetByName compares names the way the utf8_general_ci column does.
    CharacterCacheRef cached = sCharacterCache.GetByName(request.name);
    if (!cached)
    {
        // No cache entry is no character: the invitee guid stays empty, and the invite
        // answers CALENDAR_ERROR_PLAYER_NOT_FOUND.
        FinishCalendarEventInvite(session.GetPlayer(), request, false);
        return;
    }

    ResolveCalendarInvitee(*cached, request);

    // The one fact about an OFFLINE invitee that is not cached: whether he ignores the
    // inviter. `character_social` is the invitee's own list, loaded into PlayerSocial at HIS
    // login, so an online inviter cannot answer it from memory and it is not a per-login
    // cache this session could carry either. It becomes a continuation: the read is
    // queued, nothing has been mutated in front of it (this handler only sends packets and,
    // at its very end, creates the invite), and the rest of the handler runs a tick later.
    QueueCalendarInviteIgnoreRead(session.GetAccountId(), session.GetSessionId(), request);
}

/**
 * @brief Fills the invitee's three fields from his cache entry.
 *
 * The three in one place, so that the handler and the test that pins it read the same three
 * assignments rather than two copies that can drift apart.
 *
 * @param cached The invitee's character-cache entry.
 * @param request The invite being built.
 */
void CalendarHandlers::ResolveCalendarInvitee(CharacterCacheEntry const& cached, CalendarInviteRequest& request)
{
    request.inviteeGuid = cached.guid;
    request.inviteeTeam = Player::TeamForRace(cached.race);
    request.inviteeGuildId = cached.guildId;
}

/**
 * @brief Stages the calendar invite's only read.
 *
 * Split out of the handler so it can be driven on its own: everything it needs is in its
 * arguments, so a test can queue exactly what a real request queues without a Player.
 *
 * @param accountId The inviting account.
 * @param sessionId The session the request arrived on.
 * @param request The invite, with the invitee already resolved from the character cache.
 */
void CalendarHandlers::QueueCalendarInviteIgnoreRead(uint32 accountId, proto::SessionId sessionId,
                                                 CalendarInviteRequest request)
{
    SocialRepository::QueryFlags(request.inviteeGuid.GetCounter(), request.playerGuid.GetCounter(),
                                 [accountId, sessionId, request](std::optional<uint8> flags)
                                 {
                                     CalendarHandlers::HandleCalendarInviteIgnoreCallback(flags, accountId, sessionId, request);
                                 });
}

/**
 * @brief Finishes the invite once the offline invitee's ignore flag is known.
 *
 * @param flags The invitee's social flags for the inviter, if the invitee lists them.
 * @param accountId The inviting account.
 * @param sessionId The session the request arrived on.
 * @param request The invite as the handler captured it.
 */
void CalendarHandlers::HandleCalendarInviteIgnoreCallback(std::optional<uint8> const& flags, uint32 accountId,
                                                      proto::SessionId sessionId, CalendarInviteRequest request)
{
    WorldSession* session = NULL;
    Player* player = NULL;
    if (!WorldSession::FindRequesterPlayer(accountId, sessionId, request.playerGuid, session, player))
    {
        return;
    }

    bool isIgnored = false;
    if (flags)
    {
        if (*flags & SOCIAL_FLAG_IGNORED)
        {
            isIgnored = true;
        }
    }

    // The invitee was resolved from the character cache in the handler's tick; a delete
    // continuation can have removed the character since.
    if (!sCharacterCache.GetByGuid(request.inviteeGuid))
    {
        sCalendarMgr.SendCalendarCommandResult(player, CALENDAR_ERROR_PLAYER_NOT_FOUND);
        return;
    }

    FinishCalendarEventInvite(player, request, isIgnored);
}

/**
 * @brief The whole of CMSG_CALENDAR_EVENT_INVITE below its two lookups, verbatim.
 *
 * Shared by the online path (called in the arriving tick) and the offline path (called from
 * the continuation a tick later), so neither can drift from the other.
 *
 * @param player The inviter, re-found through the registry on the deferred path.
 * @param request The invite, with the invitee resolved.
 * @param isIgnored Whether the invitee ignores the inviter.
 */
void CalendarHandlers::FinishCalendarEventInvite(Player* player, CalendarInviteRequest const& request, bool isIgnored)
{
    if (request.inviteeGuid.IsEmpty())
    {
        sCalendarMgr.SendCalendarCommandResult(player, CALENDAR_ERROR_PLAYER_NOT_FOUND);
        return;
    }

    if (isIgnored)
    {
        sCalendarMgr.SendCalendarCommandResult(player, CALENDAR_ERROR_IGNORING_YOU_S, request.name.c_str());
        return;
    }

    if (player->GetTeam() != request.inviteeTeam && !sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CALENDAR))
    {
        sCalendarMgr.SendCalendarCommandResult(player, CALENDAR_ERROR_NOT_ALLIED);
        return;
    }

    if (!request.isPreInvite)
    {
        if (CalendarEvent* event = sCalendarMgr.GetEventById(request.eventId))
        {
            // Only the creator or a moderator may add people, which is the rule
            // HandleCalendarUpdateEvent already applies to editing the event.
            // Without it any player could attach anyone to any event id, and the
            // invitation arrives naming the event's owner rather than the sender.
            if (request.playerGuid != event->CreatorGuid)
            {
                CalendarInvite* inviterInvite = event->GetInviteByGuid(request.playerGuid);
                if (inviterInvite == NULL)
                {
                    sCalendarMgr.SendCalendarCommandResult(player, CALENDAR_ERROR_NOT_INVITED);
                    return;
                }

                if (inviterInvite->Rank != CALENDAR_RANK_MODERATOR &&
                    inviterInvite->Rank != CALENDAR_RANK_OWNER)
                {
                    sCalendarMgr.SendCalendarCommandResult(player, CALENDAR_ERROR_PERMISSIONS);
                    return;
                }
            }

            if (event->IsGuildEvent() && event->GuildId == request.inviteeGuildId)
            {
                // we can't invite guild members to guild events
                sCalendarMgr.SendCalendarCommandResult(player, CALENDAR_ERROR_NO_GUILD_INVITES);
                return;
            }

            sCalendarMgr.AddInvite(event, request.playerGuid, request.inviteeGuid, CALENDAR_STATUS_INVITED, CALENDAR_RANK_PLAYER, "", time(NULL));
        }
        else
        {
            sCalendarMgr.SendCalendarCommandResult(player, CALENDAR_ERROR_EVENT_INVALID);
        }
    }
    else
    {
        if (request.isGuildEvent && request.inviteeGuildId == player->GetGuildId())
        {
            sCalendarMgr.SendCalendarCommandResult(player, CALENDAR_ERROR_NO_GUILD_INVITES);
            return;
        }

        // create a temp invite to send it back to client
        CalendarInvite invite;
        invite.SenderGuid = request.playerGuid;
        invite.InviteeGuid = request.inviteeGuid;
        invite.Status = CALENDAR_STATUS_INVITED;
        invite.Rank = CALENDAR_RANK_PLAYER;
        invite.LastUpdateTime = time(NULL);

        sCalendarMgr.SendCalendarEventInvite(&invite);
        DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "PREINVITE> sender[%s], Invitee[%s]", request.playerGuid.GetString().c_str(), request.inviteeGuid.GetString().c_str());
    }
}

void CalendarHandlers::HandleCalendarEventRsvp(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid = session.GetPlayer()->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_EVENT_RSVP [%s]", guid.GetString().c_str());

    uint64 eventId;
    uint64 inviteId;
    uint32 status;

    recv_data >> eventId >> inviteId >> status;
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "], InviteId [" UI64FMTD "], status %u",
                     eventId, inviteId, status);

    if (CalendarEvent* event = sCalendarMgr.GetEventById(eventId))
    {
        // i think we still should be able to remove self from locked events
        if (status != CALENDAR_STATUS_REMOVED && event->Flags & CALENDAR_FLAG_INVITES_LOCKED)
        {
            sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_EVENT_LOCKED);
            return;
        }

        if (CalendarInvite* invite = event->GetInviteById(inviteId))
        {
            if (invite->InviteeGuid != guid)
            {
                CalendarInvite* updaterInvite = event->GetInviteByGuid(guid);
                if (updaterInvite == NULL)
                {
                    sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_NOT_INVITED);
                    return ;
                }

                if (updaterInvite->Rank != CALENDAR_RANK_MODERATOR && updaterInvite->Rank != CALENDAR_RANK_OWNER)
                {
                    // remover have not enough right to change invite status
                    sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_PERMISSIONS);
                    return;
                }
            }
            invite->Status = CalendarInviteStatus(status);
            invite->LastUpdateTime = time(NULL);

            CalendarRepository::SetInviteReply(invite->InviteId, status, uint32(invite->LastUpdateTime));
            sCalendarMgr.SendCalendarEventStatus(invite);
            sCalendarMgr.SendCalendarClearPendingAction(session.GetPlayer());
        }
        else
        {
            sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_NO_INVITE); // correct?
        }
    }
    else
    {
        sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_EVENT_INVALID);
    }
}

void CalendarHandlers::HandleCalendarEventRemoveInvite(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid = session.GetPlayer()->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_EVENT_REMOVE_INVITE [%s]", guid.GetString().c_str());

    ObjectGuid invitee;
    uint64 eventId;
    uint64 ownerInviteId; // isn't it sender's inviteId? TODO: need more test to see what we can do with that value
    uint64 inviteId;

    recv_data >> invitee.ReadAsPacked();
    recv_data >> inviteId >> ownerInviteId >> eventId;

    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "], ownerInviteId [" UI64FMTD "], Invitee ([%s] id: [" UI64FMTD "])",
                     eventId, ownerInviteId, invitee.GetString().c_str(), inviteId);

    if (CalendarEvent* event = sCalendarMgr.GetEventById(eventId))
    {
        sCalendarMgr.RemoveInvite(eventId, inviteId, guid);
    }
    else
    {
        sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_EVENT_INVALID);
    }
}

void CalendarHandlers::HandleCalendarEventStatus(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid updaterGuid = session.GetPlayer()->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_EVENT_STATUS [%s]", updaterGuid.GetString().c_str());
    recv_data.hexlike();

    ObjectGuid invitee;
    uint64 eventId;
    uint64 inviteId;
    uint64 ownerInviteId; // isn't it sender's inviteId?
    uint32 status;

    recv_data >> invitee.ReadAsPacked();
    recv_data >> eventId >> inviteId >> ownerInviteId >> status;
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "] ownerInviteId [" UI64FMTD "], Invitee ([%s] id: [" UI64FMTD "], status %u",
                     eventId, ownerInviteId, invitee.GetString().c_str(), inviteId, status);

    if (CalendarEvent* event = sCalendarMgr.GetEventById(eventId))
    {
        if (CalendarInvite* invite = event->GetInviteById(inviteId))
        {
            if (invite->InviteeGuid != updaterGuid)
            {
                CalendarInvite* updaterInvite = event->GetInviteByGuid(updaterGuid);
                if (updaterInvite == NULL)
                {
                    sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_NOT_INVITED);
                    return ;
                }

                if (updaterInvite->Rank != CALENDAR_RANK_MODERATOR && updaterInvite->Rank != CALENDAR_RANK_OWNER)
                {
                    // remover have not enough right to change invite status
                    sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_PERMISSIONS);
                    return;
                }
            }
            invite->Status = (CalendarInviteStatus)status;
            invite->LastUpdateTime = time(NULL);            // not sure if we should set response time when moderator changes invite status

            CalendarRepository::SetInviteStatus(invite->InviteId, status, uint32(invite->LastUpdateTime));
            sCalendarMgr.SendCalendarEventStatus(invite);
            sCalendarMgr.SendCalendarClearPendingAction(sObjectMgr.GetPlayer(invitee));
        }
        else
        {
            sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_NO_INVITE);
        }
    }
    else
    {
        sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_EVENT_INVALID);
    }
}

void CalendarHandlers::HandleCalendarEventModeratorStatus(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid = session.GetPlayer()->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_EVENT_MODERATOR_STATUS [%s]", guid.GetString().c_str());

    ObjectGuid invitee;
    uint64 eventId;
    uint64 inviteId;
    uint64 ownerInviteId; // isn't it sender's inviteId?
    uint32 rank;

    recv_data >> invitee.ReadAsPacked();
    recv_data >> eventId >>  inviteId >> ownerInviteId >> rank;
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "] ownerInviteId [" UI64FMTD "], Invitee ([%s] id: [" UI64FMTD "], rank %u",
                     eventId, ownerInviteId, invitee.GetString().c_str(), inviteId, rank);

    if (CalendarEvent* event = sCalendarMgr.GetEventById(eventId))
    {
        if (CalendarInvite* invite = event->GetInviteById(inviteId))
        {
            if (invite->InviteeGuid != guid)
            {
                CalendarInvite* updaterInvite = event->GetInviteByGuid(guid);
                if (updaterInvite == NULL)
                {
                    sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_NOT_INVITED);
                    return ;
                }
                if (updaterInvite->Rank != CALENDAR_RANK_MODERATOR && updaterInvite->Rank != CALENDAR_RANK_OWNER)
                {
                    // remover have not enough right to change invite status
                    sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_PERMISSIONS);
                    return;
                }
            }

            if (CalendarModerationRank(rank) == CALENDAR_RANK_OWNER)
            {
                // cannot set owner
                sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_PERMISSIONS);
                return;
            }

            CalendarRepository::SetInviteRank(invite->InviteId, rank);
            invite->Rank = CalendarModerationRank(rank);
            sCalendarMgr.SendCalendarEventModeratorStatusAlert(invite);
        }
        else
        {
            sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_NO_INVITE);
        }
    }
    else
    {
        sCalendarMgr.SendCalendarCommandResult(session.GetPlayer(), CALENDAR_ERROR_EVENT_INVALID);
    }
}

void CalendarHandlers::HandleCalendarComplain(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid = session.GetPlayer()->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_COMPLAIN [%s]", guid.GetString().c_str());

    ObjectGuid badGuyGuid;
    uint64 eventId;
    uint64 inviteId;

    recv_data >> badGuyGuid;
    recv_data >> eventId >>  inviteId;
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "], BadGuyGuid ([%s] inviteId: [" UI64FMTD "])",
                     eventId, badGuyGuid.GetString().c_str(), inviteId);

    // Remove the invite
    if (sCalendarMgr.RemoveInvite(eventId, inviteId, guid))
    {
        WorldPacket data(SMSG_COMPLAIN_RESULT, 1 + 1);
        data << uint8(0);
        data << uint8(0); // show complain saved. We can send 0x0C to show windows with ok button
        session.SendPacket(&data);
    }
}

void CalendarHandlers::HandleCalendarGetNumPending(WorldSession& session, WorldPacket& /*recv_data*/)
{
    ObjectGuid guid = session.GetPlayer()->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_GET_NUM_PENDING [%s]", guid.GetString().c_str());

    uint32 pending = sCalendarMgr.GetPlayerNumPending(guid);

    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Pending: %u", pending);

    WorldPacket data(SMSG_CALENDAR_SEND_NUM_PENDING, 4);
    data << uint32(pending);
    session.SendPacket(&data);
}
