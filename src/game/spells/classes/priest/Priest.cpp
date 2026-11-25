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

#include "spells/classes/priest/Priest.h"

namespace Priest
{
    // Each mechanic file's registration, Priest::Register<Mechanic> (PriestScriptFiles.inc, src/game/CMakeLists.txt).
#define PRIEST_SCRIPT_FILE(mechanic) uint32 Register##mechanic(SpellHandlerRegistry& registry);
#include "PriestScriptFiles.inc"
#undef PRIEST_SCRIPT_FILE
}

std::vector<Priest::ScriptFile> const& Priest::ScriptFiles()
{
    static std::vector<ScriptFile> const files =
    {
#define PRIEST_SCRIPT_FILE(mechanic) { #mechanic, &Priest::Register##mechanic },
#include "PriestScriptFiles.inc"
#undef PRIEST_SCRIPT_FILE
    };
    return files;
}

uint32 Priest::RegisterFiles(SpellHandlerRegistry& registry, std::vector<ScriptFile> const& files)
{
    uint32 registered = 0;
    for (ScriptFile const& file : files)
    {
        registered += file.registerRows(registry);
    }
    return registered;
}

uint32 RegisterPriestScripts(SpellHandlerRegistry& registry)
{
    return Priest::RegisterFiles(registry, Priest::ScriptFiles());
}
