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
 * @file GameEventMgr.cpp
 * @brief Game event system implementation
 *
 * This file implements GameEventMgr which manages time-based game events
 * such as holidays, special occasions, and scheduled content. Features:
 *
 * - Event scheduling with start/end times
 * - Event occurrence patterns (daily, weekly, custom)
 * - Event condition checking
 * - Creature/gameobject spawning/despawning for events
 * - Quest availability based on events
 * - Mail distribution for event rewards
 *
 * Events are loaded from the `game_event` database tables.
 *
 * @see GameEventMgr for the manager class
 */

#include <sstream>
#include <map>
#include "GameEventMgr.h"
#include "data/GameConfig.h"
#include "ObjectMgr.h"
#include "DBCStores.h"
#include "ObjectGuid.h"
#include "PoolManager.h"
#include "ProgressBar.h"
#include "Log.h"
#include "Policies/Singleton.h"
#include "persistence/world/events/GameEventDataRepository.h"

/**
 * @brief Checks whether a specific game event is active at the given time.
 *
 * @param entry The game event id.
 * @param currenttime The time to evaluate.
 * @return true if the event is active; otherwise false.
 */
bool GameEventMgr::CheckOneGameEvent(uint16 entry, time_t currenttime) const
{
    // Get the event information
    if (mGameEvent[entry].start < currenttime && currenttime < mGameEvent[entry].end &&
        ((currenttime - mGameEvent[entry].start) % (mGameEvent[entry].occurence * MINUTE)) < (mGameEvent[entry].length * MINUTE))
    {
        return true;
    }
    else
    {
        return false;
    }
}

/**
 * @brief Calculates the next time interval before an event state may change.
 *
 * @param entry The game event id.
 * @return uint32 The delay until the next check in seconds.
 */
uint32 GameEventMgr::NextCheck(uint16 entry) const
{
    time_t currenttime = time(NULL);

    // outdated event: we return max
    if (currenttime > mGameEvent[entry].end)
    {
        return max_ge_check_delay;
    }

    // never started event, we return delay before start
    if (mGameEvent[entry].start > currenttime)
    {
        return uint32(mGameEvent[entry].start - currenttime);
    }

    uint32 delay;
    // in event, we return the end of it
    if ((((currenttime - mGameEvent[entry].start) % (mGameEvent[entry].occurence * 60)) < (mGameEvent[entry].length * 60)))
        // we return the delay before it ends
    {
        delay = (mGameEvent[entry].length * MINUTE) - ((currenttime - mGameEvent[entry].start) % (mGameEvent[entry].occurence * MINUTE));
    }
    else                                                    // not in window, we return the delay before next start
    {
        delay = (mGameEvent[entry].occurence * MINUTE) - ((currenttime - mGameEvent[entry].start) % (mGameEvent[entry].occurence * MINUTE));
    }
    // In case the end is before next check
    if (mGameEvent[entry].end  < time_t(currenttime + delay))
    {
        return uint32(mGameEvent[entry].end - currenttime);
    }
    else
    {
        return delay;
    }
}

/**
 * @brief Puts an event's window at the current time: it starts now and ends no earlier than its length.
 *
 * @param event_id The event id.
 */
void GameEventMgr::StartWindowNow(uint16 event_id)
{
    mGameEvent[event_id].start = time(NULL);
    if (mGameEvent[event_id].end <= mGameEvent[event_id].start)
    {
        mGameEvent[event_id].end = mGameEvent[event_id].start + mGameEvent[event_id].length;
    }
}

/**
 * @brief Puts an event's window so that it ended at the current time.
 *
 * @param event_id The event id.
 */
void GameEventMgr::EndWindowNow(uint16 event_id)
{
    mGameEvent[event_id].start = time(NULL) - mGameEvent[event_id].length * MINUTE;
    if (mGameEvent[event_id].end <= mGameEvent[event_id].start)
    {
        mGameEvent[event_id].end = mGameEvent[event_id].start + mGameEvent[event_id].length;
    }
}

/**
 * @brief Loads all game event definitions and related data from the database.
 */
void GameEventMgr::LoadFromDB()
{
    {
        std::optional<uint16> highestEntry = GameEventDataRepository::QueryHighestEventEntry();
        if (!highestEntry)
        {
            sLog.outString(">> Table game_event is empty.");
            sLog.outString();
            return;
        }

        uint32 max_event_id = *highestEntry;

        mGameEvent.resize(max_event_id + 1);
    }

    std::vector<GameEventRow> events = GameEventDataRepository::QueryEvents();
    if (events.empty())
    {
        mGameEvent.clear();
        sLog.outString(">> Table game_event is empty!");
        sLog.outString();
        return;
    }

    uint32 count = 0;

    {
        BarGoLink bar(events.size());
        for (GameEventRow const& row : events)
        {
            ++count;

            bar.step();

            uint16 event_id = row.entry;
            if (event_id == 0)
            {
                sLog.outErrorDb("`game_event` game event id (%i) is reserved and can't be used.", event_id);
                continue;
            }

            GameEventData& pGameEvent = mGameEvent[event_id];
            uint64 starttime        = row.startTime;
            pGameEvent.start        = time_t(starttime);
            uint64 endtime          = row.endTime;
            pGameEvent.end          = time_t(endtime);
            pGameEvent.occurence    = row.occurence;
            pGameEvent.length       = row.length;
            pGameEvent.holiday_id   = HolidayIds(row.holiday);

            if (pGameEvent.length == 0)                     // length>0 is validity check
            {
                sLog.outErrorDb("`game_event` game event id (%i) have length 0 and can't be used.", event_id);
                continue;
            }

            if (pGameEvent.occurence < pGameEvent.length)   // occurence < length is useless. This also asserts that occurence > 0!
            {
                pGameEvent.length = 0;                      // disable event (GameEventData::isValid() returns false when it gets updated)
                sLog.outErrorDb("`game_event` game event id (%i) has occurence %u  < length %u and can't be used.", event_id, pGameEvent.occurence, pGameEvent.length);
                continue;
            }

            if (pGameEvent.holiday_id != HOLIDAY_NONE)
            {
                if (!sHolidaysStore.LookupEntry(pGameEvent.holiday_id))
                {
                    sLog.outErrorDb("`game_event` game event id (%i) have nonexistent holiday id %u.", event_id, pGameEvent.holiday_id);
                    pGameEvent.holiday_id = HOLIDAY_NONE;
                }
            }

            pGameEvent.description  = row.description;
        }

        sLog.outString();
        sLog.outString(">> Loaded %u game events", count);
    }

    std::map<uint16, int16> pool2event;                     // for check unique spawn event associated with pool
    std::map<uint32, int16> creature2event;                 // for check unique spawn event associated with creature
    std::map<uint32, int16> go2event;                       // for check unique spawn event associated with gameobject

    // list only positive event top pools, filled at creature/gameobject loading
    mGameEventSpawnPoolIds.resize(mGameEvent.size());

    mGameEventCreatureGuids.resize(mGameEvent.size() * 2 - 1);
    std::vector<GameEventCreatureRow> eventCreatures = GameEventDataRepository::QueryEventCreatures();

    count = 0;
    if (eventCreatures.empty())
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded %u creatures in game events", count);
    }
    else
    {
        BarGoLink bar(eventCreatures.size());
        for (GameEventCreatureRow const& row : eventCreatures)
        {
            bar.step();

            uint32 guid    = row.guid;
            int16 event_id = row.event;

            if (event_id == 0)
            {
                sLog.outErrorDb("`game_event_creature` game event id (%i) not allowed", event_id);
                continue;
            }

            if (!IsValidEvent(std::abs(event_id)))
            {
                sLog.outErrorDb("`game_event_creature` game event id (%i) not exist in `game_event`", event_id);
                continue;
            }

            int32 internal_event_id = mGameEvent.size() + event_id - 1;

            ++count;

            // spawn objects at event can be grouped in pools and then affected pools have stricter requirements for this case
            if (event_id > 0)
            {
                creature2event[guid] = event_id;

                // not list explicitly creatures from pools in event creature list
                if (uint16 topPoolId =  sPoolMgr.IsPartOfTopPool<Creature>(guid))
                {
                    int16& eventRef = pool2event[topPoolId];
                    if (eventRef != 0)
                    {
                        if (eventRef != event_id)
                        {
                            sLog.outErrorDb("`game_event_creature` have creature (GUID: %u) for event %i from pool or subpool of pool (ID: %u) but pool have already content from event %i. Pool don't must have content for different events!", guid, event_id, topPoolId, eventRef);
                        }
                    }
                    else
                    {
                        eventRef = event_id;
                        mGameEventSpawnPoolIds[event_id].push_back(topPoolId);
                        sPoolMgr.RemoveAutoSpawnForPool(topPoolId);
                    }

                    continue;
                }
            }

            GuidList& crelist = mGameEventCreatureGuids[internal_event_id];
            crelist.push_back(guid);
        }

        sLog.outString();
        sLog.outString(">> Loaded %u creatures in game events", count);
    }

    mGameEventGameobjectGuids.resize(mGameEvent.size() * 2 - 1);
    std::vector<GameEventGameObjectRow> eventGameObjects = GameEventDataRepository::QueryEventGameObjects();

    count = 0;
    if (eventGameObjects.empty())
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded %u gameobjects in game events", count);
    }
    else
    {
        BarGoLink bar(eventGameObjects.size());
        for (GameEventGameObjectRow const& row : eventGameObjects)
        {
            bar.step();

            uint32 guid    = row.guid;
            int16 event_id = row.event;

            if (event_id == 0)
            {
                sLog.outErrorDb("`game_event_gameobject` game event id (%i) not allowed", event_id);
                continue;
            }

            if (!IsValidEvent(std::abs(event_id)))
            {
                sLog.outErrorDb("`game_event_gameobject` game event id (%i) not exist in `game_event`", event_id);
                continue;
            }

            int32 internal_event_id = mGameEvent.size() + event_id - 1;

            ++count;

            // spawn objects at event can be grouped in pools and then affected pools have stricter requirements for this case
            if (event_id > 0)
            {
                go2event[guid] = event_id;

                // not list explicitly gameobjects from pools in event gameobject list
                if (uint16 topPoolId =  sPoolMgr.IsPartOfTopPool<GameObject>(guid))
                {
                    int16& eventRef = pool2event[topPoolId];
                    if (eventRef != 0)
                    {
                        if (eventRef != event_id)
                        {
                            sLog.outErrorDb("`game_event_gameobject` have gameobject (GUID: %u) for event %i from pool or subpool of pool (ID: %u) but pool have already content from event %i. Pool don't must have content for different events!", guid, event_id, topPoolId, eventRef);
                        }
                    }
                    else
                    {
                        eventRef = event_id;
                        mGameEventSpawnPoolIds[event_id].push_back(topPoolId);
                        sPoolMgr.RemoveAutoSpawnForPool(topPoolId);
                    }

                    continue;
                }
            }

            GuidList& golist = mGameEventGameobjectGuids[internal_event_id];
            golist.push_back(guid);
        }

        sLog.outString();
        sLog.outString(">> Loaded %u gameobjects in game events", count);
    }

    // now recheck that all eventPools linked with events after our skip pools with parents
    for (std::map<uint16, int16>::const_iterator itr = pool2event.begin(); itr != pool2event.end();  ++itr)
    {
        uint16 pool_id = itr->first;
        int16 event_id = itr->second;

        sPoolMgr.CheckEventLinkAndReport(pool_id, event_id, creature2event, go2event);
    }

    mGameEventCreatureData.resize(mGameEvent.size());
    std::vector<GameEventCreatureDataRow> eventCreatureData = GameEventDataRepository::QueryEventCreatureData();

    count = 0;
    if (eventCreatureData.empty())
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded %u creature reactions at game events", count);
    }
    else
    {
        BarGoLink bar(eventCreatureData.size());
        for (GameEventCreatureDataRow const& row : eventCreatureData)
        {
            bar.step();
            uint32 guid     = row.guid;
            uint16 event_id = row.event;

            if (event_id == 0)
            {
                sLog.outErrorDb("`game_event_creature_data` game event id (%i) is reserved and can't be used." , event_id);
                continue;
            }

            if (!IsValidEvent(event_id))
            {
                sLog.outErrorDb("`game_event_creature_data` game event id (%u) not exist in `game_event`", event_id);
                continue;
            }

            ++count;
            GameEventCreatureDataList& equiplist = mGameEventCreatureData[event_id];
            GameEventCreatureData newData;
            newData.modelid = row.modelid;
            newData.equipment_id = row.equipmentId;
            newData.entry_id = row.entryId;
            newData.spell_id_start = row.spellStart;
            newData.spell_id_end = row.spellEnd;

            if (newData.equipment_id && !sObjectMgr.GetEquipmentInfo(newData.equipment_id))
            {
                sLog.outErrorDb("Table `game_event_creature_data` have creature (Guid: %u) with equipment_id %u not found in table `creature_equip_template`, set to no equipment.", guid, newData.equipment_id);
                newData.equipment_id = 0;
            }

            if (newData.entry_id && !ObjectMgr::GetCreatureTemplate(newData.entry_id))
            {
                sLog.outErrorDb("Table `game_event_creature_data` have creature (Guid: %u) with event time entry %u not found in table `creature_template`, set to no 0.", guid, newData.entry_id);
                newData.entry_id = 0;
            }

            if (newData.spell_id_start && !sSpellStore.LookupEntry(newData.spell_id_start))
            {
                sLog.outErrorDb("Table `game_event_creature_data` have creature (Guid: %u) with nonexistent spell_start %u, set to no start spell.", guid, newData.spell_id_start);
                newData.spell_id_start = 0;
            }

            if (newData.spell_id_end && !sSpellStore.LookupEntry(newData.spell_id_end))
            {
                sLog.outErrorDb("Table `game_event_creature_data` have creature (Guid: %u) with nonexistent spell_end %u, set to no end spell.", guid, newData.spell_id_end);
                newData.spell_id_end = 0;
            }

            equiplist.push_back(GameEventCreatureDataPair(guid, newData));
            mGameEventCreatureDataPerGuid.insert(GameEventCreatureDataPerGuidMap::value_type(guid, event_id));
        }

        sLog.outString();
        sLog.outString(">> Loaded %u creature reactions at game events", count);
    }

    mGameEventQuests.resize(mGameEvent.size());

    std::vector<GameEventQuestRow> eventQuests = GameEventDataRepository::QueryEventQuests();

    count = 0;
    if (eventQuests.empty())
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded %u quests additions in game events", count);
    }
    else
    {
        BarGoLink bar(eventQuests.size());
        for (GameEventQuestRow const& row : eventQuests)
        {
            bar.step();
            uint32 quest    = row.quest;
            uint16 event_id = row.event;

            if (event_id == 0)
            {
                sLog.outErrorDb("`game_event_quest` game event id (%i) is reserved and can't be used.", event_id);
                continue;
            }

            if (!IsValidEvent(event_id))
            {
                sLog.outErrorDb("`game_event_quest` game event id (%u) not exist in `game_event`", event_id);
                continue;
            }

            const Quest* pQuest = sObjectMgr.GetQuestTemplate(quest);

            if (!pQuest)
            {
                sLog.outErrorDb("Table `game_event_quest` contain entry for quest %u (event %u) but this quest does not exist. Skipping.", quest, event_id);
                continue;
            }

            // disable any event specific quest (for cases where creature is spawned, but event not active).
            const_cast<Quest*>(pQuest)->SetQuestActiveState(false);

            ++count;

            QuestList& questlist = mGameEventQuests[event_id];
            questlist.push_back(quest);
        }

        sLog.outString();
        sLog.outString(">> Loaded %u quest additions in game events", count);
    }

    mGameEventMails.resize(mGameEvent.size() * 2 - 1);

    std::vector<GameEventMailRow> eventMails = GameEventDataRepository::QueryEventMails();

    count = 0;
    if (eventMails.empty())
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded %u start/end game event mails", count);
    }
    else
    {
        BarGoLink bar(eventMails.size());
        for (GameEventMailRow const& row : eventMails)
        {
            bar.step();
            uint16 event_id = row.event;

            GameEventMail mail;
            mail.raceMask       = row.raceMask;
            mail.questId        = row.quest;
            mail.mailTemplateId = row.mailTemplateId;
            mail.senderEntry    = row.senderEntry;

            if (event_id == 0)
            {
                sLog.outErrorDb("`game_event_mail` game event id (%i) not allowed", event_id);
                continue;
            }

            if (!IsValidEvent(event_id))
            {
                sLog.outErrorDb("`game_event_mail` game event id (%u) not exist in `game_event`", event_id);
                continue;
            }

            int32 internal_event_id = mGameEvent.size() + event_id - 1;

            if (!(mail.raceMask & RACEMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Table `game_event_mail` have raceMask (%u) requirement for game event %i that not include any player races, ignoring.", mail.raceMask, event_id);
                continue;
            }

            if (mail.questId && !sObjectMgr.GetQuestTemplate(mail.questId))
            {
                sLog.outErrorDb("Table `game_event_mail` have nonexistent quest (%u) requirement for game event %i, ignoring.", mail.questId, event_id);
                continue;
            }

            if (!sMailTemplateStore.LookupEntry(mail.mailTemplateId))
            {
                sLog.outErrorDb("Table `game_event_mail` have invalid mailTemplateId (%u) for game event %i that invalid not include any player races, ignoring.", mail.mailTemplateId, event_id);
                continue;
            }

            if (!ObjectMgr::GetCreatureTemplate(mail.senderEntry))
            {
                sLog.outErrorDb("Table `game_event_mail` have nonexistent sender creature entry (%u) for game event %i that invalid not include any player races, ignoring.", mail.senderEntry, event_id);
                continue;
            }

            ++count;

            MailList& maillist = mGameEventMails[internal_event_id];
            maillist.push_back(mail);
        }

        sLog.outString();
        sLog.outString(">> Loaded %u start/end game event mails", count);
    }
}

/**
 * @brief Gets active event-specific update data for a creature guid.
 *
 * @param lowguid The creature low guid.
 * @return GameEventCreatureData const* The active event update data, or null if none applies.
 */
GameEventCreatureData const* GameEventMgr::GetCreatureUpdateDataForActiveEvent(uint32 lowguid) const
{
    // only for active event, creature can be listed for many so search all
    uint32 event_id = 0;
    GameEventCreatureDataPerGuidBounds bounds = mGameEventCreatureDataPerGuid.equal_range(lowguid);
    for (GameEventCreatureDataPerGuidMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (IsActiveEvent(itr->second))
        {
            event_id = itr->second;
            break;
        }
    }

    if (!event_id)
    {
        return NULL;
    }

    for (GameEventCreatureDataList::const_iterator itr = mGameEventCreatureData[event_id].begin(); itr != mGameEventCreatureData[event_id].end(); ++itr)
        if (itr->first == lowguid)
        {
            return &itr->second;
        }

    return NULL;
}

// Get the Game Event ID for Creature by guid
template <>
/**
 * @brief Resolves the signed game event id associated with a creature guid.
 *
 * @param guid_or_poolid The creature guid.
 * @return int16 The signed event id, or zero if none applies.
 */
int16 GameEventMgr::GetGameEventId<Creature>(uint32 guid_or_poolid)
{
    for (uint16 i = 0; i < mGameEventCreatureGuids.size(); ++i) // 0 <= i <= 2*(S := mGameEvent.size()) - 2
        for (GuidList::const_iterator itr = mGameEventCreatureGuids[i].begin(); itr != mGameEventCreatureGuids[i].end(); ++itr)
            if (*itr == guid_or_poolid)
            {
                return i + 1 - mGameEvent.size();        // -S *1 + 1 <= . <= 1*S - 1
            }
    return 0;
}

// Get the Game Event ID for GameObject by guid
template <>
/**
 * @brief Resolves the signed game event id associated with a gameobject guid.
 *
 * @param guid_or_poolid The gameobject guid.
 * @return int16 The signed event id, or zero if none applies.
 */
int16 GameEventMgr::GetGameEventId<GameObject>(uint32 guid_or_poolid)
{
    for (uint16 i = 0; i < mGameEventGameobjectGuids.size(); ++i)
        for (GuidList::const_iterator itr = mGameEventGameobjectGuids[i].begin(); itr != mGameEventGameobjectGuids[i].end(); ++itr)
            if (*itr == guid_or_poolid)
            {
                return i + 1 - mGameEvent.size();        // -S *1 + 1 <= . <= 1*S - 1
            }
    return 0;
}

// Get the Game Event ID for Pool by pool ID
template <>
/**
 * @brief Resolves the game event id associated with a spawn pool id.
 *
 * @param guid_or_poolid The pool id.
 * @return int16 The event id, or zero if none applies.
 */
int16 GameEventMgr::GetGameEventId<Pool>(uint32 guid_or_poolid)
{
    for (uint16 i = 0; i < mGameEventSpawnPoolIds.size(); ++i)
        for (IdList::const_iterator itr = mGameEventSpawnPoolIds[i].begin(); itr != mGameEventSpawnPoolIds[i].end(); ++itr)
            if (*itr == guid_or_poolid)
            {
                return i;
            }
    return 0;
}

GameEventMgr::GameEventMgr()
{
}

/**
 * @brief Checks whether any active event matches the specified holiday id.
 *
 * @param id The holiday id to search for.
 * @return true if the holiday is currently active; otherwise false.
 */
bool GameEventMgr::IsActiveHoliday(HolidayIds id)
{
    if (id == HOLIDAY_NONE)
    {
        return false;
    }

    for (GameEventMgr::ActiveEvents::const_iterator itr = m_ActiveEvents.begin(); itr != m_ActiveEvents.end(); ++itr)
        if (mGameEvent[*itr].holiday_id == id)
        {
            return true;
        }

    return false;
}

 bool IsHolidayActive(HolidayIds id)
{
    return sGameEventMgr.IsActiveHoliday(id);
}
