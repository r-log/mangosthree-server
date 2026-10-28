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

#include "Platform/Define.h"
#include "SharedDefines.h"
#include "BattleGroundMgr.h"
#include "BattleGroundAV.h"
#include "BattleGroundAB.h"
#include "BattleGroundEY.h"
#include "BattleGroundWS.h"
#include "BattleGroundNA.h"
#include "BattleGroundBE.h"
#include "BattleGroundAA.h"
#include "BattleGroundRL.h"
#include "BattleGroundSA.h"
#include "BattleGroundDS.h"
#include "BattleGroundRV.h"
#include "BattleGroundIC.h"
#include "BattleGroundRB.h"
#include "ObjectMgr.h"
#include "DBCStores.h"
#include "ProgressBar.h"
#include "data/GameConfig.h"
#include "GameEventMgr.h"
#include "DisableMgr.h"
#include "GameTime.h"
#include "persistence/world/pvp/BattleGroundDataRepository.h"

#include "Policies/Singleton.h"

/**
 * @file BattleGroundMgrUtil.cpp
 * @brief BattleGroundMgr's type and arena mappings, testing toggles, queue-update scheduling, rating and timer config
 * getters, weekend-holiday mapping and the battlemaster and event-index loaders.
 */

bool BattleGroundMgr::IsArenaType(BattleGroundTypeId bgTypeId)
{
    switch (bgTypeId)
    {
        case BATTLEGROUND_NA:
        case BATTLEGROUND_BE:
        case BATTLEGROUND_RL:
        case BATTLEGROUND_DS:
        case BATTLEGROUND_RV:
        case BATTLEGROUND_AA:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Converts a battleground type ID to a queue type ID.
 *
 * Maps a battleground type ID to its corresponding queue type ID. Different queue types
 * have separate queues in the matchmaking system.
 *
 * @param bgTypeId The battleground type ID.
 * @param arenaType
 * @return The corresponding queue type ID, or BATTLEGROUND_QUEUE_NONE if invalid.
 */
BattleGroundQueueTypeId BattleGroundMgr::BGQueueTypeId(BattleGroundTypeId bgTypeId, ArenaType arenaType)
{
    switch (bgTypeId)
    {
        case BATTLEGROUND_WS:
            return BATTLEGROUND_QUEUE_WS;
        case BATTLEGROUND_AB:
            return BATTLEGROUND_QUEUE_AB;
        case BATTLEGROUND_AV:
            return BATTLEGROUND_QUEUE_AV;
        case BATTLEGROUND_EY:
            return BATTLEGROUND_QUEUE_EY;
        case BATTLEGROUND_SA:
            return BATTLEGROUND_QUEUE_SA;
        case BATTLEGROUND_IC:
            return BATTLEGROUND_QUEUE_IC;
        case BATTLEGROUND_RB:
            return BATTLEGROUND_QUEUE_NONE;
        case BATTLEGROUND_TP:
            return BATTLEGROUND_QUEUE_TP;
        case BATTLEGROUND_BG:
            return BATTLEGROUND_QUEUE_BG;
        case BATTLEGROUND_AA:
        case BATTLEGROUND_NA:
        case BATTLEGROUND_RL:
        case BATTLEGROUND_BE:
        case BATTLEGROUND_DS:
        case BATTLEGROUND_RV:
            switch (arenaType)
            {
                case ARENA_TYPE_2v2:
                    return BATTLEGROUND_QUEUE_2v2;
                case ARENA_TYPE_3v3:
                    return BATTLEGROUND_QUEUE_3v3;
                case ARENA_TYPE_5v5:
                    return BATTLEGROUND_QUEUE_5v5;
                default:
                    return BATTLEGROUND_QUEUE_NONE;
            }
        default:
            return BATTLEGROUND_QUEUE_NONE;
    }
}

/**
 * @brief Converts a battleground queue type to its template battleground type.
 *
 * Maps queue identifiers back to the battleground template type used to create
 * or reference battleground instances.
 *
 * @param bgQueueTypeId The battleground queue type identifier.
 * @return The corresponding battleground type identifier.
 */
BattleGroundTypeId BattleGroundMgr::BGTemplateId(BattleGroundQueueTypeId bgQueueTypeId)
{
    switch (bgQueueTypeId)
    {
        case BATTLEGROUND_QUEUE_WS:
            return BATTLEGROUND_WS;
        case BATTLEGROUND_QUEUE_AB:
            return BATTLEGROUND_AB;
        case BATTLEGROUND_QUEUE_AV:
            return BATTLEGROUND_AV;
        case BATTLEGROUND_QUEUE_EY:
            return BATTLEGROUND_EY;
        case BATTLEGROUND_QUEUE_SA:
            return BATTLEGROUND_SA;
        case BATTLEGROUND_QUEUE_IC:
            return BATTLEGROUND_IC;
        case BATTLEGROUND_QUEUE_TP:
            return BATTLEGROUND_TP;
        case BATTLEGROUND_QUEUE_BG:
            return BATTLEGROUND_BG;
        case BATTLEGROUND_QUEUE_2v2:
        case BATTLEGROUND_QUEUE_3v3:
        case BATTLEGROUND_QUEUE_5v5:
            return BATTLEGROUND_AA;
        default:
            return BattleGroundTypeId(0);                   // used for unknown template (it exist and do nothing)
    }
}

ArenaType BattleGroundMgr::BGArenaType(BattleGroundQueueTypeId bgQueueTypeId)
{
    switch (bgQueueTypeId)
    {
        case BATTLEGROUND_QUEUE_2v2:
            return ARENA_TYPE_2v2;
        case BATTLEGROUND_QUEUE_3v3:
            return ARENA_TYPE_3v3;
        case BATTLEGROUND_QUEUE_5v5:
            return ARENA_TYPE_5v5;
        default:
            return ARENA_TYPE_NONE;
    }
}

/**
 * @brief Toggles battleground debug testing mode.
 *
 * Enables or disables testing mode and broadcasts the status change to the world.
 */
void BattleGroundMgr::ToggleTesting()
{
    m_Testing = !m_Testing;
    if (m_Testing)
    {
        SendWorldText(LANG_DEBUG_BG_ON);
    }
    else
    {
        SendWorldText(LANG_DEBUG_BG_OFF);
    }
}

void BattleGroundMgr::ToggleArenaTesting()
{
    m_ArenaTesting = !m_ArenaTesting;
    if (m_ArenaTesting)
    {
        SendWorldText(LANG_DEBUG_ARENA_ON);
    }
    else
    {
        SendWorldText(LANG_DEBUG_ARENA_OFF);
    }
}

/**
 * @brief Schedules a queue update for a specific battleground queue.
 *
 * Adds a queue update to the scheduler so that the next world update cycle will
 * process matchmaking and invitations for this queue. Multiple requests for the same
 * queue are consolidated to avoid duplicate processing.
 *
 * @param arenaRating
 * @param arenaType
 * @param bgQueueTypeId The battleground queue type to update.
 * @param bgTypeId The battleground type.
 * @param bracket_id The bracket to update.
 */
void BattleGroundMgr::ScheduleQueueUpdate(uint32 arenaRating, ArenaType arenaType, BattleGroundQueueTypeId bgQueueTypeId, BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id)
{
    // we will use only 1 number created of bgTypeId and bracket_id
    uint64 schedule_id = ((uint64)arenaRating << 32) | (arenaType << 24) | (bgQueueTypeId << 16) | (bgTypeId << 8) | bracket_id;
    bool found = false;
    for (uint8 i = 0; i < m_QueueUpdateScheduler.size(); ++i)
    {
        if (m_QueueUpdateScheduler[i] == schedule_id)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        m_QueueUpdateScheduler.push_back(schedule_id);
    }
}

uint32 BattleGroundMgr::GetMaxRatingDifference() const
{
    // this is for stupid people who can't use brain and set max rating difference to 0
    uint32 diff = sGameConfig.getConfig(CONFIG_UINT32_ARENA_MAX_RATING_DIFFERENCE);
    if (diff == 0)
    {
        diff = 5000;
    }
    return diff;
}

uint32 BattleGroundMgr::GetRatingDiscardTimer() const
{
    return sGameConfig.getConfig(CONFIG_UINT32_ARENA_RATING_DISCARD_TIMER);
}

/**
 * @brief Gets the premature finish timer duration.
 *
 * Returns the configured duration in milliseconds after which a battleground can be
 * finished prematurely if one team is significantly outnumbered or defeated.
 *
 * @return The premature finish timer duration in milliseconds.
 */
uint32 BattleGroundMgr::GetPrematureFinishTime() const
{
    return sGameConfig.getConfig(CONFIG_UINT32_BATTLEGROUND_PREMATURE_FINISH_TIMER);
}

/**
 * @brief Loads battle master creature entries from the database.
 *
 * Populates the battle master map from the `battlemaster_entry` database table,
 * which maps creature entries to their respective battleground types.
 */
void BattleGroundMgr::LoadBattleMastersEntry()
{
    mBattleMastersMap.clear();                              // need for reload case

    std::vector<BattleMasterRow> rows = BattleGroundDataRepository::QueryBattleMasters();

    uint32 count = 0;

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 battlemaster entries - table is empty!");
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());

    for (BattleMasterRow const& row : rows)
    {
        ++count;
        bar.step();

        uint32 entry = row.entry;
        uint32 bgTypeId  = row.bgTemplate;
        if (!sBattlemasterListStore.LookupEntry(bgTypeId))
        {
            sLog.outErrorDb("Table `battlemaster_entry` contain entry %u for nonexistent battleground type %u, ignored.", entry, bgTypeId);
            continue;
        }

        mBattleMastersMap[entry] = BattleGroundTypeId(bgTypeId);
    }

    sLog.outString(">> Loaded %u battlemaster entries", count);
    sLog.outString();
}

/**
 * @brief Converts a battleground type to its weekend holiday ID.
 *
 * Maps battleground types to their associated "Call to Arms" weekend holiday events that
 * provide bonus rewards for participating in that battleground type.
 *
 * @param bgTypeId The battleground type to convert.
 * @return The corresponding holiday ID, or HOLIDAY_NONE if not a recognized type.
 */
HolidayIds BattleGroundMgr::BGTypeToWeekendHolidayId(BattleGroundTypeId bgTypeId)
{
    switch (bgTypeId)
    {
        case BATTLEGROUND_AV: return HOLIDAY_CALL_TO_ARMS_AV;
        case BATTLEGROUND_EY: return HOLIDAY_CALL_TO_ARMS_EY;
        case BATTLEGROUND_WS: return HOLIDAY_CALL_TO_ARMS_WS;
        case BATTLEGROUND_SA: return HOLIDAY_CALL_TO_ARMS_SA;
        case BATTLEGROUND_IC: return HOLIDAY_CALL_TO_ARMS_IC;
        case BATTLEGROUND_TP: return HOLIDAY_CALL_TO_ARMS_TP;
        case BATTLEGROUND_BG: return HOLIDAY_CALL_TO_ARMS_BG;
        case BATTLEGROUND_AB: return HOLIDAY_CALL_TO_ARMS_AB;
        default: return HOLIDAY_NONE;
    }
}

/**
 * @brief Converts a battleground type to its weekend holiday ID.
 *
 * Maps battleground types to their associated "Call to Arms" weekend holiday events.
 *
 * @param holiday The holiday ID to convert.
 * @return The corresponding battleground type, or BATTLEGROUND_TYPE_NONE if invalid.
 */
BattleGroundTypeId BattleGroundMgr::WeekendHolidayIdToBGType(HolidayIds holiday)
{
    switch (holiday)
    {
        case HOLIDAY_CALL_TO_ARMS_AV: return BATTLEGROUND_AV;
        case HOLIDAY_CALL_TO_ARMS_EY: return BATTLEGROUND_EY;
        case HOLIDAY_CALL_TO_ARMS_WS: return BATTLEGROUND_WS;
        case HOLIDAY_CALL_TO_ARMS_SA: return BATTLEGROUND_SA;
        case HOLIDAY_CALL_TO_ARMS_IC: return BATTLEGROUND_IC;
        case HOLIDAY_CALL_TO_ARMS_TP: return BATTLEGROUND_TP;
        case HOLIDAY_CALL_TO_ARMS_BG: return BATTLEGROUND_BG;
        case HOLIDAY_CALL_TO_ARMS_AB: return BATTLEGROUND_AB;
        default: return BATTLEGROUND_TYPE_NONE;
    }
}

/**
 * @brief Checks if a battleground type is active for the weekend.
 *
 * Determines whether the specified battleground type has an active "Call to Arms"
 * weekend event that provides bonus experience and reputation.
 *
 * @param bgTypeId The battleground type to check.
 * @return true if the battleground is currently featured for the weekend, false otherwise.
 */
bool BattleGroundMgr::IsBGWeekend(BattleGroundTypeId bgTypeId)
{
    return sGameEventMgr.IsActiveHoliday(BGTypeToWeekendHolidayId(bgTypeId));
}

/**
 * @brief Loads battleground event indexes from the database.
 *
 * Populates the game object and creature event index maps from the database,
 * associating spawned objects and creatures with their battleground events.
 * This enables proper spawning and despawning of objectives during battles.
 */
void BattleGroundMgr::LoadBattleEventIndexes()
{
    BattleGroundEventIdx events;
    events.event1 = BG_EVENT_NONE;
    events.event2 = BG_EVENT_NONE;
    m_GameObjectBattleEventIndexMap.clear();             // need for reload case
    m_GameObjectBattleEventIndexMap[-1] = events;
    m_CreatureBattleEventIndexMap.clear();               // need for reload case
    m_CreatureBattleEventIndexMap[-1] = events;

    uint32 count = 0;

    std::vector<BattleGroundEventIndexRow> rows = BattleGroundDataRepository::QueryEventIndexes();
    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 battleground eventindexes.");
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());

    for (BattleGroundEventIndexRow const& row : rows)
    {
        bar.step();
        if (row.event1 == BG_EVENT_NONE || row.event2 == BG_EVENT_NONE)
        {
            continue; // we don't need to add those to the eventmap
        }

        bool gameobject         = (row.type == 1);
        uint32 dbTableGuidLow   = row.guid;
        events.event1           = row.event1;
        events.event2           = row.event2;
        uint32 map              = row.map;

        uint32 desc_map = row.descriptionMap;
        uint8 desc_event1 = row.descriptionEvent1;
        uint8 desc_event2 = row.descriptionEvent2;
        const char* description = (row.description ? row.description->c_str() : NULL);

        // checking for NULL - through right outer join this will mean following:
        if (row.spawnGuid != dbTableGuidLow)
        {
            sLog.outErrorDb("BattleGroundEvent: %s with nonexistent guid %u for event: map:%u, event1:%u, event2:%u (\"%s\")",
                            (gameobject) ? "gameobject" : "creature", dbTableGuidLow, map, events.event1, events.event2, description);
            continue;
        }

        // checking for NULL - through full outer join this can mean 2 things:
        if (desc_map != map)
        {
            // there is an event missing
            if (dbTableGuidLow == 0)
            {
                sLog.outErrorDb("BattleGroundEvent: missing db-data for map:%u, event1:%u, event2:%u (\"%s\")", desc_map, desc_event1, desc_event2, description);
                continue;
            }
            // we have an event which shouldn't exist
            else
            {
                sLog.outErrorDb("BattleGroundEvent: %s with guid %u is registered, for a nonexistent event: map:%u, event1:%u, event2:%u",
                                (gameobject) ? "gameobject" : "creature", dbTableGuidLow, map, events.event1, events.event2);
                continue;
            }
        }

        if (gameobject)
        {
            m_GameObjectBattleEventIndexMap[dbTableGuidLow] = events;
        }
        else
        {
            m_CreatureBattleEventIndexMap[dbTableGuidLow] = events;
        }

        ++count;
    }

    sLog.outString(">> Loaded %u battleground eventindexes", count);
    sLog.outString();
}
