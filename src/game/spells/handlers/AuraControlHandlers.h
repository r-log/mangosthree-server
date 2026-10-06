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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_AURACONTROLHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_AURACONTROLHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"

class Unit;

/**
 * @file AuraControlHandlers.h
 * @brief The `Aura::HandleModThreat` site of the spell handler registry.
 *
 * The handlers are file-static functions in AuraControlHandlers.cpp; this header holds what the registry and
 * tests see.
 */

/// What the bodies of `Aura::HandleModThreat`'s threat-by-level switch read and write: the target, and the two
/// locals the member reads after the switch.
struct AuraThreatContext
{
    AuraThreatContext(Unit*& targetLocal, int& levelDiffLocal, int& multiplierLocal)
        : target(targetLocal), level_diff(levelDiffLocal), multiplier(multiplierLocal) {}

    Unit*& target;          ///< the function's local `Unit* target = GetTarget();`
    int& level_diff;        ///< the function's local `int level_diff = 0;`, read after the switch
    int& multiplier;        ///< the function's local `int multiplier = 0;`, read after the switch
};

/// `Aura::HandleModThreat`, at a real apply or remove on a living target, its `switch (GetId())`: each label sets
/// the target's levels above 60 and the threat per level, then continues after the switch; there is no default.
struct AuraThreatSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_THREAT;
    typedef void Value;
    typedef AuraThreatContext Context;
};

/// Registers every `Aura::HandleModThreat` handler on `registry` (defined in AuraControlHandlers.cpp).
/// Returns the number of rows it registered; a row whose key was taken does not change the table, so a
/// caller that registers on an empty table and finds fewer keys than rows has a duplicate.
uint32 RegisterAuraControlHandlers(SpellHandlerRegistry& registry);

#endif
