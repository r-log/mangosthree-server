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

#include "data/SkillLevelSpellStore.h"

#include <algorithm>

std::array<uint32, 9> const& SkillLevelSpellStore::Allowlist()
{
    static std::array<uint32, 9> const spells =
    {
        2842,                                               // Poisons
        79327,                                              // Sinister Strike Enabler
        56816,                                              // Rune Strike
        84733,                                              // Holy Focus
        82945,                                              // Immolation Trap (Trap Launcher)
        60192,                                              // Freezing Trap (Trap Launcher)
        82939,                                              // Explosive Trap (Trap Launcher)
        82941,                                              // Ice Trap (Trap Launcher)
        82948,                                              // Snake Trap (Trap Launcher)
    };
    return spells;
}

bool SkillLevelSpellStore::IsAllowlisted(uint32 spellId)
{
    std::array<uint32, 9> const& spells = Allowlist();
    return std::find(spells.begin(), spells.end(), spellId) != spells.end();
}

void SkillLevelSpellStore::Clear()
{
    m_rows.clear();
}

bool SkillLevelSpellStore::Add(SkillLevelSpell const& row)
{
    if (!IsAllowlisted(row.spellId))
    {
        return false;
    }

    m_rows.push_back(row);
    return true;
}

std::vector<uint32> SkillLevelSpellStore::SpellsToLearn(uint32 raceMask, uint32 classMask, uint32 level,
                                                        std::function<bool(uint32)> const& hasSkillLine) const
{
    std::vector<uint32> spells;
    for (SkillLevelSpell const& row : m_rows)
    {
        if (row.raceMask && !(row.raceMask & raceMask))
        {
            continue;
        }

        if (row.classMask && !(row.classMask & classMask))
        {
            continue;
        }

        if (level < row.baseLevel || !hasSkillLine(row.skillLine))
        {
            continue;
        }

        if (std::find(spells.begin(), spells.end(), row.spellId) == spells.end())
        {
            spells.push_back(row.spellId);
        }
    }
    return spells;
}
