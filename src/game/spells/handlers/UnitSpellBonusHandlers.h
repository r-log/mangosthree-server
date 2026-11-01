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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_UNITSPELLBONUSHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_UNITSPELLBONUSHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"
#include "Object/Unit.h"
#include <list>

class Aura;
struct SpellEntry;

/**
 * @file UnitSpellBonusHandlers.h
 * @brief The three dummy-aura sites of `Unit`'s spell and melee bonus functions in the spell handler registry.
 *
 * The handlers are file-static functions in UnitSpellBonusHandlers.cpp; this header holds what the registry and
 * tests see.
 */

/// What the bodies of `Unit::SpellDamageBonusTaken`'s dummy-aura switch read: the unit, the spell, the
/// multiplier they scale and the aura the loop stands at. Built, it checks that this thread owns the
/// unit (`Unit::AssertSpellSeamOwner`); a context with no unit checks nothing.
struct SpellDamageTakenContext
{
    SpellDamageTakenContext(Unit* thisUnit, SpellEntry const* const& spellProtoParameter, float& takenTotalModLocal,
                            std::list<Aura*>::const_iterator const& auraIterator)
        : unit(thisUnit), spellProto(spellProtoParameter), TakenTotalMod(takenTotalModLocal), i(auraIterator)
    {
        if (unit)
        {
            unit->AssertSpellSeamOwner();
        }
    }

    Unit* const unit;                               ///< `this` in the case bodies
    SpellEntry const* const& spellProto;            ///< the function's parameter `SpellEntry const* spellProto`
    float& TakenTotalMod;                           ///< the function's local `float TakenTotalMod`, which bodies set
    std::list<Aura*>::const_iterator const& i;      ///< the loop's iterator over the unit's dummy auras
};

/// `Unit::SpellDamageBonusTaken`, the loop over the dummy auras: its `switch ((*i)->GetId())`. A label scales the
/// multiplier or takes the loop's next aura; a miss goes on to the loop's next aura.
struct SpellDamageTakenSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_UNIT_SPELL_DAMAGE_TAKEN;
    typedef void Value;
    typedef SpellDamageTakenContext Context;
};

/// What the bodies of `Unit::SpellHealingBonusDone`'s Lifebloom dummy-aura switch read: the unit, the flat bonus
/// they add to and the aura the loop stands at. Built, it checks that this thread owns the unit
/// (`Unit::AssertSpellSeamOwner`); a context with no unit checks nothing.
struct SpellHealingDoneContext
{
    SpellHealingDoneContext(Unit* thisUnit, int32& doneTotalLocal, std::list<Aura*>::const_iterator const& auraIterator)
        : unit(thisUnit), DoneTotal(doneTotalLocal), i(auraIterator)
    {
        if (unit)
        {
            unit->AssertSpellSeamOwner();
        }
    }

    Unit* const unit;                               ///< `this` in the case bodies
    int32& DoneTotal;                               ///< the function's local `int32 DoneTotal`, which bodies set
    std::list<Aura*>::const_iterator const& i;      ///< the loop's iterator over the owner's dummy auras
};

/// `Unit::SpellHealingBonusDone`, Lifebloom, the loop over the owner's dummy auras: its `switch ((*i)->GetId())`.
/// A label adds to the flat bonus; a miss goes on to the loop's next aura.
struct SpellHealingDoneSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_UNIT_SPELL_HEALING_DONE;
    typedef void Value;
    typedef SpellHealingDoneContext Context;
};

/// What the bodies of `Unit::MeleeDamageBonusTaken`'s dummy-aura switch read: the unit, the percentage they
/// scale and the aura the loop stands at. Built, it checks that this thread owns the unit
/// (`Unit::AssertSpellSeamOwner`); a context with no unit checks nothing.
struct MeleeDamageTakenContext
{
    MeleeDamageTakenContext(Unit* thisUnit, float& takenPercentLocal,
                            std::list<Aura*>::const_iterator const& auraIterator)
        : unit(thisUnit), TakenPercent(takenPercentLocal), i(auraIterator)
    {
        if (unit)
        {
            unit->AssertSpellSeamOwner();
        }
    }

    Unit* const unit;                               ///< `this` in the case bodies
    float& TakenPercent;                            ///< the function's local `float TakenPercent`, which bodies set
    std::list<Aura*>::const_iterator const& i;      ///< the loop's iterator over the unit's dummy auras
};

/// `Unit::MeleeDamageBonusTaken`, the loop over the dummy auras: its `switch ((*i)->GetId())`. A label scales the
/// percentage or takes the loop's next aura; a miss goes on to the loop's next aura.
struct MeleeDamageTakenSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_UNIT_MELEE_DAMAGE_TAKEN;
    typedef void Value;
    typedef MeleeDamageTakenContext Context;
};

/// Registers every handler of the three dummy-aura sites on `registry` (defined in UnitSpellBonusHandlers.cpp).
/// Returns the number of rows it registered; a row whose key was taken does not change the table, so a caller
/// that registers on an empty table and finds fewer keys than rows has a duplicate.
uint32 RegisterUnitSpellBonusHandlers(SpellHandlerRegistry& registry);

#endif
