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
 * @file CreatureLinkingHolder.cpp
 * @brief The linked npcs of one map: their linked aggro, evade, death, respawn and despawn
 * actions, their following, and the spawn conditions that depend on a master
 */

#include <cmath>
#include "Utilities/MathDefines.h"
#include "CreatureLinkingHolder.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "Map.h"
#include "MapPersistentStateMgr.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "SharedDefines.h"

// Function to add slave-NPCs to the holder
void CreatureLinkingHolder::AddSlaveToHolder(Creature* pCreature)
{
    CreatureLinkingInfo const* pInfo = sCreatureLinkingMgr.GetLinkedTriggerInformation(pCreature->GetEntry(), pCreature->GetGUIDLow(), pCreature->GetMapId());
    if (!pInfo)
    {
        return;
    }

    if (pInfo->mapId == INVALID_MAP_ID)                     // Guid case, store master->slaves for fast access
    {
        HolderMapBounds bounds = m_holderGuidMap.equal_range(pInfo->masterId);
        for (HolderMap::iterator itr = bounds.first; itr != bounds.second; ++itr)
        {
            if (itr->second.linkingFlag == pInfo->linkingFlag)
            {
                itr->second.linkedGuids.push_back(pCreature->GetObjectGuid());
                pCreature = NULL;                           // Store that is was handled
                break;
            }
        }

        // If this is a new flag, insert new entry
        if (pCreature)
        {
            InfoAndGuids tmp;
            tmp.linkedGuids.push_back(pCreature->GetObjectGuid());
            tmp.linkingFlag = pInfo->linkingFlag;
            tmp.searchRange = 0;
            m_holderGuidMap.insert(HolderMap::value_type(pInfo->masterId, tmp));
        }
        return;
    }

    // First try to find holder with same flag
    HolderMapBounds bounds = m_holderMap.equal_range(pInfo->masterId);
    for (HolderMap::iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second.linkingFlag == pInfo->linkingFlag && itr->second.searchRange == pInfo->searchRange)
        {
            itr->second.linkedGuids.push_back(pCreature->GetObjectGuid());
            pCreature = NULL;                               // Store that is was handled
            break;
        }
    }

    // If this is a new flag, insert new entry
    if (pCreature)
    {
        InfoAndGuids tmp;
        tmp.linkedGuids.push_back(pCreature->GetObjectGuid());
        tmp.linkingFlag = pInfo->linkingFlag;
        tmp.searchRange = pInfo->searchRange;
        m_holderMap.insert(HolderMap::value_type(pInfo->masterId, tmp));
    }
}

// Function to add master-NPCs to the holder
void CreatureLinkingHolder::AddMasterToHolder(Creature* pCreature)
{
    if (pCreature->IsPet())
    {
        return;
    }

    // Only add master NPCs (by entry)
    if (!sCreatureLinkingMgr.IsLinkedMaster(pCreature->GetEntry()))
    {
        return;
    }

    // Check, if already stored
    BossGuidMapBounds bounds = m_masterGuid.equal_range(pCreature->GetEntry());
    for (BossGuidMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
        if (itr->second == pCreature->GetObjectGuid())
        {
            return;                                          // Already added
        }

    m_masterGuid.insert(BossGuidMap::value_type(pCreature->GetEntry(), pCreature->GetObjectGuid()));
}

// Function to process actions for linked NPCs
void CreatureLinkingHolder::DoCreatureLinkingEvent(CreatureLinkingEvent eventType, Creature* pSource, Unit* pEnemy /* = NULL*/)
{
    // This check will be needed in reload case
    if (!sCreatureLinkingMgr.IsLinkedEventTrigger(pSource->GetEntry(), pSource->GetGUIDLow(), pSource->GetMapId()))
    {
        return;
    }

    // Ignore atypic behaviour
    if (pSource->IsControlledByPlayer())
    {
        return;
    }

    if (eventType == LINKING_EVENT_AGGRO && !pEnemy)
    {
        return;
    }

    uint32 eventFlagFilter = 0;
    uint32 reverseEventFlagFilter = 0;

    switch (eventType)
    {
        case LINKING_EVENT_AGGRO:   eventFlagFilter = EVENT_MASK_ON_AGGRO;   reverseEventFlagFilter = FLAG_TO_AGGRO_ON_AGGRO;   break;
        case LINKING_EVENT_EVADE:   eventFlagFilter = EVENT_MASK_ON_EVADE;   reverseEventFlagFilter = FLAG_TO_RESPAWN_ON_EVADE; break;
        case LINKING_EVENT_DIE:     eventFlagFilter = EVENT_MASK_ON_DIE;     reverseEventFlagFilter = 0;                        break;
        case LINKING_EVENT_RESPAWN: eventFlagFilter = EVENT_MASK_ON_RESPAWN; reverseEventFlagFilter = FLAG_FOLLOW;              break;
        case LINKING_EVENT_DESPAWN: eventFlagFilter = EVENT_MASK_ON_DESPAWN; reverseEventFlagFilter = 0;                        break;
    }

    // Process Slaves (by entry)
    HolderMapBounds bounds = m_holderMap.equal_range(pSource->GetEntry());
    for (HolderMap::iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        ProcessSlaveGuidList(eventType, pSource, itr->second.linkingFlag & eventFlagFilter, itr->second.searchRange, itr->second.linkedGuids, pEnemy);
    }

    // Process Slaves (by guid)
    bounds = m_holderGuidMap.equal_range(pSource->GetGUIDLow());
    for (HolderMap::iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        ProcessSlaveGuidList(eventType, pSource, itr->second.linkingFlag & eventFlagFilter, itr->second.searchRange, itr->second.linkedGuids, pEnemy);
    }

    // Process Master
    if (CreatureLinkingInfo const* pInfo = sCreatureLinkingMgr.GetLinkedTriggerInformation(pSource->GetEntry(), pSource->GetGUIDLow(), pSource->GetMapId()))
    {
        if (pInfo->linkingFlag & reverseEventFlagFilter)
        {
            Creature* pMaster = NULL;
            if (pInfo->mapId != INVALID_MAP_ID)             // entry case
            {
                BossGuidMapBounds finds = m_masterGuid.equal_range(pInfo->masterId);
                for (BossGuidMap::const_iterator itr = finds.first; itr != finds.second; ++itr)
                {
                    pMaster = pSource->GetMap()->GetCreature(itr->second);
                    if (pMaster && IsSlaveInRangeOfBoss(pSource, pMaster, pInfo->searchRange))
                    {
                        break;
                    }
                }
            }
            else                                            // guid case
            {
                CreatureData const* masterData = sObjectMgr.GetCreatureData(pInfo->masterDBGuid);
                CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(masterData->id);
                pMaster = pSource->GetMap()->GetCreature(ObjectGuid(cInfo->GetHighGuid(), cInfo->Entry, pInfo->masterDBGuid));
            }

            if (pMaster)
            {
                switch (eventType)
                {
                    case LINKING_EVENT_AGGRO:
                        if (pMaster->IsControlledByPlayer())
                        {
                            return;
                        }

                        if (pMaster->IsInCombat())
                        {
                            pMaster->SetInCombatWith(pEnemy);
                        }
                        else
                        {
                            pMaster->AI()->AttackStart(pEnemy);
                        }
                        break;
                    case LINKING_EVENT_EVADE:
                        if (!pMaster->IsAlive())
                        {
                            pMaster->Respawn();
                        }
                        break;
                    case LINKING_EVENT_RESPAWN:
                        if (pMaster->IsAlive())
                        {
                            SetFollowing(pSource, pMaster);
                        }
                        break;
                    case LINKING_EVENT_DIE:                 // Nothing linked for this case
                    case LINKING_EVENT_DESPAWN:             // Nothing linked for this case
                        break;
                }
            }
        }
    }
}

// Helper function, to process a slave list
void CreatureLinkingHolder::ProcessSlaveGuidList(CreatureLinkingEvent eventType, Creature* pSource, uint32 flag, uint16 searchRange, GuidList& slaveGuidList, Unit* pEnemy)
{
    if (!flag)
    {
        return;
    }

    for (GuidList::iterator slave_itr = slaveGuidList.begin(); slave_itr != slaveGuidList.end();)
    {
        Creature* pSlave = pSource->GetMap()->GetCreature(*slave_itr);
        if (!pSlave)
        {
            // Remove old guid first
            slaveGuidList.erase(slave_itr++);
            continue;
        }

        ++slave_itr;

        // Ignore Pets
        if (pSlave->IsPet())
        {
            continue;
        }

        // Handle single slave
        if (IsSlaveInRangeOfBoss(pSlave, pSource, searchRange))
        {
            ProcessSlave(eventType, pSource, flag, pSlave, pEnemy);
        }
    }
}

// Helper function, to process a single slave
void CreatureLinkingHolder::ProcessSlave(CreatureLinkingEvent eventType, Creature* pSource, uint32 flag, Creature* pSlave, Unit* pEnemy)
{
    switch (eventType)
    {
        case LINKING_EVENT_AGGRO:
            if (flag & FLAG_AGGRO_ON_AGGRO)
            {
                if (pSlave->IsControlledByPlayer())
                {
                    return;
                }

                if (pSlave->IsInCombat())
                {
                    pSlave->SetInCombatWith(pEnemy);
                }
                else
                {
                    pSlave->AI()->AttackStart(pEnemy);
                }
            }
            break;
        case LINKING_EVENT_EVADE:
            if (flag & FLAG_DESPAWN_ON_EVADE && pSlave->IsAlive())
            {
                pSlave->ForcedDespawn();
            }
            if (flag & FLAG_RESPAWN_ON_EVADE && !pSlave->IsAlive())
            {
                pSlave->Respawn();
            }
            break;
        case LINKING_EVENT_DIE:
            if (flag & FLAG_SELFKILL_ON_DEATH && pSlave->IsAlive())
            {
                pSlave->DealDamage(pSlave, pSlave->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
            }
            if (flag & FLAG_DESPAWN_ON_DEATH && pSlave->IsAlive())
            {
                pSlave->ForcedDespawn();
            }
            if (flag & FLAG_RESPAWN_ON_DEATH && !pSlave->IsAlive())
            {
                pSlave->Respawn();
            }
            break;
        case LINKING_EVENT_RESPAWN:
            if (flag & FLAG_RESPAWN_ON_RESPAWN)
            {
                // Additional check to prevent endless loops (in case whole group respawns on first respawn)
                if (!pSlave->IsAlive() && pSlave->GetRespawnTime() > time(NULL))
                {
                    pSlave->Respawn();
                }
            }
            else if (flag & FLAG_DESPAWN_ON_RESPAWN && pSlave->IsAlive())
            {
                pSlave->ForcedDespawn();
            }

            if (flag & FLAG_FOLLOW && pSlave->IsAlive() && !pSlave->IsInCombat())
            {
                SetFollowing(pSlave, pSource);
            }

            break;
        case LINKING_EVENT_DESPAWN:
            if (flag & FLAG_DESPAWN_ON_DESPAWN && !pSlave->IsDespawned())
            {
                pSlave->ForcedDespawn();
            }

            break;
    }
}

// Helper function to set following
void CreatureLinkingHolder::SetFollowing(Creature* pWho, Creature* pWhom)
{
    // Do some calculations
    float sX, sY, sZ, mX, mY, mZ, mO;
    sX = pWho->Spawn().X();
    sY = pWho->Spawn().Y();
    sZ = pWho->Spawn().Z();
    mX = pWhom->Spawn().X();
    mY = pWhom->Spawn().Y();
    mZ = pWhom->Spawn().Z();
    mO = pWhom->Spawn().Facing();

    float dx, dy, dz;
    dx = sX - mX;
    dy = sY - mY;
    dz = sZ - mZ;

    float dist = sqrt(dx * dx + dy * dy + dz * dz);
    // REMARK: This code needs the same distance calculation that is used for following
    // Atm this means we have to subtract the bounding radiuses
    dist = dist - pWho->Where().Extent() - pWhom->Where().Extent();
    if (dist < 0.0f)
    {
        dist = 0.0f;
    }

    // Need to pass the relative angle to following
    float angle = atan2(dy, dx) - mO;
    angle = (angle >= 0) ? angle : 2 * M_PI_F + angle;

    pWho->GetMotionMaster()->MoveFollow(pWhom, dist, angle);
}

// Function to check if a slave belongs to a boss by range-issue
bool CreatureLinkingHolder::IsSlaveInRangeOfBoss(Creature const* pSlave, Creature const* pBoss, uint16 searchRange) const
{
    float sX, sY, sZ;
    sX = pSlave->Spawn().X();
    sY = pSlave->Spawn().Y();
    sZ = pSlave->Spawn().Z();
    return IsSlaveInRangeOfBoss(pBoss, sX, sY, searchRange);
}

/**
 * @brief Checks whether a slave spawn position lies within a boss search radius.
 *
 * @param pBoss The boss creature used as the center point.
 * @param sX The slave respawn X coordinate.
 * @param sY The slave respawn Y coordinate.
 * @param searchRange The allowed search radius.
 * @return true if the slave lies within range, or if no range restriction exists.
 */
bool CreatureLinkingHolder::IsSlaveInRangeOfBoss(Creature const* pBoss, float sX, float sY, uint16 searchRange) const
{
    if (!searchRange)
    {
        return true;
    }

    // Do some calculations
    float mX, mY, mZ, dx, dy;
    mX = pBoss->Spawn().X();
    mY = pBoss->Spawn().Y();
    mZ = pBoss->Spawn().Z();

    dx = sX - mX;
    dy = sY - mY;

    return dx * dx + dy * dy < searchRange * searchRange;
}

// helper function to check if a lowguid can respawn
bool CreatureLinkingHolder::IsRespawnReady(uint32 dbLowGuid, Map* _map) const
{
    time_t respawnTime = _map->GetPersistentState()->GetCreatureRespawnTime(dbLowGuid);
    return (!respawnTime || respawnTime <= time(NULL)) && CanSpawn(dbLowGuid, _map, NULL, 0.0f, 0.0f);
}

// Function to check if a passive spawning condition is met
bool CreatureLinkingHolder::CanSpawn(Creature* pCreature) const
{
    CreatureLinkingInfo const*  pInfo = sCreatureLinkingMgr.GetLinkedTriggerInformation(pCreature->GetEntry(), pCreature->GetGUIDLow(), pCreature->GetMapId());
    if (!pInfo)
    {
        return true;
    }

    float sx, sy, sz;
    sx = pCreature->Spawn().X();
    sy = pCreature->Spawn().Y();
    sz = pCreature->Spawn().Z();
    return CanSpawn(0, pCreature->GetMap(), pInfo, sx, sy);
}

/** Worker function to check if a spawning condition is met
 *
 * This function is used directly from above function, and for recursive use
 * in case of recursive use it is used only on _map with information of lowGuid.
 *
 * @param lowGuid (only relevant in case of recursive uses) -- db-guid of the npc that is checked
 * @param _map Map on which things are checked
 * @param pInfo (only shipped in case of initial use) -- used as marker of first use, also in first use filled directly
 * @param sx, sy (spawn position of the checked npc with initial use)
 */
bool CreatureLinkingHolder::CanSpawn(uint32 lowGuid, Map* _map, CreatureLinkingInfo const* pInfo, float sx, float sy) const
{
    if (!pInfo)                                             // Prepare data for recursive use
    {
        CreatureData const* data = sObjectMgr.GetCreatureData(lowGuid);
        if (!data)
        {
            return true;
        }

        pInfo = sCreatureLinkingMgr.GetLinkedTriggerInformation(data->id, lowGuid, data->mapid);
        if (!pInfo)
        {
            return true;
        }

        // Has lowGuid npc actually spawning linked?
        if (!sCreatureLinkingMgr.IsSpawnedByLinkedMob(pInfo))
        {
            return true;
        }

        sx = data->posX;                                    // Fill position data
        sy = data->posY;
    }

    if (pInfo->searchRange == 0)                            // Map wide case
    {
        if (!pInfo->masterDBGuid)
        {
            return false;                                    // This should never happen
        }

        if (pInfo->linkingFlag & FLAG_CANT_SPAWN_IF_BOSS_DEAD)
        {
            return IsRespawnReady(pInfo->masterDBGuid, _map);
        }
        else if (pInfo->linkingFlag & FLAG_CANT_SPAWN_IF_BOSS_ALIVE)
        {
            return !IsRespawnReady(pInfo->masterDBGuid, _map);
        }
        else
        {
            return true;
        }
    }

    // Search for nearby master
    BossGuidMapBounds finds = m_masterGuid.equal_range(pInfo->masterId);
    for (BossGuidMap::const_iterator itr = finds.first; itr != finds.second; ++itr)
    {
        Creature* pMaster = _map->GetCreature(itr->second);
        if (pMaster && IsSlaveInRangeOfBoss(pMaster, sx, sy, pInfo->searchRange))
        {
            if (pInfo->linkingFlag & FLAG_CANT_SPAWN_IF_BOSS_DEAD)
            {
                return pMaster->IsAlive();
            }
            else if (pInfo->linkingFlag & FLAG_CANT_SPAWN_IF_BOSS_ALIVE)
            {
                return !pMaster->IsAlive();
            }
            else
            {
                return true;
            }
        }
    }

    return true;                                            // local boss does not exist - spawn
}

// This function lets a slave refollow his master
bool CreatureLinkingHolder::TryFollowMaster(Creature* pCreature)
{
    CreatureLinkingInfo const*  pInfo = sCreatureLinkingMgr.GetLinkedTriggerInformation(pCreature->GetEntry(), pCreature->GetGUIDLow(), pCreature->GetMapId());
    if (!pInfo || !(pInfo->linkingFlag & FLAG_FOLLOW))
    {
        return false;
    }

    Creature* pMaster = NULL;
    if (pInfo->mapId != INVALID_MAP_ID)                     // entry case
    {
        BossGuidMapBounds finds = m_masterGuid.equal_range(pInfo->masterId);
        for (BossGuidMap::const_iterator itr = finds.first; itr != finds.second; ++itr)
        {
            pMaster = pCreature->GetMap()->GetCreature(itr->second);
            if (pMaster && IsSlaveInRangeOfBoss(pCreature, pMaster, pInfo->searchRange))
            {
                break;
            }
        }
    }
    else                                                    // guid case
    {
        CreatureData const* masterData = sObjectMgr.GetCreatureData(pInfo->masterDBGuid);
        CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(masterData->id);
        pMaster = pCreature->GetMap()->GetCreature(ObjectGuid(cInfo->GetHighGuid(), cInfo->Entry, pInfo->masterDBGuid));
    }

    if (pMaster && pMaster->IsAlive())
    {
        SetFollowing(pCreature, pMaster);
        return true;
    }

    return false;
}

/*! @} */
