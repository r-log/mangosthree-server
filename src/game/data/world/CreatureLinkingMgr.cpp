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
 * @file CreatureLinkingMgr.cpp
 * @brief NPC linking system for coordinated creature behavior
 *
 * This file implements the CreatureLinkingMgr which allows NPCs to be
 * linked together for coordinated behavior. Supported linking types:
 *
 * - Aggro linking: NPCs aggro together when one is attacked
 * - Death linking: NPCs die together when one dies
 * - Evade linking: NPCs evade together when one evades
 * - Respawn linking: NPCs respawn together
 * - Following: NPCs follow each other
 * - Boss dependency: NPCs depend on boss being alive/dead
 *
 * Linking rules are loaded from the `creature_linking` database table.
 *
 * @see CreatureLinkingMgr for the manager class
 */

#include "CreatureLinkingMgr.h"
#include "Policies/Singleton.h"
#include "ProgressBar.h"
#include "Database/DatabaseEnv.h"
#include "ObjectMgr.h"
#include "DBCStores.h"
#include "SharedDefines.h"
#include "persistence/world/creatures/CreatureDataRepository.h"
#include "persistence/world/spawns/SpawnRepository.h"


/* *********************************************************
 * Method to Load From DB
 * DB Format:   entry, map, master_entry, flag, search_radius   for `creature_linking_template` (by entry)
 *              0      1    2             3     4
 * DB Format:   guid, master_guid, flag                         for `creature_linking` (by guid)
 *              0     1            2
 * **************************************
 * entry/guid:                  creature_template.entry/guid
 * map:                         Map on which the NPC has to be
 * master_entry/master_guid     creature_template.entry of the npc, that shall trigger the actions
 * flag:                        flag value, of type CreatureLinkingFlags
 * search_radius:               radius, in which master and slave must be spawned so that they are linked together
 *
 * ***************************************************** */

void CreatureLinkingMgr::LoadFromDB()
{
    // Clear maps
    m_creatureLinkingMap.clear();
    m_creatureLinkingGuidMap.clear();
    m_eventTriggers.clear();                              // master
    m_eventGuidTriggers.clear();

    // Load `creature_linking_template`
    uint32 count = 0;
    std::vector<CreatureLinkingTemplateRow> linkingTemplates = CreatureDataRepository::QueryLinkingTemplates();
    if (linkingTemplates.empty())
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString(">> Table creature_linking_template is empty.");
        sLog.outString();
    }
    else
    {
        BarGoLink bar((int)linkingTemplates.size());
        for (CreatureLinkingTemplateRow const& row : linkingTemplates)
        {
            bar.step();

            CreatureLinkingInfo tmp;

            uint32 entry            = row.entry;
            tmp.mapId               = row.map;
            tmp.masterId            = row.masterEntry;
            tmp.linkingFlag         = row.flag;
            tmp.searchRange         = row.searchRange;
            tmp.masterDBGuid        = 0;                        // Will be initialized for unique mobs in IsLinkingEntryValid (only for spawning dependend)

            if (!IsLinkingEntryValid(entry, &tmp, true))
            {
                continue;
            }

            ++count;

            // Add it to the map
            m_creatureLinkingMap.insert(CreatureLinkingMap::value_type(entry, tmp));

            // Store master_entry
            m_eventTriggers.insert(tmp.masterId);
        }

        sLog.outString();
        sLog.outString(">> Loaded creature linking for %u creature-entries", count);
    }

    // Load `creature_linking`
    count = 0;
    std::vector<CreatureLinkingRow> linkings = CreatureDataRepository::QueryLinkings();
    if (linkings.empty())
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString(">> Table creature_linking is empty.");
        sLog.outString();

        return;
    }

    BarGoLink guidBar((int)linkings.size());
    for (CreatureLinkingRow const& row : linkings)
    {
        guidBar.step();

        CreatureLinkingInfo tmp;

        uint32 guid             = row.guid;
        tmp.mapId               = INVALID_MAP_ID;           // some invalid value, this marks the guid-linking
        tmp.masterId            = row.masterGuid;
        tmp.linkingFlag         = row.flag;
        tmp.masterDBGuid        = tmp.masterId;
        tmp.searchRange         = 0;

        if (!IsLinkingEntryValid(guid, &tmp, false))
        {
            continue;
        }

        ++count;

        // Add it to the map
        m_creatureLinkingGuidMap.insert(CreatureLinkingMap::value_type(guid, tmp));

        // Store master_guid
        m_eventGuidTriggers.insert(tmp.masterId);
    }

    sLog.outString();
    sLog.outString(">> Loaded creature linking for %u creature-Guids", count);
}

/** This function is used to check if a DB-Entry is valid
 *
 * @param byEntry: is the first parameter of the function a npc entry or a npc guid?
 * @param slaveEntry: dependend on byEntry param this is either the slave's npc-entry or the npc-guid
 * @param pTmp: Information about the CreatureLinking of the npc. Note that this information may be changed in some cases
 *
 * In case of checking by entry and in case of linked spawning and searchRange == 0, pTmp will be changed to keep information about the (unique!) master's db-guid
 */
bool CreatureLinkingMgr::IsLinkingEntryValid(uint32 slaveEntry, CreatureLinkingInfo* pTmp, bool byEntry)
{
    // Basic checks first
    if (byEntry)                                            // Entry given
    {
        CreatureInfo const* pInfo = ObjectMgr::GetCreatureTemplate(slaveEntry);
        CreatureInfo const* pMasterInfo = ObjectMgr::GetCreatureTemplate(pTmp->masterId);

        if (!pInfo)
        {
            sLog.outErrorDb("`creature_linking_template` has a non existing slave_entry (slave: %u, master %u), skipped.", slaveEntry, pTmp->masterId);
            return false;
        }
        if (!pMasterInfo)
        {
            sLog.outErrorDb("`creature_linking_template` has a non existing master_entry (slave: %u, master %u), skipped", slaveEntry, pTmp->masterId);
            return false;
        }
        if (pTmp->mapId && !sMapStore.LookupEntry(pTmp->mapId))
        {
            sLog.outErrorDb("`creature_linking_template` has a non existing map %u (slave %u, master %u), skipped", pTmp->mapId, slaveEntry, pTmp->masterId);
            return false;
        }
    }
    else                                                    // guid given
    {
        CreatureData const* slaveData = sObjectMgr.GetCreatureData(slaveEntry);
        CreatureData const* masterData = sObjectMgr.GetCreatureData(pTmp->masterId);

        if (!slaveData)
        {
            sLog.outErrorDb("`creature_linking` has a non existing slave (guid: %u, master_guid %u), skipped", slaveEntry, pTmp->masterId);
            return false;
        }
        if (!masterData)
        {
            sLog.outErrorDb("`creature_linking` has a non existing master (guid: %u,, master_guid: %u), skipped", slaveEntry, pTmp->masterId);
            return false;
        }
        if (slaveData->mapid != masterData->mapid)
        {
            sLog.outErrorDb("`creature_linking` has a slave and master on different maps (guid: %u, master_guid: %u), skipped", slaveEntry, pTmp->masterId);
            return false;
        }
    }

    if (pTmp->linkingFlag & ~(LINKING_FLAG_INVALID - 1)  || pTmp->linkingFlag == 0)
    {
        sLog.outErrorDb("`creature_linking%s` has invalid flag, (entry: %u, map: %u, flags: %u), skipped", byEntry ? "_template" : "", slaveEntry, pTmp->mapId, pTmp->linkingFlag);
        return false;
    }

    // Additional checks, depending on flags
    if (pTmp->linkingFlag & FLAG_DESPAWN_ON_RESPAWN && slaveEntry == pTmp->masterId)
    {
        sLog.outErrorDb("`creature_linking%s` has pointless FLAG_DESPAWN_ON_RESPAWN for self, (entry: %u, map: %u), skipped", byEntry ? "_template" : "", slaveEntry, pTmp->mapId);
        return false;
    }

    if (byEntry)
    {
        // Check for uniqueness of mob whom is followed, on whom spawning is dependend
        if (pTmp->searchRange == 0 && pTmp->linkingFlag & (FLAG_FOLLOW | FLAG_CANT_SPAWN_IF_BOSS_DEAD | FLAG_CANT_SPAWN_IF_BOSS_ALIVE))
        {
            std::vector<uint32> masterGuids = SpawnRepository::QueryCreatureGuidsOnMap(pTmp->masterId, pTmp->mapId);
            if (masterGuids.empty())
            {
                sLog.outErrorDb("`creature_linking_template` has FLAG_FOLLOW, but no master, (entry: %u, map: %u, master: %u)", slaveEntry, pTmp->mapId, pTmp->masterId);
                return false;
            }

            if (masterGuids.size() > 1)
            {
                sLog.outErrorDb("`creature_linking_template` has FLAG_FOLLOW, but non unique master, (entry: %u, map: %u, master: %u)", slaveEntry, pTmp->mapId, pTmp->masterId);
                return false;
            }

            pTmp->masterDBGuid = masterGuids[0];
        }
    }

    // All checks are passed, entry/guid is valid
    return true;
}

// This functions checks if the NPC has linked NPCs for dynamic action
bool CreatureLinkingMgr::IsLinkedEventTrigger(uint32 entry, uint32 lowGuid, uint32 mapId) const
{
    // Entry case
    if (m_eventTriggers.find(entry) != m_eventTriggers.end())
    {
        return true;
    }

    // Guid case
    if (m_eventGuidTriggers.find(lowGuid) != m_eventGuidTriggers.end())
    {
        return true;
    }

    // Also return true for npcs that trigger reverse actions, or for followers(needed in respawn)
    if (CreatureLinkingInfo const* pInfo = GetLinkedTriggerInformation(entry, lowGuid, mapId))
    {
        return pInfo->linkingFlag & EVENT_MASK_TRIGGER_TO;
    }

    return false;
}

// This function check if the NPC is a master to other NPCs
// return true only for masters stored by entry - this prevents adding them to master-holder maps
bool CreatureLinkingMgr::IsLinkedMaster(uint32 entry) const
{
    return m_eventTriggers.find(entry) != m_eventTriggers.end();
}

// This function checks if the spawning of this NPC is dependend on other NPCs
bool CreatureLinkingMgr::IsSpawnedByLinkedMob(uint32 entry, uint32 lowGuid, uint32 mapId) const
{
    return IsSpawnedByLinkedMob(GetLinkedTriggerInformation(entry, lowGuid, mapId));
}

/**
 * @brief Checks whether linked-creature spawn rules depend on a master creature.
 *
 * @param pInfo The linking information to evaluate.
 * @return true if the creature is conditionally spawned by linked-mob rules; otherwise false.
 */
bool CreatureLinkingMgr::IsSpawnedByLinkedMob(CreatureLinkingInfo const* pInfo) const
{
    return pInfo && pInfo->linkingFlag & (FLAG_CANT_SPAWN_IF_BOSS_DEAD | FLAG_CANT_SPAWN_IF_BOSS_ALIVE) && (pInfo->masterDBGuid || pInfo->searchRange);
}

/**
 * @brief Resolves linking metadata for a creature entry or database guid on a map.
 *
 * @param entry The creature entry.
 * @param lowGuid The creature low guid.
 * @param mapId The map identifier.
 * @return CreatureLinkingInfo const* The resolved linking information, or NULL if none exists.
 */
CreatureLinkingInfo const* CreatureLinkingMgr::GetLinkedTriggerInformation(uint32 entry, uint32 lowGuid, uint32 mapId) const
{
    // guid case
    CreatureLinkingMapBounds bounds = m_creatureLinkingGuidMap.equal_range(lowGuid);
    for (CreatureLinkingMap::const_iterator iter = bounds.first; iter != bounds.second; ++iter)
    {
        return &(iter->second);
    }

    // entry case
    bounds = m_creatureLinkingMap.equal_range(entry);
    for (CreatureLinkingMap::const_iterator iter = bounds.first; iter != bounds.second; ++iter)
    {
        if (iter->second.mapId == mapId)
        {
            return &(iter->second);
        }
    }

    return NULL;
}
