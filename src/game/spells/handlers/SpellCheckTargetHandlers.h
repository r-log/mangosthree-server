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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_SPELLCHECKTARGETHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_SPELLCHECKTARGETHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"

class Unit;

/**
 * @file SpellCheckTargetHandlers.h
 * @brief The `Spell::CheckTarget` site of the spell handler registry.
 *
 * The handlers are file-static functions in SpellCheckTargetHandlers.cpp; this header holds what the
 * registry and tests see.
 */

/// What the bodies of `Spell::CheckTarget`'s switch read: the spell's caster and the target checked.
struct SpellCheckTargetContext
{
    SpellCheckTargetContext(Unit*& casterMember, Unit* const& targetParameter)
        : m_caster(casterMember), target(targetParameter) {}

    Unit*& m_caster;        ///< the spell's member `Unit* m_caster;`
    Unit* const& target;    ///< the function's parameter `Unit* target`, which no body writes
};

/// `Spell::CheckTarget`, after the player-only attribute check: its `switch (m_spellInfo->ID)`. A label
/// refuses the target or continues; the `default:` continues; the check goes on after the switch.
struct SpellCheckTargetSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_CHECK_TARGET;
    typedef bool Value;
    typedef SpellCheckTargetContext Context;
};

/// Registers every `Spell::CheckTarget` handler on `registry` (defined in SpellCheckTargetHandlers.cpp).
/// Returns the number of rows it registered, the site's `default:` counting as one; a row whose key was
/// taken, or a second default, does not change the table, so a caller that registers on an empty table and
/// finds fewer keys and defaults than rows has a duplicate.
uint32 RegisterSpellCheckTargetHandlers(SpellHandlerRegistry& registry);

#endif
