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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_AURASHAPESHIFTHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_AURASHAPESHIFTHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"

class Unit;

/**
 * @file AuraShapeshiftHandlers.h
 * @brief The `Aura::HandleAuraTransform` site of the spell handler registry.
 *
 * The handlers are file-static functions in AuraShapeshiftHandlers.cpp; this header holds what the registry and
 * tests see.
 */

/// What the bodies of `Aura::HandleAuraTransform`'s model-by-spell switch read: the target and the aura's spell id.
struct AuraTransformContext
{
    AuraTransformContext(uint32 auraSpellId, Unit*& targetLocal) : spellId(auraSpellId), target(targetLocal) {}

    uint32 const spellId;   ///< `GetId()` in the case bodies, which no body changes
    Unit*& target;          ///< the function's local `Unit* target = GetTarget();`
};

/// `Aura::HandleAuraTransform`, AT APPLY with no creature entry (`m_modifier.m_miscvalue == 0`), its
/// `switch (GetId())`: the `default:` logs the spell; every label and the default continue after the switch.
struct AuraTransformSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_TRANSFORM;
    typedef void Value;
    typedef AuraTransformContext Context;
};

/// Registers every `Aura::HandleAuraTransform` handler on `registry` (defined in AuraShapeshiftHandlers.cpp).
/// Returns the number of rows it registered, the site's `default:` counting as one; a row whose key was
/// taken, or a second default, does not change the table, so a caller that registers on an empty table and
/// finds fewer keys and defaults than rows has a duplicate.
uint32 RegisterAuraShapeshiftHandlers(SpellHandlerRegistry& registry);

#endif
