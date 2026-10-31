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
 * @addtogroup npc_linking System to link groups of NPCs together
 * This NPC-linking system in MaNGOS consists of 4 files:
 * - CreatureLinkingMgr.h and CreatureLinkingMgr.cpp: the links loaded from the database
 * - CreatureLinkingHolder.h and CreatureLinkingHolder.cpp: the holder of the linked npcs for every map
 * as well of
 * - hooks in Creature.cpp, to trigger actions
 *
 * @{
 *
 * @file CreatureLinkingMgr.h
 * This file contains the the headers needed for MaNGOS to link NPCs together
 *
 */

#ifndef CREATURE_LINKING_MGR_H
#define CREATURE_LINKING_MGR_H

#include <unordered_set>
#include <utility>
#include "Platform/Define.h"
#include <map>
#include "Policies/Singleton.h"
#include <functional>

// enum on which Events an action for linked NPCs can trigger
enum CreatureLinkingEvent
{
    LINKING_EVENT_AGGRO         = 0,
    LINKING_EVENT_EVADE         = 1,
    LINKING_EVENT_DIE           = 2,
    LINKING_EVENT_RESPAWN       = 3,
    LINKING_EVENT_DESPAWN       = 4,
};

// enum describing possible flags action flags for NPCs linked to other NPCs
// These flags are actually put into the database
// FLAG_TO_ means, that in this case the linked NPC will also trigger an action for the NPC it is linked to
enum CreatureLinkingFlags
{
    // Dynamic behaviour, in combat
    FLAG_AGGRO_ON_AGGRO             = 0x0001,
    FLAG_TO_AGGRO_ON_AGGRO          = 0x0002,
    FLAG_RESPAWN_ON_EVADE           = 0x0004,
    FLAG_TO_RESPAWN_ON_EVADE        = 0x0008,
    FLAG_DESPAWN_ON_EVADE           = 0x1000,
    FLAG_DESPAWN_ON_DEATH           = 0x0010,
    FLAG_SELFKILL_ON_DEATH          = 0x0020,
    FLAG_RESPAWN_ON_DEATH           = 0x0040,
    FLAG_RESPAWN_ON_RESPAWN         = 0x0080,
    FLAG_DESPAWN_ON_RESPAWN         = 0x0100,

    // Dynamic behaviour, out of combat
    FLAG_FOLLOW                     = 0x0200,
    FLAG_DESPAWN_ON_DESPAWN         = 0x2000,

    // Passive behaviour
    FLAG_CANT_SPAWN_IF_BOSS_DEAD    = 0x0400,
    FLAG_CANT_SPAWN_IF_BOSS_ALIVE   = 0x0800,

    LINKING_FLAG_INVALID            = 0x4000,               // TODO adjust when other flags are implemented
};

// Structure holding the information for an entry
struct CreatureLinkingInfo
{
    uint32 mapId;
    uint32 masterId;
    uint32 masterDBGuid;
    uint16 linkingFlag: 16;
    uint16 searchRange: 16;
};

/// The map id of a link by guid, which is not bound to a map.
const uint32 INVALID_MAP_ID = 0xFFFFFFFF;

// Linked actions and corresponding flags
enum CreatureLinkingEventMask
{
    EVENT_MASK_ON_AGGRO     = FLAG_AGGRO_ON_AGGRO,
    EVENT_MASK_ON_EVADE     = FLAG_RESPAWN_ON_EVADE | FLAG_DESPAWN_ON_EVADE,
    EVENT_MASK_ON_DIE       = FLAG_DESPAWN_ON_DEATH | FLAG_SELFKILL_ON_DEATH | FLAG_RESPAWN_ON_DEATH | FLAG_FOLLOW,
    EVENT_MASK_ON_RESPAWN   = FLAG_RESPAWN_ON_RESPAWN | FLAG_DESPAWN_ON_RESPAWN | FLAG_FOLLOW,
    EVENT_MASK_TRIGGER_TO   = FLAG_TO_AGGRO_ON_AGGRO | FLAG_TO_RESPAWN_ON_EVADE | FLAG_FOLLOW,
    EVENT_MASK_ON_DESPAWN   = FLAG_DESPAWN_ON_DESPAWN,
};

/**
 * A class to represent the static information of linking NPCs together
 */

class CreatureLinkingMgr
{
    public:                                                 // Constructors
        CreatureLinkingMgr() {}

    public:                                                 // Initialisation
        void LoadFromDB();

    public:                                                 // Accessors
        // This functions checks if the NPC (entry, low guid, map) triggers actions for other NPCs
        bool IsLinkedEventTrigger(uint32 entry, uint32 lowGuid, uint32 mapId) const;

        // This function checks if the NPC entry is a master NPC.
        bool IsLinkedMaster(uint32 entry) const;

        // This function checks if the spawning of this NPC is dependend on other NPCs
        bool IsSpawnedByLinkedMob(uint32 entry, uint32 lowGuid, uint32 mapId) const;
        bool IsSpawnedByLinkedMob(CreatureLinkingInfo const* pInfo) const;

        // This gives the information of a linked NPC (describes action when its ActionTrigger triggers)
        // Depends of the map
        CreatureLinkingInfo const* GetLinkedTriggerInformation(uint32 entry, uint32 lowGuid, uint32 mapId) const;

    private:
        typedef std::multimap < uint32 /*slaveEntry*/, CreatureLinkingInfo > CreatureLinkingMap;
        typedef std::pair<CreatureLinkingMap::const_iterator, CreatureLinkingMap::const_iterator> CreatureLinkingMapBounds;

        // Storage of Data: npc_entry_slave, (map, npc_entry_master, flag, master_db_guid[If Unique], search_range)
        CreatureLinkingMap m_creatureLinkingMap;
        // Storage of Data: npc_guid_slave, (map, npc_guid_master, flag, master_db_guid, search_range)
        CreatureLinkingMap m_creatureLinkingGuidMap;

        // Lookup Storage for fast access:
        std::unordered_set<uint32> m_eventTriggers;              // master by entry
        std::unordered_set<uint32> m_eventGuidTriggers;          // master by guid

        // Check-routine
        static bool IsLinkingEntryValid(uint32 slaveEntry, CreatureLinkingInfo* pInfo, bool byEntry);
};

#define sCreatureLinkingMgr MaNGOS::Singleton<CreatureLinkingMgr>::Instance()

#endif
/*! @} */
