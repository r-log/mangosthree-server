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
/// dereferenced), whether it is flat or a percentage, its amount (with its fraction) and its spell
/// class mask by address (read at each use, so a change to either while it is listed is seen), the
/// modifier operation, the spell family it applies to, whether the aura's spell is a mastery, and
/// the aura's spell and effect index, which name the modifier once its aura is gone (0 names none).
struct SpellModEntry
{
    Aura const* aura;
    bool flat;
    float const* amount;
    int32 op;
    uint32 family;
    ClassFamilyMask const* mask;
    bool mastery;
    uint32 spellId;
    uint8 effIndex;
};

/// A spell modifier a spell keeps for its own hits after the modifier left the list: the values of
/// its list entry when it left, the amount and the class mask copied.
struct KeptSpellMod
{
    uint32 spellId;
    uint8 effIndex;
    bool flat;
    float amount;
    int32 op;
    uint32 family;
    uint64 maskFlags;
    uint32 maskFlags2;
    bool mastery;
};

typedef std::vector<KeptSpellMod> KeptSpellMods;

/// One effect bit of a modifier's class mask and the sum the client is told for it.
struct SpellModValue
{
    uint8 effect;
    float value;
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
 * so a later write to the amount changes what the modifier adds. The amounts and their sums are
 * floats, so a mastery's percent keeps its fraction.
 *
 * Change adds or removes one modifier. Before the list changes, it computes what the client is
 * told (the sums per class mask bit with the change applied) and reports it through the sink it
 * is handed; an empty sink fails an assertion. ApplySpellMod applies the listed modifiers of an
 * operation that fit a spell to a value in place and returns the difference; with `masteryOnly`,
 * only those whose aura's spell is a mastery. The spell store is the one global it reads.
 *
 * A spell keeps the modifiers its cast end takes off the list: while a KeepScope lives, a removal
 * copies each removed modifier that fits the scope's spell and has an operation the spell reads
 * after its cast (ReadAfterTheCast) into the scope's list; while a KeptScope lives, ApplySpellMod
 * for the scope's spell applies that list as well, in place of any listed modifier of the same aura
 * spell and effect index.
 */
class SpellModMgr
{
    public:
        /// While it lives, the manager copies into `into` (emptied first) each modifier a removal
        /// takes off its list that spell `spellId` keeps (ReadAfterTheCast, its family and class
        /// mask), with its amount as it is then; the keeping before it comes back at its end. A
        /// null manager keeps nothing.
        class KeepScope
        {
            public:
                KeepScope(SpellModMgr* mgr, uint32 spellId, KeptSpellMods& into);
                ~KeepScope();

            private:
                KeepScope(KeepScope const&) = delete;
                KeepScope& operator=(KeepScope const&) = delete;

                SpellModMgr* m_mgr;
                uint32 m_previousSpell;
                KeptSpellMods* m_previousInto;
        };

        /// While it lives, ApplySpellMod for spell `spellId` also applies `kept`; the use before it
        /// comes back at its end. A null manager does nothing.
        class KeptScope
        {
            public:
                KeptScope(SpellModMgr* mgr, uint32 spellId, KeptSpellMods const& kept);
                ~KeptScope();

            private:
                KeptScope(KeptScope const&) = delete;
                KeptScope& operator=(KeptScope const&) = delete;

                SpellModMgr* m_mgr;
                uint32 m_previousSpell;
                KeptSpellMods const* m_previousKept;
        };

        void Change(SpellModEntry const& entry, bool apply, SpellModChangedSink const& report);
        template <class T> T ApplySpellMod(uint32 spellId, SpellModOp op, T& basevalue, bool masteryOnly = false);
        /// Forgets every modifier without telling the client, for a unit whose auras were deleted.
        void Clear();
        /// Whether a spell reads operation `op` after its cast has ended: at its hits, on each target,
        /// or on the auras its hits make, whose periodic amounts are fixed there.
        static bool ReadAfterTheCast(int32 op);

    private:
        void KeepRemoved(Aura const* aura, int32 op);

        std::list<SpellModEntry> m_mods[MAX_SPELLMOD];
        uint32 m_keepSpell = 0;
        KeptSpellMods* m_keepInto = NULL;
        uint32 m_keptSpell = 0;
        KeptSpellMods const* m_kept = NULL;
};

#endif
