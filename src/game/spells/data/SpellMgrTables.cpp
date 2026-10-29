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

#include "spells/data/SpellMgr.h"

#include <algorithm>
#include <cstddef>

namespace
{
    /// A spell id and the spell it answers.
    struct SpellAnswer
    {
        uint32 spellId;
        uint32 answer;
    };

    /// Whether each row's spell id is greater than the one before it: sorted and unique.
    template <std::size_t N>
    constexpr bool SortedBySpellId(SpellAnswer const (&rows)[N])
    {
        for (std::size_t i = 1; i < N; ++i)
        {
            if (!(rows[i - 1].spellId < rows[i].spellId))
            {
                return false;
            }
        }
        return true;
    }

    template <std::size_t N>
    constexpr bool SortedBySpellId(uint32 const (&ids)[N])
    {
        for (std::size_t i = 1; i < N; ++i)
        {
            if (!(ids[i - 1] < ids[i]))
            {
                return false;
            }
        }
        return true;
    }

    /// The answer of the row holding `spellId`, or `none` when no row holds it.
    template <std::size_t N>
    uint32 AnswerOf(SpellAnswer const (&rows)[N], uint32 spellId, uint32 none)
    {
        SpellAnswer const* end = rows + N;
        SpellAnswer const* row = std::lower_bound(rows, end, spellId,
                                                  [](SpellAnswer const& r, uint32 id) { return r.spellId < id; });
        return row != end && row->spellId == spellId ? row->answer : none;
    }

    /// The dummy spells goober templates name, and the spells cast in their place.
    constexpr SpellAnswer GOOBER_CAST_SPELLS[] =
    {
        { 34448, 26566 },   // Orb of Translocation
        { 34452, 26572 },   // Orb of Translocation
        { 37639, 36326 },   // Nether Drake Egg
        { 45367, 45371 },   // Orb of Translocation
        { 45370, 45368 },   // Orb of Translocation
    };

    /// Ferocious Inspiration's talent rank, and the aura the pet casts on itself for it.
    constexpr SpellAnswer FEROCIOUS_INSPIRATION_PET_AURAS[] =
    {
        { 34460, 75447 },   // Ferocious Inspiration: 3%
    };

    /// Mastery proc auras whose own case in HandleProcTriggerSpellAuraProc or HandleDummyAuraProc decides their procs.
    constexpr uint32 CORE_HANDLED_MASTERY_PROCS[] =
    {
        76659,  // Wild Quiver (Marksmanship Hunter)
        76669,  // Illuminated Healing (Holy Paladin)
        76672,  // Hand of Light (Retribution Paladin)
        76838,  // Strikes of Opportunity (Arms Warrior; its dummy-aura case is under SPELLFAMILY_ROGUE, the DBC's family)
        77222,  // Elemental Overload (Elemental Shaman)
    };

    /// Soulstone Resurrection's aura, and the resurrection it grants.
    constexpr SpellAnswer SOULSTONE_RESURRECTIONS[] =
    {
        { 20707, 3026 },
    };

    static_assert(SortedBySpellId(GOOBER_CAST_SPELLS), "goober rows are sorted by spell id, each once");
    static_assert(SortedBySpellId(FEROCIOUS_INSPIRATION_PET_AURAS), "Ferocious Inspiration rows are sorted, each once");
    static_assert(SortedBySpellId(CORE_HANDLED_MASTERY_PROCS), "mastery proc ids are sorted, each once");
    static_assert(SortedBySpellId(SOULSTONE_RESURRECTIONS), "Soulstone rows are sorted by spell id, each once");
}

uint32 GetGooberCastSpell(uint32 templateSpellId)
{
    return AnswerOf(GOOBER_CAST_SPELLS, templateSpellId, templateSpellId);
}

uint32 GetFerociousInspirationPetAura(uint32 talentRankSpellId)
{
    return AnswerOf(FEROCIOUS_INSPIRATION_PET_AURAS, talentRankSpellId, 0);
}

bool IsCoreHandledMasteryProc(uint32 auraSpellId)
{
    return std::binary_search(std::begin(CORE_HANDLED_MASTERY_PROCS), std::end(CORE_HANDLED_MASTERY_PROCS), auraSpellId);
}

uint32 GetSoulstoneResurrectionSpell(uint32 auraSpellId)
{
    return AnswerOf(SOULSTONE_RESURRECTIONS, auraSpellId, 0);
}
