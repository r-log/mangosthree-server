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
 * @file GameEventScheduler.cpp
 * @brief The game event system's runtime
 *
 * GameEventScheduler starts and stops the game events on their windows: it spawns and despawns
 * their creatures, game objects and pools, swaps creature models and equipment, toggles their
 * quests and world states, and sends their mails. It reads the events GameEventMgr loaded.
 */

#include <sstream>
#include "GameEventScheduler.h"
#include "BattleGround/BattleGroundMgr.h"
#include "Creature.h"
#include "DBCStores.h"
#include "GameObject.h"
#include "data/Language.h"
#include "Log.h"
#include "MapManager.h"
#include "MassMailMgr.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "PoolManager.h"
#include "PoolSpawner.h"
#include "data/GameConfig.h"
#include "data/WorldBroadcast.h"
#include "persistence/characters/events/GameEventStatusRepository.h"

/**
 * @brief Activates a game event and optionally rewrites its timing window.
 *
 * @param event_id The event id to start.
 * @param overwrite True to overwrite the stored timing window.
 * @param resume True if the event is being resumed after shutdown.
 */
void GameEventScheduler::StartEvent(uint16 event_id, bool overwrite /*=false*/, bool resume /*=false*/)
{
    ApplyNewEvent(event_id, resume);
    if (overwrite)
    {
        sGameEventMgr.StartWindowNow(event_id);
    }
}

/**
 * @brief Deactivates a game event and optionally rewrites its timing window.
 *
 * @param event_id The event id to stop.
 * @param overwrite True to overwrite the stored timing window.
 */
void GameEventScheduler::StopEvent(uint16 event_id, bool overwrite)
{
    UnApplyEvent(event_id);
    if (overwrite)
    {
        sGameEventMgr.EndWindowNow(event_id);
    }
}

uint32 GameEventScheduler::Initialize()                           // return the next event delay in ms
{
    sGameEventMgr.ClearActiveEvents();

    GameEventMgr::ActiveEvents activeAtShutdown;

    std::vector<uint16> activeEvents = GameEventStatusRepository::QueryActiveEvents();
    if (!activeEvents.empty())
    {
        for (uint16 event_id : activeEvents)
        {
            activeAtShutdown.insert(event_id);
        }

        GameEventStatusRepository::Truncate();
    }

    uint32 delay = Update(&activeAtShutdown);
    BASIC_LOG("Game Event system initialized.");
    m_IsGameEventsInit = true;
    return delay;
}

// return the next event delay in ms
uint32 GameEventScheduler::Update(GameEventMgr::ActiveEvents const* activeAtShutdown /*= NULL*/)
{
    time_t currenttime = time(NULL);

    uint32 nextEventDelay = max_ge_check_delay;             // 1 day
    uint32 calcDelay;
    for (uint16 itr = 1; itr < sGameEventMgr.GetEventMap().size(); ++itr)
    {
        if (!sGameEventMgr.GetEventMap()[itr].isValid())
        {
            continue;
        }

        // sLog.outErrorDb("Checking event %u",itr);
        if (sGameEventMgr.CheckOneGameEvent(itr, currenttime))
        {
            // DEBUG_LOG("GameEvent %u is active",itr->first);
            if (!sGameEventMgr.IsActiveEvent(itr))
            {
                bool resume = activeAtShutdown && (activeAtShutdown->find(itr) != activeAtShutdown->end());
                StartEvent(itr, false, resume);
            }
        }
        else
        {
            // DEBUG_LOG("GameEvent %u is not active",itr->first);
            if (sGameEventMgr.IsActiveEvent(itr))
            {
                StopEvent(itr);
            }
            else
            {
                if (!m_IsGameEventsInit)
                {
                    int16 event_nid = (-1) * (itr);
                    // spawn all negative ones for this event
                    GameEventSpawn(event_nid);
                    UpdateWorldStates(itr, false);
                }
            }
        }
        calcDelay = sGameEventMgr.NextCheck(itr);
        if (calcDelay < nextEventDelay)
        {
            nextEventDelay = calcDelay;
        }
    }
    BASIC_LOG("Next game event check in %u seconds.", nextEventDelay + 1);
    return (nextEventDelay + 1) * IN_MILLISECONDS;          // Add 1 second to be sure event has started/stopped at next call
}

/**
 * @brief Removes an active event's gameplay effects from the world.
 *
 * @param event_id The event id to remove.
 */
void GameEventScheduler::UnApplyEvent(uint16 event_id)
{
    sGameEventMgr.RemoveActiveEvent(event_id);
    GameEventStatusRepository::DeleteEvent(event_id);

    sLog.outString("GameEvent %u \"%s\" removed.", event_id, sGameEventMgr.GetEventMap()[event_id].description.c_str());
    // un-spawn positive event tagged objects
    GameEventUnspawn(event_id);
    // spawn negative event tagget objects
    int16 event_nid = (-1) * event_id;
    GameEventSpawn(event_nid);
    // restore equipment or model
    UpdateCreatureData(event_id, false);
    // Remove quests that are events only to non event npc
    UpdateEventQuests(event_id, false);
    UpdateWorldStates(event_id, false);
    SendEventMails(event_nid);
}

/**
 * @brief Applies a newly active event's gameplay effects to the world.
 *
 * @param event_id The event id to apply.
 * @param resume True if the event is resuming after shutdown.
 */
void GameEventScheduler::ApplyNewEvent(uint16 event_id, bool resume)
{
    sGameEventMgr.AddActiveEvent(event_id);
    GameEventStatusRepository::InsertEvent(event_id);

    if (sGameConfig.getConfig(CONFIG_BOOL_EVENT_ANNOUNCE))
    {
        sWorldBroadcast.WorldText(LANG_EVENTMESSAGE, sGameEventMgr.GetEventMap()[event_id].description.c_str());
    }

    sLog.outString("GameEvent %u \"%s\" started.", event_id, sGameEventMgr.GetEventMap()[event_id].description.c_str());
    // spawn positive event tagget objects
    GameEventSpawn(event_id);
    // un-spawn negative event tagged objects
    int16 event_nid = (-1) * event_id;
    GameEventUnspawn(event_nid);
    // Change equipement or model
    UpdateCreatureData(event_id, true);
    // Add quests that are events only to non event npc
    UpdateEventQuests(event_id, true);
    UpdateWorldStates(event_id, true);

    // Not send mails at game event startup, if game event just resume after server shutdown (has been active at server before shutdown)
    if (!resume)
    {
        SendEventMails(event_id);
    }
}

/**
 * @brief Spawns creatures, gameobjects, and pools associated with an event state.
 *
 * @param event_id The signed event id to spawn for.
 */
void GameEventScheduler::GameEventSpawn(int16 event_id)
{
    int32 internal_event_id = sGameEventMgr.GetEventMap().size() + event_id - 1;

    if (internal_event_id < 0 || (size_t)internal_event_id >= sGameEventMgr.GetEventCreatureGuids().size())
    {
        sLog.outError("GameEventMgr::GameEventSpawn attempt access to out of range mGameEventCreatureGuids element %i (size: %zu)", internal_event_id, sGameEventMgr.GetEventCreatureGuids().size());
        return;
    }

    for (GameEventMgr::GuidList::const_iterator itr = sGameEventMgr.GetEventCreatureGuids()[internal_event_id].begin(); itr != sGameEventMgr.GetEventCreatureGuids()[internal_event_id].end(); ++itr)
    {
        // Add to correct cell
        CreatureData const* data = sObjectMgr.GetCreatureData(*itr);
        if (data)
        {
            // negative event id for pool element meaning allow be used in next pool spawn
            if (event_id < 0)
            {
                if (uint16 pool_id = sPoolMgr.IsPartOfAPool<Creature>(*itr))
                {
                    // will have chance at next pool update
                    sPoolMgr.SetExcludeObject<Creature>(pool_id, *itr, false);
                    sPoolSpawner.UpdatePoolInMaps<Creature>(pool_id);
                    continue;
                }
            }

            sObjectMgr.AddCreatureToGrid(*itr, data);

            Creature::SpawnInMaps(*itr, data);
        }
    }

    if (internal_event_id < 0 || (size_t)internal_event_id >= sGameEventMgr.GetEventGameobjectGuids().size())
    {
        sLog.outError("GameEventMgr::GameEventSpawn attempt access to out of range mGameEventGameobjectGuids element %i (size: %zu)", internal_event_id, sGameEventMgr.GetEventGameobjectGuids().size());
        return;
    }

    for (GameEventMgr::GuidList::const_iterator itr = sGameEventMgr.GetEventGameobjectGuids()[internal_event_id].begin(); itr != sGameEventMgr.GetEventGameobjectGuids()[internal_event_id].end(); ++itr)
    {
        // Add to correct cell
        GameObjectData const* data = sObjectMgr.GetGOData(*itr);
        if (data)
        {
            // negative event id for pool element meaning allow be used in next pool spawn
            if (event_id < 0)
            {
                if (uint16 pool_id = sPoolMgr.IsPartOfAPool<GameObject>(*itr))
                {
                    // will have chance at next pool update
                    sPoolMgr.SetExcludeObject<GameObject>(pool_id, *itr, false);
                    sPoolSpawner.UpdatePoolInMaps<GameObject>(pool_id);
                    continue;
                }
            }

            sObjectMgr.AddGameobjectToGrid(*itr, data);

            GameObject::SpawnInMaps(*itr, data);
        }
    }

    if (event_id > 0)
    {
        if ((size_t)event_id >= sGameEventMgr.GetEventSpawnPoolIds().size())
        {
            sLog.outError("GameEventMgr::GameEventSpawn attempt access to out of range mGameEventSpawnPoolIds element %i (size: %zu)", event_id, sGameEventMgr.GetEventSpawnPoolIds().size());
            return;
        }

        for (GameEventMgr::IdList::const_iterator itr = sGameEventMgr.GetEventSpawnPoolIds()[event_id].begin(); itr != sGameEventMgr.GetEventSpawnPoolIds()[event_id].end(); ++itr)
        {
            sPoolSpawner.SpawnPoolInMaps(*itr, true);
        }
    }
}

/**
 * @brief Unspawns creatures, gameobjects, and pools associated with an event state.
 *
 * @param event_id The signed event id to unspawn for.
 */
void GameEventScheduler::GameEventUnspawn(int16 event_id)
{
    int32 internal_event_id = sGameEventMgr.GetEventMap().size() + event_id - 1;

    if (internal_event_id < 0 || (size_t)internal_event_id >= sGameEventMgr.GetEventCreatureGuids().size())
    {
        sLog.outError("GameEventMgr::GameEventUnspawn attempt access to out of range mGameEventCreatureGuids element %i (size: %zu)", internal_event_id, sGameEventMgr.GetEventCreatureGuids().size());
        return;
    }

    for (GameEventMgr::GuidList::const_iterator itr = sGameEventMgr.GetEventCreatureGuids()[internal_event_id].begin(); itr != sGameEventMgr.GetEventCreatureGuids()[internal_event_id].end(); ++itr)
    {
        // Remove the creature from grid
        if (CreatureData const* data = sObjectMgr.GetCreatureData(*itr))
        {
            // negative event id for pool element meaning unspawn in pool and exclude for next spawns
            if (event_id < 0)
            {
                if (uint16 poolid = sPoolMgr.IsPartOfAPool<Creature>(*itr))
                {
                    sPoolMgr.SetExcludeObject<Creature>(poolid, *itr, true);
                    sPoolSpawner.UpdatePoolInMaps<Creature>(poolid, *itr);
                    continue;
                }
            }

            // Remove spawn data
            sObjectMgr.RemoveCreatureFromGrid(*itr, data);

            // Remove spawned cases
            Creature::AddToRemoveListInMaps(*itr, data);
        }
    }

    if (internal_event_id < 0 || (size_t)internal_event_id >= sGameEventMgr.GetEventGameobjectGuids().size())
    {
        sLog.outError("GameEventMgr::GameEventUnspawn attempt access to out of range mGameEventGameobjectGuids element %i (size: %zu)", internal_event_id, sGameEventMgr.GetEventGameobjectGuids().size());
        return;
    }

    for (GameEventMgr::GuidList::const_iterator itr = sGameEventMgr.GetEventGameobjectGuids()[internal_event_id].begin(); itr != sGameEventMgr.GetEventGameobjectGuids()[internal_event_id].end(); ++itr)
    {
        // Remove the gameobject from grid
        if (GameObjectData const* data = sObjectMgr.GetGOData(*itr))
        {
            // negative event id for pool element meaning unspawn in pool and exclude for next spawns
            if (event_id < 0)
            {
                if (uint16 poolid = sPoolMgr.IsPartOfAPool<GameObject>(*itr))
                {
                    sPoolMgr.SetExcludeObject<GameObject>(poolid, *itr, true);
                    sPoolSpawner.UpdatePoolInMaps<GameObject>(poolid, *itr);
                    continue;
                }
            }

            // Remove spawn data
            sObjectMgr.RemoveGameobjectFromGrid(*itr, data);

            // Remove spawned cases
            GameObject::AddToRemoveListInMaps(*itr, data);
        }
    }

    if (event_id > 0)
    {
        if ((size_t)event_id >= sGameEventMgr.GetEventSpawnPoolIds().size())
        {
            sLog.outError("GameEventMgr::GameEventUnspawn attempt access to out of range mGameEventSpawnPoolIds element %i (size: %zu)", event_id, sGameEventMgr.GetEventSpawnPoolIds().size());
            return;
        }

        for (GameEventMgr::IdList::const_iterator itr = sGameEventMgr.GetEventSpawnPoolIds()[event_id].begin(); itr != sGameEventMgr.GetEventSpawnPoolIds()[event_id].end(); ++itr)
        {
            sPoolSpawner.DespawnPoolInMaps(*itr);
        }
    }
}

struct GameEventUpdateCreatureDataInMapsWorker
{
    GameEventUpdateCreatureDataInMapsWorker(ObjectGuid guid, CreatureData const* data, GameEventCreatureData const* event_data, bool activate)
        : i_guid(guid), i_data(data), i_event_data(event_data), i_activate(activate) {}

    void operator()(Map* map)
    {
        if (Creature* pCreature = map->GetCreature(i_guid))
        {
            pCreature->UpdateEntry(i_data->id, TEAM_NONE, i_data, i_activate ? i_event_data : NULL);

            // spells not casted for event remove case (sent NULL into update), do it
            if (!i_activate)
            {
                pCreature->ApplyGameEventSpells(i_event_data, false);
            }
        }
    }

    ObjectGuid i_guid;
    CreatureData const* i_data;
    GameEventCreatureData const* i_event_data;
    bool i_activate;
};

/**
 * @brief Updates spawned creatures with event-specific template data.
 *
 * @param event_id The event id whose creature data should be applied.
 * @param activate True to apply event data; false to remove it.
 */
void GameEventScheduler::UpdateCreatureData(int16 event_id, bool activate)
{
    for (GameEventMgr::GameEventCreatureDataList::const_iterator itr = sGameEventMgr.GetEventCreatureData()[event_id].begin(); itr != sGameEventMgr.GetEventCreatureData()[event_id].end(); ++itr)
    {
        // Remove the creature from grid
        CreatureData const* data = sObjectMgr.GetCreatureData(itr->first);
        if (!data)
        {
            continue;
        }

        // Update if spawned
        GameEventUpdateCreatureDataInMapsWorker worker(data->GetObjectGuid(itr->first), data, &itr->second, activate);
        sMapMgr.DoForAllMapsWithMapId(data->mapid, worker);
    }
}

/**
 * @brief Toggles event-specific quest availability.
 *
 * @param event_id The event id whose quests should be toggled.
 * @param Activate True to activate the quests; false to deactivate them.
 */
void GameEventScheduler::UpdateEventQuests(uint16 event_id, bool Activate)
{
    GameEventMgr::QuestList::const_iterator itr;
    for (itr = sGameEventMgr.GetEventQuests()[event_id].begin(); itr != sGameEventMgr.GetEventQuests()[event_id].end(); ++itr)
    {
        const Quest* pQuest = sObjectMgr.GetQuestTemplate(*itr);

        // if (Activate)
        //{
        // TODO: implement way to reset quests when event begin.
        //}

        const_cast<Quest*>(pQuest)->SetQuestActiveState(Activate);
    }
}

void GameEventScheduler::UpdateWorldStates(uint16 event_id, bool Activate)
{
    GameEventData const& event = sGameEventMgr.GetEventMap()[event_id];
    if (event.holiday_id != HOLIDAY_NONE)
    {
        BattleGroundTypeId bgTypeId = BattleGroundMgr::WeekendHolidayIdToBGType(event.holiday_id);
        if (bgTypeId != BATTLEGROUND_TYPE_NONE)
        {
            BattlemasterListEntry const* bl = sBattlemasterListStore.LookupEntry(bgTypeId);
            if (bl && bl->HolidayWorldState)
            {
                sWorldBroadcast.WorldStateChanged(bl->HolidayWorldState, Activate ? 1 : 0);
            }
        }
    }
}

/**
 * @brief Queues event-related mass mails for eligible characters.
 *
 * @param event_id The signed event id whose mails should be sent.
 */
void GameEventScheduler::SendEventMails(int16 event_id)
{
    int32 internal_event_id = sGameEventMgr.GetEventMap().size() + event_id - 1;

    GameEventMgr::MailList const& mails = sGameEventMgr.GetEventMails()[internal_event_id];

    for (GameEventMgr::MailList::const_iterator itr = mails.begin(); itr != mails.end(); ++itr)
    {
        if (itr->questId)
        {
            // need special query
            std::ostringstream ss;
            ss << "SELECT `characters`.`guid` FROM `characters`, `character_queststatus` "
               "WHERE (1 << (`characters`.`race` - 1)) & "
               << itr->raceMask
               << " AND `characters`.`deleteDate` IS NULL AND `character_queststatus`.`guid` = `characters`.`guid` AND `character_queststatus`.`quest` = "
               << itr->questId
               << " AND `character_queststatus`.`rewarded` <> 0";
            sMassMailMgr.AddMassMailTask(new MailDraft(itr->mailTemplateId), MailSender(MAIL_CREATURE, itr->senderEntry), ss.str().c_str());
        }
        else
        {
            sMassMailMgr.AddMassMailTask(new MailDraft(itr->mailTemplateId), MailSender(MAIL_CREATURE, itr->senderEntry), itr->raceMask);
        }
    }
}
