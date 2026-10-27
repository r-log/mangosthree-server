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

#include "data/GameObjectTemplateStore.h"

#include <cstring>

namespace
{
    char* CopyText(std::unique_ptr<char[]>& owned, std::string const& text)
    {
        owned.reset(new char[text.size() + 1]);
        std::memcpy(owned.get(), text.c_str(), text.size() + 1);
        return owned.get();
    }
}

void GameObjectTemplateStore::Clear()
{
    m_templates.clear();
}

void GameObjectTemplateStore::Set(GameObjectInfo const& info, std::string const& name, std::string const& iconName, std::string const& castBarCaption, std::string const& unk1)
{
    Entry& entry = m_templates[info.id];
    entry.info = info;
    entry.info.name = CopyText(entry.name, name);
    entry.info.IconName = CopyText(entry.iconName, iconName);
    entry.info.castBarCaption = CopyText(entry.castBarCaption, castBarCaption);
    entry.info.unk1 = CopyText(entry.unk1, unk1);
}

GameObjectInfo const* GameObjectTemplateStore::Find(uint32 entry) const
{
    TemplateMap::const_iterator itr = m_templates.find(entry);
    if (itr != m_templates.end())
    {
        return &itr->second.info;
    }
    return NULL;
}

GameObjectInfo* GameObjectTemplateStore::FindForEdit(uint32 entry)
{
    TemplateMap::iterator itr = m_templates.find(entry);
    if (itr != m_templates.end())
    {
        return &itr->second.info;
    }
    return NULL;
}

uint32 GameObjectTemplateStore::GetCount() const
{
    return uint32(m_templates.size());
}

uint32 GameObjectTemplateStore::GetMaxEntry() const
{
    if (m_templates.empty())
    {
        return 0;
    }
    return m_templates.rbegin()->first + 1;
}

GameObjectTemplateStore::const_iterator GameObjectTemplateStore::begin() const
{
    return const_iterator(m_templates.begin());
}

GameObjectTemplateStore::const_iterator GameObjectTemplateStore::end() const
{
    return const_iterator(m_templates.end());
}
