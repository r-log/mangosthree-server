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
 * @file ScriptMgrBinding.cpp
 * @brief ScriptMgr's script name and binding registry and the library version query.
 */

#include <set>
#include "ScriptMgr.h"
#include "Log.h"
#include "ProgressBar.h"
#include "ObjectMgr.h"
#include <DBCStores.h>
#include "persistence/world/ScriptDataRepository.h"

// /////////////////////////////////////////////////////////
//              Scripting Library Hooks
// /////////////////////////////////////////////////////////
void ScriptMgr::LoadScriptBinding()
{
    if (!ScriptHooks::IsInstalled())
    {
        return;
    }

    sScriptNameStore.ClearBindings();

    std::vector<ScriptBindingRow> rows = ScriptDataRepository::QueryScriptBindings();
    uint32 count = 0;

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded no script binding.");
        sLog.outString();
        return;
    }

    std::set<uint32> eventIds;                              // Store possible event ids, for checking
    CollectPossibleEventIds(eventIds);

    BarGoLink bar(rows.size());
    for (ScriptBindingRow const& row : rows)
    {
        ++count;
        bar.step();

        uint8 type = row.type;
        int32 id = row.bind;
        const char* scriptName = row.scriptName.c_str();
        uint8 data = row.data;

        if (type >= SCRIPTED_MAX_TYPE)
        {
            sLog.outErrorScriptLib("script_binding table contains a script for non-existent type %u (bind %d), ignoring.", type, id);
            continue;
        }
        uint32 scriptId = sScriptNameStore.GetScriptId(scriptName);
        if (!scriptId)  //this should never happen! the script names are initialized from the same table
        {
            sLog.outErrorScriptLib("something is very bad with your script_binding table!");
            continue;
        }

        // checking if the scripted object actually exists
        bool exists = false;
        switch (type)
        {
        case SCRIPTED_UNIT:
            exists = id > 0 ? bool(ObjectMgr::GetCreatureTemplate(uint32(id))) : bool(sObjectMgr.GetCreatureData(uint32(-id)));
            break;
        case SCRIPTED_GAMEOBJECT:
            exists = id > 0 ? bool(ObjectMgr::GetGameObjectInfo(uint32(id))) : bool(sObjectMgr.GetGOData(uint32(-id)));
            break;
        case SCRIPTED_ITEM:
            exists = bool(ObjectMgr::GetItemPrototype(uint32(id)));
            break;
        case SCRIPTED_AREATRIGGER:
            exists = bool(sAreaTriggerStore.LookupEntry(uint32(id)));
            break;
        case SCRIPTED_SPELL:
        case SCRIPTED_AURASPELL:
            exists = bool(sSpellStore.LookupEntry(uint32(id)));
            break;
        case SCRIPTED_MAPEVENT:
            exists = eventIds.count(uint32(id));
            break;
        case SCRIPTED_MAP:
            exists = bool(sMapStore.LookupEntry(uint32(id)));
            break;
        case SCRIPTED_PVP_ZONE: // for now, no check on special zones
            exists = bool(sAreaStore.LookupEntry(uint32(id)));
            break;
        case SCRIPTED_BATTLEGROUND:
            if (MapEntry const* mapEntry = sMapStore.LookupEntry(uint32(id)))
            {
                exists = mapEntry->IsBattleGround();
            }
            break;
        case SCRIPTED_INSTANCE:
            if (MapEntry const* mapEntry = sMapStore.LookupEntry(uint32(id)))
            {
                exists = mapEntry->IsDungeon();
            }
            break;
        case SCRIPTED_CONDITION:
            exists = sObjectMgr.GetCondition(uint32(id));
            break;
        case SCRIPTED_ACHIEVEMENT:
            break;
        }

        if (!exists)
        {
            sLog.outErrorScriptLib("script type %u (%s) is bound to non-existing entry %d, ignoring.", type, scriptName, id);
            continue;
        }

        if (type == SCRIPTED_SPELL || type == SCRIPTED_AURASPELL)
        {
            id |= uint32(data) << 24;   //incorporate spell effect number into the key
        }

        sScriptNameStore.Bind(ScriptedObjectType(type), id, scriptId);
    }

    sLog.outString("Of the total %u script bindings, loaded succesfully:", count);
    for (uint8 i = 0; i < SCRIPTED_MAX_TYPE; ++i)
    {
        uint32 binds = sScriptNameStore.GetBindingCount(ScriptedObjectType(i));
        if (binds) //ignore missing script types to shorten the log
        {
            sLog.outString(".. type %u: %u binds", i, binds);
            count -= binds;
        }
    }
    sLog.outString("Thus, %u script binds are found bad.", count);

    sLog.outString();
    return;
}

/**
 * @brief Reloads script bindings in debug builds.
 *
 * @return true if bindings were reloaded; otherwise false.
 */
bool ScriptMgr::ReloadScriptBinding()
{
#ifdef _DEBUG
    sScriptNameStore.GetBindingLock().lock();
    LoadScriptBinding();
    sScriptNameStore.GetBindingLock().unlock();
    return true;
#else
    return false;
#endif /* _DEBUG */
}

/**
 * @brief Loads and sorts the distinct script names referenced by script bindings.
 */
void ScriptMgr::LoadScriptNames()
{
    sScriptNameStore.AddName("");
    std::vector<ScriptNameRow> rows = ScriptDataRepository::QueryScriptNames();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded empty set of Script Names!");
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());
    uint32 count = 0;

    for (ScriptNameRow const& row : rows)
    {
        bar.step();
        sScriptNameStore.AddName((row.scriptName ? row.scriptName->c_str() : NULL));
        ++count;
    }

    sScriptNameStore.SortNames();

    sLog.outString(">> Loaded %d unique Script Names", count);
    sLog.outString();
}
