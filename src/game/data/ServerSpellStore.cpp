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

#include "data/ServerSpellStore.h"

#include <cstring>

ServerSpell::ServerSpell() : effect(), hasEffect(false), auraOptions()
{
    std::memset(m_entry, 0, sizeof(m_entry));
}

SpellEntry& ServerSpell::Entry()
{
    return *reinterpret_cast<SpellEntry*>(m_entry);
}

SpellEntry const& ServerSpell::Entry() const
{
    return *reinterpret_cast<SpellEntry const*>(m_entry);
}

ServerSpell& ServerSpellStore::Add(uint32 id, std::string const& name)
{
    m_spells.erase(id);
    Entry& stored = m_spells[id];
    stored.name = name;
    for (uint32 i = 0; i < MAX_LOCALE; ++i)
    {
        stored.names[i] = stored.name.c_str();
        stored.subtexts[i] = "";
    }
    SpellEntry& entry = stored.spell.Entry();
    entry.ID = id;
    entry.Name_lang = stored.names;
    entry.NameSubtext_lang = stored.subtexts;
    return stored.spell;
}

ServerSpell const* ServerSpellStore::Find(uint32 id) const
{
    ServerSpellMap::const_iterator itr = m_spells.find(id);
    if (itr != m_spells.end())
    {
        return &itr->second.spell;
    }
    return NULL;
}

uint32 ServerSpellStore::GetCount() const
{
    return uint32(m_spells.size());
}
