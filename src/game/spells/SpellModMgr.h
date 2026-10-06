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

#ifndef MANGOS_H_SPELLMODMGR
#define MANGOS_H_SPELLMODMGR

#include "Platform/Define.h"
#include "SharedDefines.h"
#include <functional>
#include <list>
#include <vector>

class Aura;
struct ClassFamilyMask;

/// One spell modifier on a list: the aura it comes from (an identity for the removal, never
/// dereferenced), whether it is flat or a percentage, its amount and its spell class mask by
/// address (read at each use, so a change to either while it is listed is seen), the modifier
/// operation and the spell family it applies to.
struct SpellModEntry
{
    Aura const* aura;
    bool flat;
    int32 const* amount;
    int32 op;
    uint32 family;
    ClassFamilyMask const* mask;
};

/// One effect bit of a modifier's class mask and the sum the client is told for it.
struct SpellModValue
{
    uint8 effect;
    int32 value;
};

/// A modifier added or removed: whether the sums are flat or percentages, the operation, and
/// for each bit of the modifier's class mask, in bit order, the sum of the listed modifiers of
/// the same kind holding that bit, with the change applied.
struct SpellModChangedFact
{
    bool flat;
    uint8 op;
    std::vector<SpellModValue> values;
};

/// Reports a SpellModChangedFact; the owner turns it into the client's packet.
typedef std::function<void(SpellModChangedFact const& fact)> SpellModChangedSink;

/**
 * @brief A character's spell modifiers and the rules over them, held apart from the object that
 * plays the character.
 *
 * The state is one list of modifiers per modifier operation, in the order they were added. An
 * entry holds the addresses of its aura's amount and class mask, and both are read at each use,
 * so a later write to the amount (the mastery update rewrites it in place) changes what the
 * modifier adds.
 *
 * Change adds or removes one modifier. Before the list changes, it computes what the client is
 * told (the sums per class mask bit with the change applied) and reports it through the sink it
 * is handed; an empty sink fails an assertion. ApplySpellMod applies the listed modifiers of an
 * operation that fit a spell to a value in place and returns the difference. The spell store is
 * the one global it reads.
 */
class SpellModMgr
{
    public:
        void Change(SpellModEntry const& entry, bool apply, SpellModChangedSink const& report);
        template <class T> T ApplySpellMod(uint32 spellId, SpellModOp op, T& basevalue);

    private:
        std::list<SpellModEntry> m_mods[MAX_SPELLMOD];
};

#endif
