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

#ifndef MANGOS_H_SCRIPTNAMESTORE
#define MANGOS_H_SCRIPTNAMESTORE

#include "Platform/Define.h"
#include "Policies/Singleton.h"

#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

enum ScriptedObjectType
{
    SCRIPTED_UNIT           = 0,    //CreatureScript
    SCRIPTED_GAMEOBJECT     = 1,    //GameObjectScript
    SCRIPTED_ITEM           = 2,    //ItemScript
    SCRIPTED_AREATRIGGER    = 3,    //AreaTriggerScript
    SCRIPTED_SPELL          = 4,    //SpellScript
    SCRIPTED_AURASPELL      = 5,    //AuraScript
    SCRIPTED_MAPEVENT       = 6,    //MapEventScript
    SCRIPTED_MAP            = 7,    //ZoneScript
    SCRIPTED_BATTLEGROUND   = 8,    //BattleGroundScript
    SCRIPTED_PVP_ZONE       = 9,    //OutdoorPvPScript
    SCRIPTED_INSTANCE       = 10,   //InstanceScript
    SCRIPTED_CONDITION      = 11,   //ConditionScript
    SCRIPTED_ACHIEVEMENT    = 12,   //AchievementScript
    SCRIPTED_MAX_TYPE
};

/**
 * The script names and their bindings (`script_binding`). The names are held in id order: sorted,
 * the empty name first, so that id 0 is no script. An id past the last answers the empty name, and a
 * name not held answers 0. Per scripted object type, an entry is bound to a script id; an entry with
 * no binding answers 0, and so does a type past the last, which is logged.
 */
class ScriptNameStore
{
    public:
        void AddName(char const* name);
        /// Sorts the names; a name's id is its place after sorting.
        void SortNames();
        char const* GetScriptName(uint32 id) const;
        uint32 GetScriptId(char const* name) const;
        uint32 GetScriptIdsCount() const;

        void ClearBindings();
        void Bind(ScriptedObjectType type, int32 entry, uint32 scriptId);
        uint32 GetBindingCount(ScriptedObjectType type) const;
        uint32 GetBoundScriptId(ScriptedObjectType entity, int32 entry);
#ifdef _DEBUG
        /// Held exclusively while a reload rebuilds the bindings; each lookup holds it shared.
        std::shared_mutex& GetBindingLock();
#endif /* _DEBUG */

    private:
        typedef std::vector<std::string> ScriptNameMap;
        typedef std::unordered_map<int32, uint32> EntryToScriptIdMap;

        EntryToScriptIdMap m_scriptBind[SCRIPTED_MAX_TYPE];
        ScriptNameMap m_scriptNames;
#ifdef _DEBUG
        std::shared_mutex m_bindMutex;
#endif /* _DEBUG */
};

#define sScriptNameStore MaNGOS::Singleton<ScriptNameStore>::Instance()

/**
 * Returns the numeric script identifier for the specified script name.
 */
uint32 GetScriptId(const char* name);

/**
 * Returns the script name associated with the specified script identifier.
 */
char const* GetScriptName(uint32 id);

/**
 * Returns the number of registered script identifiers.
 */
uint32 GetScriptIdsCount();

#endif
