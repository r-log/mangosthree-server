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

#ifndef MANGOS_H_SCRIPTMGR
#define MANGOS_H_SCRIPTMGR

#include <unordered_map>
#include "Platform/Define.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include "Policies/Singleton.h"
#include "ObjectGuid.h"
#include "DBCEnums.h"
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include "data/DbScriptStore.h"
#include "data/ScriptNameStore.h"
#include "data/ScriptHooks.h"

struct AreaTriggerEntry;
struct SpellEntry;
class Aura;
class Creature;
class CreatureAI;
class GameObject;
class GameObjectAI;
class InstanceData;
class Item;
class Map;
class Object;
class Player;
class Quest;
class SpellCastTargets;
class Unit;
class WorldObject;

enum ScriptImplementation
{
    SCRIPT_FROM_DATABASE    = 0,
    SCRIPT_FROM_CORE        = 1,
};

class ScriptAction
{
    public:
        ScriptAction(DBScriptType _type, Map* _map, ObjectGuid _sourceGuid, ObjectGuid _targetGuid, ObjectGuid _ownerGuid, ScriptInfo const* _script) :
            m_type(_type), m_map(_map), m_sourceGuid(_sourceGuid), m_targetGuid(_targetGuid), m_ownerGuid(_ownerGuid), m_script(_script)
        {}

        bool HandleScriptStep();                            // return true IF AND ONLY IF the script should be terminated

        DBScriptType GetType() const
        {
            return m_type;
        }
        uint32 GetId() const
        {
            return m_script->id;
        }
        ObjectGuid GetSourceGuid() const
        {
            return m_sourceGuid;
        }
        ObjectGuid GetTargetGuid() const
        {
            return m_targetGuid;
        }
        ObjectGuid GetOwnerGuid() const
        {
            return m_ownerGuid;
        }

        bool IsSameScript(DBScriptType type, uint32 id, ObjectGuid sourceGuid, ObjectGuid targetGuid, ObjectGuid ownerGuid) const
        {
            return type == m_type && id == GetId() &&
                   (sourceGuid == m_sourceGuid || !sourceGuid) &&
                   (targetGuid == m_targetGuid || !targetGuid) &&
                   (ownerGuid == m_ownerGuid || !ownerGuid);
        }

    private:
        DBScriptType m_type;                                // which type has the script was started
        Map* m_map;                                         // Map on which the action will be executed
        ObjectGuid m_sourceGuid;
        ObjectGuid m_targetGuid;
        ObjectGuid m_ownerGuid;                             // owner of source if source is item
        ScriptInfo const* m_script;                         // pointer to static script data

        // Helper functions
        bool GetScriptCommandObject(const ObjectGuid guid, bool includeItem, Object*& resultObject);
        bool GetScriptProcessTargets(WorldObject* pOrigSource, WorldObject* pOrigTarget, WorldObject*& pFinalSource, WorldObject*& pFinalTarget);
        bool LogIfNotCreature(WorldObject* pWorldObject);
        bool LogIfNotUnit(WorldObject* pWorldObject);
        bool LogIfNotGameObject(WorldObject* pWorldObject);
        bool LogIfNotPlayer(WorldObject* pWorldObject);
        Player* GetPlayerTargetOrSourceAndLog(WorldObject* pSource, WorldObject* pTarget);
};


enum ScriptLoadResult
{
    SCRIPT_LOAD_OK,
    SCRIPT_LOAD_ERR_NOT_FOUND,
    SCRIPT_LOAD_ERR_WRONG_API,
    SCRIPT_LOAD_ERR_OUTDATED,
};

class ScriptMgr
{
    public:
        ScriptMgr();
        ~ScriptMgr();

        void LoadDbScripts(DBScriptType type);
        void LoadDbScriptStrings();

        void LoadScriptNames();
        void LoadScriptBinding();

        bool ReloadScriptBinding();

        ScriptLoadResult LoadScriptLibrary(const char* libName);
        void UnloadScriptLibrary();
        uint32 IncreaseScheduledScriptsCount()
        {
            return (uint32)++m_scheduledScripts;
        }
        uint32 DecreaseScheduledScriptCount()
        {
            return (uint32)--m_scheduledScripts;
        }
        uint32 DecreaseScheduledScriptCount(size_t count)
        {
            return (uint32)(m_scheduledScripts -= count);
        }
        bool IsScriptScheduled() const
        {
            return m_scheduledScripts > 0;
        }

    private:
        void CollectPossibleEventIds(std::set<uint32>& eventIds);
        void LoadScripts(DBScriptType type);
        void CheckScriptTexts(std::set<int32>& ids);

        // atomic op counter for active scripts amount
        std::atomic<long> m_scheduledScripts;
        char __cache_guard[1024];
};

// Starters for events
bool StartEvents_Event(Map* map, uint32 id, Object* source, Object* target, bool isStart = true, Unit* forwardToPvp = NULL);

#define sScriptMgr MaNGOS::Singleton<ScriptMgr>::Instance()

/**
 * Sets the external waypoint table used for loading script waypoints.
 */
void SetExternalWaypointTable(char const* tableName);

/**
 * Adds a waypoint definition from the external waypoint source.
 */
bool AddWaypointFromExternal(uint32 entry, int32 pathId, uint32 pointId, float x, float y, float z, float o, uint32 waittime);

#endif
