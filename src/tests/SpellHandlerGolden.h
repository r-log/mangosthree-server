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

#ifndef MANGOS_TESTS_SPELLHANDLERGOLDEN_H
#define MANGOS_TESTS_SPELLHANDLERGOLDEN_H

#include "spells/handlers/SpellHandlerRegistry.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifndef MANGOS_TEST_DATA_DIR
#error "MANGOS_TEST_DATA_DIR must name src/tests/goldens (see src/tests/CMakeLists.txt)"
#endif

/**
 * The counts the spell handler tests derive from goldens/spell_handler_keys.txt instead of writing by hand.
 *
 * The golden holds one "<site> <spell id>" line per key of the game's table, in site then spell id order, and
 * SpellHandlerRegistry_TheGameTableMatchesItsGolden holds the game's table to it line by line. A key a PR adds is one
 * line there; every count below follows from the file, so no test names the number of keys at a site or a hook.
 */
namespace SpellHandlerGolden
{
    /// Every key of the golden, in its order; empty when the file cannot be read, which the golden test reports.
    inline std::vector<std::pair<uint32, uint32> > const& Keys()
    {
        static std::vector<std::pair<uint32, uint32> > const keys = []()
        {
            std::vector<std::pair<uint32, uint32> > read;
            std::ifstream golden((std::string(MANGOS_TEST_DATA_DIR) + "/spell_handler_keys.txt").c_str());
            std::string text;
            while (std::getline(golden, text))
            {
                std::istringstream line(text);
                uint32 site = 0;
                uint32 spellId = 0;
                if (line >> site >> spellId)
                {
                    read.push_back(std::make_pair(site, spellId));
                }
            }
            return read;
        }();
        return keys;
    }

    /// The golden's keys at `site`, a legacy site or a script hook.
    inline std::size_t CountAt(uint32 site)
    {
        std::size_t count = 0;
        for (std::pair<uint32, uint32> const& key : Keys())
        {
            count += key.first == site ? 1 : 0;
        }
        return count;
    }

    /// Whether the golden holds (`site`, `spellId`).
    inline bool Holds(std::pair<uint32, uint32> const& key)
    {
        return std::binary_search(Keys().begin(), Keys().end(), key);
    }

    /// The class scripts' rows (RegisterClassScripts: the mage's, the haste spells' and the priest's) on a table of
    /// their own, built once.
    inline SpellHandlerRegistry const& ClassScripts()
    {
        static SpellHandlerRegistry const classes = []()
        {
            SpellHandlerRegistry built;
            RegisterClassScripts(built);
            return built;
        }();
        return classes;
    }

    /// The class scripts' keys at `site` that `table` does not hold.
    inline std::size_t ClassRowsBeyond(SpellHandlerRegistry const& table, uint32 site)
    {
        std::vector<std::pair<uint32, uint32> > const held = table.Keys();
        std::size_t rows = 0;
        for (std::pair<uint32, uint32> const& key : ClassScripts().Keys())
        {
            if (key.first == site && !std::binary_search(held.begin(), held.end(), key))
            {
                ++rows;
            }
        }
        return rows;
    }

    /// The rows the game's table holds at `site` when `table` holds the rows of the legacy file whose site it is: that
    /// file's and those the class scripts add there. A row there of any other file makes the game's count larger.
    inline std::size_t RowsWithClassScripts(SpellHandlerRegistry const& table, uint32 site)
    {
        return table.CountAt(site) + ClassRowsBeyond(table, site);
    }
}

#endif
