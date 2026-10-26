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
 * @file ObjectMgrDungeonFinder.cpp
 * @brief ObjectMgr's dungeon finder loaders: requirements, rewards, items and dungeon entrances.
 */

#include "Utilities/PackedValues.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"

#include "SQLStorages.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "ProgressBar.h"
#include "Language.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "Util.h"
#include "WaypointManager.h"
#include "DB2Structure.h"
#include "DB2Stores.h"
#include "GridNotifiersImpl.h"
#include "DisableMgr.h"

#include <limits>
#include "persistence/world/maps/DungeonDataRepository.h"

void ObjectMgr::LoadDungeonFinderRequirements()
{
    uint32 count = 0;
    m_dungeonFinder.ClearRequirements();      // in case of a reload

    QueryResult* result = DungeonDataRepository::QueryDungeonFinderRequirements();

    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outErrorDb(">> Loaded 0 dungeon finder requirements. DB table `dungeonfinder_requirements`, is empty!");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 mapId          = fields[0].GetUInt32();
        uint32 difficulty     = fields[1].GetUInt32();
        uint32 dungeonKey     = MAKE_PAIR32(mapId, difficulty); // for unique key

        uint32 minItemLevel   = fields[2].GetUInt32();
        uint32 item           = fields[3].GetUInt32();
        uint32 item2          = fields[4].GetUInt32();
        uint32 allianceQuest  = fields[5].GetUInt32();
        uint32 hordeQuest     = fields[6].GetUInt32();
        uint32 achievement    = fields[7].GetUInt32();
        const char* questText = fields[8].GetString();

        // check that items, quests, & achievements are real
        if (item)
        {
            ItemEntry const* dbcitem = sItemStore.LookupEntry(item);
            if (!dbcitem)
            {
                sLog.outString();
                sLog.outErrorDb("Table `dungeonfinder_requirements` has invalid item entry %u for map %u ! Removing requirement.", item, mapId);
                item = 0;
            }
        }

        if (item2)
        {
            ItemEntry const* dbcitem = sItemStore.LookupEntry(item2);
            if (!dbcitem)
            {
                sLog.outString();
                sLog.outErrorDb("Table `dungeonfinder_requirements` has invalid item entry %u for map %u ! Removing requirement.", item2, mapId);
                item2 = 0;
            }
        }

        if (allianceQuest)
        {
            QuestMap::iterator qReqItr = mQuestTemplates.find(allianceQuest);
            if (qReqItr == mQuestTemplates.end())
            {
                sLog.outString();
                sLog.outErrorDb("Table `dungeonfinder_requirements` has invalid quest requirement %u for map %u ! Removing requirement.", allianceQuest, mapId);
                allianceQuest = 0;
            }
        }

        if (hordeQuest)
        {
            QuestMap::iterator qReqItr = mQuestTemplates.find(hordeQuest);
            if (qReqItr == mQuestTemplates.end())
            {
                sLog.outString();
                sLog.outErrorDb("Table `dungeonfinder_requirements` has invalid quest requirement %u for map %u ! Removing requirement.", hordeQuest, mapId);
                hordeQuest = 0;
            }
        }

        if (achievement)
        {
            if (!sAchievementStore.LookupEntry(achievement))
            {
                sLog.outString();
                sLog.outErrorDb("Table `dungeonfinder_requirements` has invalid achievement %u for map %u ! Removing requirement.", achievement, mapId);
                achievement = 0;
            }
        }

        // add to map after checks
        DungeonFinderRequirements requirement(minItemLevel, item, item2, allianceQuest, hordeQuest, achievement, questText);
        m_dungeonFinder.SetRequirements(dungeonKey, requirement);

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString(">> Loaded %u Dungeon Finder Requirements", count);
}

void ObjectMgr::LoadDungeonFinderRewards()
{
    uint32 count = 0;
    m_dungeonFinder.ClearRewards();      // in case of a reload

    QueryResult* result = DungeonDataRepository::QueryDungeonFinderRewards();

    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outErrorDb(">> Loaded 0 dungeon finder rewards. DB table `dungeonfinder_rewards`, is empty!");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 level     = fields[1].GetUInt32();
        uint32 baseXP    = fields[2].GetUInt32();
        int32 baseMoney  = fields[3].GetInt32();

        DungeonFinderRewards reward(baseXP, baseMoney);
        m_dungeonFinder.SetRewards(level, reward);

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString(">> Loaded %u Dungeon Finder Rewards", count);
}

void ObjectMgr::LoadDungeonFinderItems()
{
    uint32 count = 0;
    m_dungeonFinder.ClearItems();   // in case of reload

    QueryResult* result = DungeonDataRepository::QueryDungeonFinderItemRewards();

    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outErrorDb(">> Loaded 0 dungeon finder items. DB table `dungeonfinder_item_rewards`, is empty!");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 id          = fields[0].GetUInt32();
        uint32 minLevel    = fields[1].GetUInt32();
        uint32 maxLevel    = fields[2].GetUInt32();
        uint32 itemReward  = fields[3].GetInt32();
        uint32 itemAmount  = fields[4].GetUInt32();
        uint32 dungeonType = fields[5].GetUInt32();

        DungeonFinderItems rewardItems(minLevel, maxLevel, itemReward, itemAmount, dungeonType);
        m_dungeonFinder.SetItems(id, rewardItems);

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString(">> Loaded %u Dungeon Finder Items", count);
}

void ObjectMgr::LoadLfgDungeonEntrances()
{
    uint32 count = 0;
    m_lfgDungeonEntrances.Clear();     // in case of a reload

    QueryResult* result = DungeonDataRepository::QueryDungeonEntrances();

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString();
        sLog.outString(">> Loaded 0 LFG dungeon entrance overrides. DB table `lfg_dungeon_entrances` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 dungeonId = fields[0].GetUInt32();
        if (!sLfgDungeonsStore.LookupEntry(dungeonId))
        {
            sLog.outString();
            sLog.outErrorDb("Table `lfg_dungeon_entrances` has a row for nonexistent dungeon %u, skipped.", dungeonId);
            continue;
        }

        LfgDungeonEntrance entrance;
        entrance.x = fields[1].GetFloat();
        entrance.y = fields[2].GetFloat();
        entrance.z = fields[3].GetFloat();
        entrance.o = fields[4].GetFloat();
        m_lfgDungeonEntrances.Set(dungeonId, entrance);

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString(">> Loaded %u LFG dungeon entrance overrides", count);
}
