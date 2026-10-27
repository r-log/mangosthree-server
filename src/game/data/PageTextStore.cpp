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

#include "data/PageTextStore.h"

#include <cstring>

void PageTextStore::Clear()
{
    m_pages.clear();
}

void PageTextStore::Set(uint32 id, std::string const& text, uint32 nextPage)
{
    Entry& entry = m_pages[id];
    entry.text.reset(new char[text.size() + 1]);
    std::memcpy(entry.text.get(), text.c_str(), text.size() + 1);
    entry.page.Page_ID = id;
    entry.page.Text = entry.text.get();
    entry.page.Next_Page = nextPage;
}

PageText const* PageTextStore::Find(uint32 id) const
{
    PageTextMap::const_iterator itr = m_pages.find(id);
    if (itr != m_pages.end())
    {
        return &itr->second.page;
    }
    return NULL;
}

PageText* PageTextStore::FindForEdit(uint32 id)
{
    PageTextMap::iterator itr = m_pages.find(id);
    if (itr != m_pages.end())
    {
        return &itr->second.page;
    }
    return NULL;
}

uint32 PageTextStore::GetCount() const
{
    return uint32(m_pages.size());
}

uint32 PageTextStore::GetMaxEntry() const
{
    if (m_pages.empty())
    {
        return 0;
    }
    return m_pages.rbegin()->first + 1;
}
