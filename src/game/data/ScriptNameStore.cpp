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

#include "data/ScriptNameStore.h"
#include "Log.h"

#include <algorithm>

void ScriptNameStore::AddName(char const* name)
{
    m_scriptNames.push_back(name);
}

void ScriptNameStore::SortNames()
{
    std::sort(m_scriptNames.begin(), m_scriptNames.end());
}

char const* ScriptNameStore::GetScriptName(uint32 id) const
{
    return id < m_scriptNames.size() ? m_scriptNames[id].c_str() : "";
}

uint32 ScriptNameStore::GetScriptId(char const* name) const
{
    // use binary search to find the script name in the sorted vector
    // assume "" is the first element
    if (!name)
    {
        return 0;
    }

    ScriptNameMap::const_iterator itr =
        std::lower_bound(m_scriptNames.begin(), m_scriptNames.end(), name);

    if (itr == m_scriptNames.end() || *itr != name)
    {
        return 0;
    }

    return uint32(itr - m_scriptNames.begin());
}

uint32 ScriptNameStore::GetScriptIdsCount() const
{
    return m_scriptNames.size();
}

void ScriptNameStore::ClearBindings()
{
    for (int i = 0; i < SCRIPTED_MAX_TYPE; ++i)
    {
        m_scriptBind[i].clear();
    }
}

void ScriptNameStore::Bind(ScriptedObjectType type, int32 entry, uint32 scriptId)
{
    m_scriptBind[type][entry] = scriptId;
}

uint32 ScriptNameStore::GetBindingCount(ScriptedObjectType type) const
{
    return uint32(m_scriptBind[type].size());
}

uint32 ScriptNameStore::GetBoundScriptId(ScriptedObjectType entity, int32 entry)
{
#ifdef _DEBUG
    m_bindMutex.lock_shared();
#endif /* _DEBUG */
    uint32 id = 0;
    if (entity < SCRIPTED_MAX_TYPE)
    {
        EntryToScriptIdMap::iterator it = m_scriptBind[entity].find(entry);
        if (it != m_scriptBind[entity].end())
        {
            id = it->second;
        }
    }
    else
        sLog.outErrorScriptLib("asking a script for non-existing entity type %u!", entity);
#ifdef _DEBUG
    m_bindMutex.unlock_shared();
#endif /* _DEBUG */
    return id;
}

#ifdef _DEBUG
std::shared_mutex& ScriptNameStore::GetBindingLock()
{
    return m_bindMutex;
}
#endif /* _DEBUG */

uint32 GetScriptId(const char* name)
{
    return sScriptNameStore.GetScriptId(name);
}

char const* GetScriptName(uint32 id)
{
    return sScriptNameStore.GetScriptName(id);
}

uint32 GetScriptIdsCount()
{
    return sScriptNameStore.GetScriptIdsCount();
}
