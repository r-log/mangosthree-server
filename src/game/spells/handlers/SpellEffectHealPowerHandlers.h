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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_SPELLEFFECTHEALPOWERHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_SPELLEFFECTHEALPOWERHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"

class Unit;

/**
 * @file SpellEffectHealPowerHandlers.h
 * @brief The `Spell::EffectEnergize` site of the spell handler registry.
 *
 * The handlers are file-static functions in SpellEffectHealPowerHandlers.cpp; this header holds what the registry
 * and tests see.
 */

/// What the bodies of `Spell::EffectEnergize`'s level-dependent switch read and write: the spell's caster and
/// target, the amount, and the two locals the member shrinks the amount by after the switch.
struct SpellEffectEnergizeContext
{
    SpellEffectEnergizeContext(Unit*& casterMember, Unit*& targetMember, int32& damageMember, int& levelDiffLocal,
                               int& levelMultiplierLocal)
        : m_caster(casterMember), unitTarget(targetMember), damage(damageMember), level_diff(levelDiffLocal),
          level_multiplier(levelMultiplierLocal) {}

    Unit*& m_caster;            ///< the spell's member `Unit* m_caster;`
    Unit*& unitTarget;          ///< the spell's member `Unit* unitTarget;`
    int32& damage;              ///< the spell's member `int32 damage;`, the amount, read after the switch
    int& level_diff;            ///< the function's local `int level_diff = 0;`, read after the switch
    int& level_multiplier;      ///< the function's local `int level_multiplier = 0;`, read after the switch
};

/// `Spell::EffectEnergize`, its `switch (m_spellInfo->ID)` before the amount is shrunk by level: the labels set
/// the level shrink or change the amount; the `default:` changes nothing; all continue after the switch.
struct SpellEffectEnergizeSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_ENERGIZE;
    typedef void Value;
    typedef SpellEffectEnergizeContext Context;
};

/// Registers every `Spell::EffectEnergize` handler on `registry` (defined in SpellEffectHealPowerHandlers.cpp).
/// Returns the number of rows it registered, the site's `default:` counting as one; a row whose key was
/// taken, or a second default, does not change the table, so a caller that registers on an empty table and
/// finds fewer keys and defaults than rows has a duplicate.
uint32 RegisterSpellEffectHealPowerHandlers(SpellHandlerRegistry& registry);

#endif
