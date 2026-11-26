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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_PRIEST_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_PRIEST_H

#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/SpellScriptHooks.h"

#include <cstddef>
#include <vector>

/**
 * @file Priest.h
 * @brief The priest's spell scripts, registered by spell id in the spell handler registry.
 *
 * Priest.cpp holds RegisterPriestScripts and the list of the folder's mechanic files. Every other .cpp under this
 * folder, in a spec folder below it (discipline/, holy/, shadow/: the talent tree a talent sits in, the skill line a
 * baseline spell is taught by), is a mechanic file: <Mechanic>.cpp, its name unique within the class, defines
 * Priest::Register<Mechanic>, which registers the file's rows (a Row table per legacy site of SpellHandlerRegistry.h
 * through RegisterRows, and RegisterScript or RegisterRanks per script hook of SpellScriptHooks.h) and answers the rows
 * and defaults it registered. src/game/CMakeLists.txt lists the folder's mechanic files at configure time, in file-name
 * order, and RegisterPriestScripts calls each of them, so a new mechanic file needs no line in any other file. A
 * mechanic file declares its leaves in a header of its own, <Mechanic>.h, and includes this one for the helpers below.
 */

/// Registers every priest row: those of each mechanic file under the folder, in file-name order (Priest::ScriptFiles).
/// Returns the rows and defaults registered; a row whose key was taken does not change the table, so a caller that
/// finds fewer keys than rows has a duplicate.
uint32 RegisterPriestScripts(SpellHandlerRegistry& registry);

namespace Priest
{
    /// A row of a legacy site's table: a spell id and its body.
    template <class Site>
    struct Row
    {
        uint32 spellId;
        typename SpellHandler<Site>::Function function;
    };

    /// Registers every row of one site's table on `registry`; answers the number of rows.
    template <class Site, std::size_t N>
    uint32 RegisterRows(SpellHandlerRegistry& registry, Row<Site> const (&rows)[N])
    {
        for (Row<Site> const& row : rows)
        {
            registry.Register<Site>(row.spellId, row.function);
        }
        return uint32(N);
    }

    /// A mechanic file of the folder and the function that registers its rows.
    struct ScriptFile
    {
        char const* name;                                       ///< the file's name without ".cpp"
        uint32 (*registerRows)(SpellHandlerRegistry& registry); ///< Priest::Register<name>: its rows and defaults
    };

    /// Every mechanic file under the folder, by file name: each .cpp under spells/classes/priest but Priest.cpp, as
    /// src/game/CMakeLists.txt found them at configure time.
    std::vector<ScriptFile> const& ScriptFiles();

    /// Runs the registration of each of `files` on `registry`, in their order; answers the sum of what they answer.
    uint32 RegisterFiles(SpellHandlerRegistry& registry, std::vector<ScriptFile> const& files);
}

#endif
