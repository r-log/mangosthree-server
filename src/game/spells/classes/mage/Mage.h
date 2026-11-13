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

#ifndef MANGOSSERVER_SPELLS_CLASSES_MAGE_MAGE_H
#define MANGOSSERVER_SPELLS_CLASSES_MAGE_MAGE_H

#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/SpellScriptHooks.h"

#include <cstddef>

/**
 * @file Mage.h
 * @brief The mage's spell scripts, registered by spell id in the spell handler registry.
 *
 * Mage.cpp holds the class-wide bodies, the mage sites' defaults, and ProcEffectIs and CastProcSpell, which the proc
 * scripts of Fire.cpp and Arcane.cpp share; Fire.cpp, Frost.cpp and Arcane.cpp hold the bodies of the three
 * specializations, and Fire.cpp also CountCritInARow, Hot Streak's count. Each file registers its rows through one
 * function of namespace Mage: a table per legacy site of SpellHandlerRegistry.h, and RegisterRanks per script hook.
 */

/// Registers every mage row: the class-wide bodies and those of the three specializations. Returns the rows and
/// defaults registered; a row whose key was taken does not change the table, so a caller that finds fewer keys than
/// rows has a duplicate.
uint32 RegisterMageScripts(SpellHandlerRegistry& registry);

namespace Mage
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

    /// Registers the class-wide rows and the defaults of the mage trigger-spell, periodic dummy tick and boost sites.
    /// Returns the rows and defaults registered.
    uint32 RegisterClassWide(SpellHandlerRegistry& registry);
    /// Registers the Fire specialization's rows. Returns the rows registered.
    uint32 RegisterFire(SpellHandlerRegistry& registry);
    /// Registers the Frost specialization's rows. Returns the rows registered.
    uint32 RegisterFrost(SpellHandlerRegistry& registry);
    /// Registers the Arcane specialization's rows. Returns the rows registered.
    uint32 RegisterArcane(SpellHandlerRegistry& registry);

    /// Whether the aura effect a proc runs for is of `auraType`. A proc script runs for every effect of its spell that
    /// passed the proc's class-mask checks; it acts for the effects of the aura type whose proc handler it stands for,
    /// and lets the handler of any other run. Defined in Mage.cpp.
    bool ProcEffectIs(OnProcContext const& ctx, uint32 auraType);

    /// Counts a hit into `crits`, the crits in a row so far: a crit adds one, any other hit sets it to 0. True, with
    /// `crits` set back to 0, when the hit is the second crit in a row. Hot Streak's count; defined in Fire.cpp.
    bool CountCritInARow(int32& crits, bool crit);

    /// The end of a proc that casts `spellId` at `to`, as Unit::HandleDummyAuraProc ends: FAILED, and an error line,
    /// when the spell store has no `spellId`; otherwise OK when ctx.CastTriggered cast it, FAILED when it refused.
    /// `basePoints`, when given, holds MAX_EFFECT_INDEX values; a zero keeps that effect's own value. Defined in
    /// Mage.cpp.
    SpellAuraProcResult CastProcSpell(OnProcContext& ctx, Unit& to, uint32 spellId, int32 const* basePoints = NULL);
}

#endif
