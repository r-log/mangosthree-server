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

#ifndef MANGOS_H_SKILLLEVELSPELLSTORE
#define MANGOS_H_SKILLLEVELSPELLSTORE

#include "Platform/Define.h"

#include <array>
#include <functional>
#include <vector>

/// A spell a character learns with a skill line once it reaches the spell's base level.
struct SkillLevelSpell
{
    uint32 spellId;
    uint32 skillLine;
    uint32 raceMask;                                        ///< 0: every race
    uint32 classMask;                                       ///< 0: every class
    uint32 baseLevel;
};

/**
 * The spells a class learns with its skill line at a level, limited to a fixed list of nine:
 * per row, the spell, a skill line it is learned with (SkillLineAbility.dbc), the races and
 * classes of that row, and the spell's base level (SpellLevels.dbc). A row for a spell off the
 * list is refused.
 */
class SkillLevelSpellStore
{
    public:
        static std::array<uint32, 9> const& Allowlist();
        static bool IsAllowlisted(uint32 spellId);

        void Clear();
        /// Adds the row; refuses it when its spell is off the list.
        bool Add(SkillLevelSpell const& row);
        std::vector<SkillLevelSpell> const& Rows() const { return m_rows; }

        /// The spells a character of these masks and level learns, each once: those of the rows
        /// whose race and class it matches, whose skill line `hasSkillLine` says it has, and whose
        /// base level it has reached.
        std::vector<uint32> SpellsToLearn(uint32 raceMask, uint32 classMask, uint32 level,
                                          std::function<bool(uint32)> const& hasSkillLine) const;

    private:
        std::vector<SkillLevelSpell> m_rows;
};

#endif
