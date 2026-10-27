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

#include "data/InstanceTemplateStore.h"

void InstanceTemplateStore::Clear()
{
    m_templates.clear();
}

void InstanceTemplateStore::Set(InstanceTemplate const& instance)
{
    m_templates[instance.map] = instance;
}

InstanceTemplate const* InstanceTemplateStore::Find(uint32 map) const
{
    InstanceTemplateMap::const_iterator itr = m_templates.find(map);
    if (itr != m_templates.end())
    {
        return &itr->second;
    }
    return NULL;
}

InstanceTemplate* InstanceTemplateStore::FindForEdit(uint32 map)
{
    InstanceTemplateMap::iterator itr = m_templates.find(map);
    if (itr != m_templates.end())
    {
        return &itr->second;
    }
    return NULL;
}

void InstanceTemplateStore::Erase(uint32 map)
{
    m_templates.erase(map);
}

uint32 InstanceTemplateStore::GetCount() const
{
    return uint32(m_templates.size());
}

uint32 InstanceTemplateStore::GetMaxEntry() const
{
    if (m_templates.empty())
    {
        return 0;
    }
    return m_templates.rbegin()->first + 1;
}
