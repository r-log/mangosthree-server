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

#include "data/CreatureAddonStore.h"

void CreatureAddonStore::Clear()
{
    m_addons.clear();
}

void CreatureAddonStore::Set(CreatureDataAddon const& addon)
{
    Entry& entry = m_addons[addon.guidOrEntry];
    entry.auras.clear();
    entry.addon = addon;
    entry.addon.auras = NULL;
}

void CreatureAddonStore::SetAuras(uint32 key, std::vector<uint32> const& aurasWithTerminator)
{
    AddonMap::iterator itr = m_addons.find(key);
    if (itr == m_addons.end())
    {
        return;
    }
    itr->second.auras = aurasWithTerminator;
    itr->second.addon.auras = itr->second.auras.empty() ? NULL : itr->second.auras.data();
}

CreatureDataAddon const* CreatureAddonStore::Find(uint32 key) const
{
    AddonMap::const_iterator itr = m_addons.find(key);
    if (itr != m_addons.end())
    {
        return &itr->second.addon;
    }
    return NULL;
}

CreatureDataAddon* CreatureAddonStore::FindForEdit(uint32 key)
{
    AddonMap::iterator itr = m_addons.find(key);
    if (itr != m_addons.end())
    {
        return &itr->second.addon;
    }
    return NULL;
}

uint32 CreatureAddonStore::GetCount() const
{
    return uint32(m_addons.size());
}

uint32 CreatureAddonStore::GetMaxEntry() const
{
    if (m_addons.empty())
    {
        return 0;
    }
    return m_addons.rbegin()->first + 1;
}
