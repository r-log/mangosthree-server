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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_AURADUMMYHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_AURADUMMYHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"

class Aura;
class Unit;

/**
 * @file AuraDummyHandlers.h
 * @brief The `Aura::HandleAuraDummy` sites of the spell handler registry.
 *
 * The handlers are file-static functions in AuraDummyHandlers.cpp; this header holds what the registry and tests see.
 */

/// The live-out locals of `Aura::HandleAuraDummy` at an AT APPLY family switch. Every name a
/// registered body reads or writes that is declared outside the case body is here, by reference
/// where the function could write it.
struct AuraDummyApplyContext
{
    AuraDummyApplyContext(Aura* thisAura, Unit*& targetLocal) : aura(thisAura), target(targetLocal) {}

    Aura* const aura;   ///< `this` in the case bodies
    Unit*& target;      ///< the function's local `Unit* target = GetTarget();`
};

/// `Aura::HandleAuraDummy`, AT APPLY, SPELLFAMILY_WARRIOR's `switch (GetId())`: no `default:`,
/// so a miss continues at the Overpower block after it.
struct AuraDummyApplyWarriorSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_DUMMY_APPLY_WARRIOR;
    typedef void Value;
    typedef AuraDummyApplyContext Context;
};

/// The live-out local of the quest-tame switch: each label sets `finalSpellId`, which the code after it casts.
struct AuraDummyQuestTameContext
{
    explicit AuraDummyQuestTameContext(uint32& finalSpellIdLocal) : finalSpellId(finalSpellIdLocal) {}

    uint32& finalSpellId;   ///< the block's local `uint32 finalSpellId = 0;`
};

/// `Aura::HandleAuraDummy`, AT REMOVE, the quest-tame `switch (GetId())`: no `default:`; a miss casts nothing.
struct AuraDummyQuestTameSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_DUMMY_QUEST_TAME;
    typedef void Value;
    typedef AuraDummyQuestTameContext Context;
};

/// The live-out locals of `Aura::HandleAuraDummy` at its AT REMOVE switch: the aura and `target`.
struct AuraDummyRemoveContext
{
    AuraDummyRemoveContext(Aura* thisAura, Unit*& targetLocal) : aura(thisAura), target(targetLocal) {}

    Aura* const aura;   ///< `this` in the case bodies
    Unit*& target;      ///< the function's local `Unit* target = GetTarget();`
};

/// `Aura::HandleAuraDummy`, AT REMOVE `switch (GetId())`: a miss runs the switch that still holds the other labels.
struct AuraDummyRemoveSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_DUMMY_REMOVE;
    typedef void Value;
    typedef AuraDummyRemoveContext Context;
};

/// The live-out locals of `Aura::HandleAuraDummy` at an AT APPLY & REMOVE family switch: the
/// bodies there read `apply` as well as `target` and the aura.
struct AuraDummyApplyRemoveContext
{
    AuraDummyApplyRemoveContext(Aura* thisAura, Unit*& targetLocal, bool applyParameter)
        : aura(thisAura), target(targetLocal), apply(applyParameter) {}

    Aura* const aura;   ///< `this` in the case bodies
    Unit*& target;      ///< the function's local `Unit* target = GetTarget();`
    bool const apply;   ///< the function's parameter `apply`, which no body writes
};

/// `Aura::HandleAuraDummy`, AT APPLY & REMOVE, SPELLFAMILY_GENERIC's `switch (GetId())`: no `default:`; a miss
/// runs the switch that still holds the other labels.
struct AuraDummyApplyRemoveGenericSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_DUMMY_APPLY_REMOVE_GENERIC;
    typedef void Value;
    typedef AuraDummyApplyRemoveContext Context;
};

/// `Aura::HandleAuraDummy`, AT APPLY & REMOVE, SPELLFAMILY_DRUID's `switch (GetId())`: no
/// `default:`, so a miss continues at the Lifebloom and Predatory Strikes blocks after it.
struct AuraDummyDruidSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_DUMMY_DRUID;
    typedef void Value;
    typedef AuraDummyApplyRemoveContext Context;
};

/// Registers every `Aura::HandleAuraDummy` handler on `registry` (defined in AuraDummyHandlers.cpp).
/// Returns the number of rows it registered, a site's `default:` counting as one; a row whose key
/// was taken, or a second default at a site, does not change the table, so a caller that registers
/// on an empty table and finds fewer keys and defaults than rows has a duplicate.
uint32 RegisterAuraDummyHandlers(SpellHandlerRegistry& registry);

#endif
