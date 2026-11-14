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

#ifndef MANGOSSERVER_SPELLS_SPELLSCRIPTQUERIES_H
#define MANGOSSERVER_SPELLS_SPELLSCRIPTQUERIES_H

#include "Unit.h"                                       // SpellEffectIndex, ObjectGuid

#include <cstddef>
#include <vector>

/**
 * @file SpellScriptQueries.h
 * @brief What a spell script reads of the auras on a unit, and copies between units, without the aura headers.
 *
 * The bodies sit in spells/auras/SpellScriptQueries.cpp, beside the aura holders they read.
 */

/// One periodic damage effect of a holder on a unit, as a script reads it.
struct PeriodicDamageView
{
    uint32 spellId;                                 ///< the holder's spell
    SpellEffectIndex effIndex;                      ///< the periodic damage effect
    int32 amount;                                   ///< the damage of one tick, as the aura holds it
    uint32 periodMs;                                ///< the time between ticks, as the aura holds it
    int32 remainingMs;                              ///< the holder's time left; -1 when it does not run out
    uint32 remainingTicks;                          ///< the ticks left
};

/// Fills `out`, cleared first, with every periodic damage effect (SPELL_AURA_PERIODIC_DAMAGE) that `caster` has on
/// `target` among the `count` spells of `spellIds`, in that order, and for each spell in the order of its holders and
/// effects; answers how many were found.
size_t FindPeriodicDamage(Unit const& target, ObjectGuid const& caster, uint32 const* spellIds, size_t count,
                          std::vector<PeriodicDamageView>& out);

/// Puts on `to` a copy of the holder of `spellId` that `caster` has on `from`: the same caster, stack and charges, each
/// aura made from the source aura's base points with the source's period and time to its next tick, and the time the
/// source has left as both the copy's duration and its full duration, so the copy's tick count is the ticks the source
/// has left. A periodic aura of the copy starts from the source aura's amount before its caster's bonus (a rolled-over
/// bank included), so a later re-application carries what the source would. The copy is added as a new holder
/// (Unit::AddSpellAuraHolder), so the stacking and target limit rules apply; then each periodic aura of it takes the
/// source aura's amount, which already holds the caster's bonus, so the apply's own bonus is not taken twice. False,
/// and nothing done, when `from` holds none, when its caster cannot be found, when `to` already holds that caster's, or
/// when `to` refuses it.
bool CopyAuraHolder(Unit& from, Unit& to, uint32 spellId, ObjectGuid const& caster);

#endif
