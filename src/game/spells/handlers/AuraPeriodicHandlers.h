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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_AURAPERIODICHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_AURAPERIODICHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"

class Aura;
class Unit;

/**
 * @file AuraPeriodicHandlers.h
 * @brief The `SpellAuraPeriodic.cpp` sites of the spell handler registry.
 *
 * The handlers are file-static functions in AuraPeriodicHandlers.cpp; this header holds what the registry and
 * tests see.
 */

/// What the bodies of `Aura::HandleAuraProcTriggerSpell`'s switch read: the aura, the target and
/// `apply`.
struct AuraProcTriggerContext
{
    AuraProcTriggerContext(Aura* thisAura, Unit*& targetLocal, bool const& applyParameter)
        : aura(thisAura), target(targetLocal), apply(applyParameter) {}

    Aura* const aura;  ///< `this` in the case bodies
    Unit*& target;     ///< the function's local `Unit* target = GetTarget();`
    bool const& apply; ///< the function's parameter `apply`, which no body writes
};

/// `Aura::HandleAuraProcTriggerSpell`, at a real apply or remove, its `switch (GetId())`: each label
/// continues after the switch; the `default:` does nothing.
struct AuraProcTriggerSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_PROC_TRIGGER;
    typedef void Value;
    typedef AuraProcTriggerContext Context;
};

/// What the bodies of `Aura::HandlePeriodicTriggerSpell`'s switch at remove read: the aura and the
/// target.
struct AuraPeriodicTriggerContext
{
    AuraPeriodicTriggerContext(Aura* thisAura, Unit*& targetLocal) : aura(thisAura), target(targetLocal) {}

    Aura* const aura; ///< `this` in the case bodies
    Unit*& target;    ///< the function's local `Unit* target = GetTarget();`
};

/// `Aura::HandlePeriodicTriggerSpell`, at remove, its `switch (GetId())`: each label returns from the
/// member; the `default:` continues after the switch.
struct AuraPeriodicTriggerSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_PERIODIC_TRIGGER;
    typedef void Value;
    typedef AuraPeriodicTriggerContext Context;
};

/// What the bodies of `Aura::HandlePeriodicEnergize`'s switch read and write: the aura (its modifier's
/// amount is written through it) and the target.
struct AuraPeriodicEnergizeContext
{
    AuraPeriodicEnergizeContext(Aura* thisAura, Unit*& targetLocal) : aura(thisAura), target(targetLocal) {}

    Aura* const aura; ///< `this` in the case bodies
    Unit*& target;    ///< the function's local `Unit* target = GetTarget();`
};

/// `Aura::HandlePeriodicEnergize`, at an apply that is not a load, its `switch (GetId())`: each label
/// sets the modifier's amount and continues after the switch; the `default:` does nothing.
struct AuraPeriodicEnergizeSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_PERIODIC_ENERGIZE;
    typedef void Value;
    typedef AuraPeriodicEnergizeContext Context;
};

/// What the body of `Aura::HandleAuraPeriodicDummy`'s rogue switch reads: the aura, the target and
/// `apply`.
struct AuraPeriodicDummyRogueContext
{
    AuraPeriodicDummyRogueContext(Aura* thisAura, Unit*& targetLocal, bool const& applyParameter)
        : aura(thisAura), target(targetLocal), apply(applyParameter) {}

    Aura* const aura;  ///< `this` in the case bodies
    Unit*& target;     ///< the function's local `Unit* target = GetTarget();`
    bool const& apply; ///< the function's parameter `apply`, which no body writes
};

/// `Aura::HandleAuraPeriodicDummy`, at a real apply or remove, SPELLFAMILY_ROGUE's
/// `switch(GetSpellProto()->ID)`: no `default:`; every label continues after the switch.
struct AuraPeriodicDummyRogueSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_PERIODIC_DUMMY_ROGUE;
    typedef void Value;
    typedef AuraPeriodicDummyRogueContext Context;
};

/// What the bodies of `Aura::HandleAuraModIncreaseHealth`'s switch read and write: the aura (its
/// modifier's amount is written through it), the target, `apply` and `Real`.
struct AuraIncreaseHealthContext
{
    AuraIncreaseHealthContext(Aura* thisAura, Unit*& targetLocal, bool const& applyParameter, bool const& realParameter)
        : aura(thisAura), target(targetLocal), apply(applyParameter), real(realParameter) {}

    Aura* const aura;  ///< `this` in the case bodies
    Unit*& target;     ///< the function's local `Unit* target = GetTarget();`
    bool const& apply; ///< the function's parameter `apply`, which no body writes
    bool const& real;  ///< the function's parameter `Real`, which no body writes
};

/// `Aura::HandleAuraModIncreaseHealth`, its `switch (GetId())`: the labels return from the member; the
/// `default:` applies the modifier as a flat health bonus and continues after the switch.
struct AuraIncreaseHealthSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_INCREASE_HEALTH;
    typedef void Value;
    typedef AuraIncreaseHealthContext Context;
};

/// Registers every `SpellAuraPeriodic.cpp` handler on `registry` (defined in AuraPeriodicHandlers.cpp).
/// Returns the number of rows it registered, a site's `default:` counting as one; a row whose key was taken,
/// or a second default at a site, does not change the table, so a caller that registers on an empty table
/// and finds fewer keys and defaults than rows has a duplicate.
uint32 RegisterAuraPeriodicHandlers(SpellHandlerRegistry& registry);

#endif
