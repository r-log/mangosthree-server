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

#ifndef MANGOS_H_SERVERSPELLSTORE
#define MANGOS_H_SERVERSPELLSTORE

#include "Common/Locales.h"
#include "DBCStructure.h"
#include "Platform/Define.h"

#include <map>
#include <string>

/**
 * A spell the client's spell data does not hold: its main record, its one effect and its aura
 * options, all zeroed until filled. `SpellEntry` cannot be constructed or copied, so the main record
 * is built in place in zeroed storage, as the DBC loader builds its records.
 */
class ServerSpell
{
    public:
        ServerSpell();

        SpellEntry& Entry();
        SpellEntry const& Entry() const;

        SpellEffectEntry effect;
        bool hasEffect;
        SpellAuraOptionsEntry auraOptions;

    private:
        ServerSpell(ServerSpell const&);
        ServerSpell& operator=(ServerSpell const&);

        char m_entry[sizeof(SpellEntry)];
};

/**
 * The server-side spells (`spell_template`), one per id. The store owns each spell's records and its
 * name, given in every locale with no subtext. The records keep their addresses while the store
 * lives, so the spell stores can point at them. Adding an id the store holds replaces its spell.
 */
class ServerSpellStore
{
    public:
        /// Adds the spell `id`, its records zeroed but for the id and the name `name`, and answers it
        /// for the loader to fill.
        ServerSpell& Add(uint32 id, std::string const& name);
        ServerSpell const* Find(uint32 id) const;
        uint32 GetCount() const;

    private:
        struct Entry
        {
            ServerSpell spell;
            std::string name;
            char const* names[MAX_LOCALE];
            char const* subtexts[MAX_LOCALE];
        };

        typedef std::map<uint32, Entry> ServerSpellMap;

        ServerSpellMap m_spells;
};

#endif
