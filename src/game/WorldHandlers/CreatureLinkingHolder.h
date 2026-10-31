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
 * @addtogroup npc_linking
 * @{
 *
 * @file CreatureLinkingHolder.h
 * The holder of the linked npcs of one map, which triggers their linked actions
 */

#ifndef CREATURE_LINKING_HOLDER_H
#define CREATURE_LINKING_HOLDER_H

#include <map>
#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "CreatureLinkingMgr.h"

class Unit;
class Creature;
class Map;

/**
 * A class to represent the dynamic information of linking NPCs together
 *
 * Every map has an instance of this class as member, in which the dynamic information (GUIDs) are stored
 */

class CreatureLinkingHolder
{
    public:                                                 // Constructors
        CreatureLinkingHolder() {}

    public:                                                 // Accessors
        // Function to add slave-NPCs to the holder
        void AddSlaveToHolder(Creature* pCreature);

        // Function to add master-NPCs to the holder
        void AddMasterToHolder(Creature* pCreature);

        // Function to process actions for linked NPCs
        void DoCreatureLinkingEvent(CreatureLinkingEvent eventType, Creature* pSource, Unit* pEnemy = NULL);

        // Function to check if a passive spawning condition is met
        bool CanSpawn(Creature* pCreature) const;

        // This function lets a slave refollow his master
        bool TryFollowMaster(Creature* pCreature);

    private:
        // Structure associated to a master (entry case)
        struct InfoAndGuids
        {
            uint16 linkingFlag: 16;
            uint16 searchRange: 16;
            GuidList linkedGuids;
        };
        // Structure associated to a master (guid case)
        struct InfoAndGuid
        {
            uint16 linkingFlag;
            ObjectGuid linkedGuid;
        };

        typedef std::multimap < uint32 /*masterEntryOrGuid*/, InfoAndGuids > HolderMap;
        typedef std::pair<HolderMap::iterator, HolderMap::iterator> HolderMapBounds;
        typedef std::multimap < uint32 /*Entry*/, ObjectGuid > BossGuidMap;
        typedef std::pair<BossGuidMap::const_iterator, BossGuidMap::const_iterator> BossGuidMapBounds;

        // Helper function, to process a slave list
        void ProcessSlaveGuidList(CreatureLinkingEvent eventType, Creature* pSource, uint32 flag, uint16 searchRange, GuidList& slaveGuidList, Unit* pEnemy);
        // Helper function, to process a single slave
        void ProcessSlave(CreatureLinkingEvent eventType, Creature* pSource, uint32 flag, Creature* pSlave, Unit* pEnemy);
        // Helper function to set following
        void SetFollowing(Creature* pWho, Creature* pWhom);
        // Helper function to return if a slave is in range of a boss
        bool IsSlaveInRangeOfBoss(Creature const* pSlave, Creature const* pBoss, uint16 searchRange) const;
        bool IsSlaveInRangeOfBoss(Creature const* pBoss, float slaveX, float slaveY, uint16 searchRange) const;
        // Another helper function
        bool IsRespawnReady(uint32 dbLowGuid, Map* _map) const;
        // Helper function for recursive spawning-checks of an additional linked
        bool CanSpawn(uint32 lowGuid, Map* _map, CreatureLinkingInfo const* pInfo, float sx, float sy) const;

        // Storage of Data (boss, flag, searchRange, GuidList) for action triggering
        HolderMap m_holderMap;
        // Storage of Data (boss, flag, slave-guid)
        HolderMap m_holderGuidMap;
        // boss_entry, guid for reverse action triggering and check alive
        BossGuidMap m_masterGuid;
};

#endif
/*! @} */
