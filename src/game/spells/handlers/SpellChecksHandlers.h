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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_SPELLCHECKSHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_SPELLCHECKSHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"
#include "Server/SharedDefines.h"

class Unit;

/**
 * @file SpellChecksHandlers.h
 * @brief The `Spell::CheckCast` site of the spell handler registry.
 *
 * The handlers are file-static functions in SpellChecksHandlers.cpp; this header holds what the registry
 * and tests see.
 */

/// What the bodies of `Spell::CheckCast`'s SPELL_AURA_DUMMY switch read: the spell's caster.
struct SpellCheckCastAuraDummyContext
{
    explicit SpellCheckCastAuraDummyContext(Unit*& casterMember) : m_caster(casterMember) {}

    Unit*& m_caster;    ///< the spell's member `Unit* m_caster;`
};

/// `Spell::CheckCast`, an effect that applies SPELL_AURA_DUMMY: its `switch (m_spellInfo->ID)`. A label fails
/// the cast with its result or continues; the `default:` continues; the check goes on after the switch.
struct SpellCheckCastAuraDummySite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_CHECK_CAST_AURA_DUMMY;
    typedef SpellCastResult Value;
    typedef SpellCheckCastAuraDummyContext Context;
};

/// Registers every `Spell::CheckCast` handler on `registry` (defined in SpellChecksHandlers.cpp).
/// Returns the number of rows it registered, the site's `default:` counting as one; a row whose key was
/// taken, or a second default, does not change the table, so a caller that registers on an empty table and
/// finds fewer keys and defaults than rows has a duplicate.
uint32 RegisterSpellChecksHandlers(SpellHandlerRegistry& registry);

#endif
