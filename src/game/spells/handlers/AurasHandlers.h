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


#ifndef MANGOSSERVER_SPELLS_HANDLERS_AURASHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_AURASHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"

class Aura;
class ObjectGuid;
class Unit;
struct SpellEntry;

/**
 * @file AurasHandlers.h
 * @brief The `SpellAuras.cpp` sites of the spell handler registry.
 *
 * The handlers are file-static functions in AurasHandlers.cpp; this header holds what the registry and tests see.
 */

/// What the bodies of `Aura::TriggerSpell`'s switches read and write: the aura and the function's locals, by
/// reference where a body sets one.
struct AuraTriggerSpellContext
{
    AuraTriggerSpellContext(Aura* thisAura, ObjectGuid const& casterGuidLocal, Unit*& triggerTargetLocal,
                            uint32& triggerSpellIdLocal, SpellEntry const* const& triggeredSpellInfoLocal,
                            SpellEntry const* const& auraSpellInfoLocal, uint32 const& auraIdLocal,
                            Unit*& targetLocal, Unit*& triggerCasterLocal)
        : aura(thisAura), casterGUID(casterGuidLocal), triggerTarget(triggerTargetLocal),
          trigger_spell_id(triggerSpellIdLocal), triggeredSpellInfo(triggeredSpellInfoLocal),
          auraSpellInfo(auraSpellInfoLocal), auraId(auraIdLocal), target(targetLocal),
          triggerCaster(triggerCasterLocal) {}

    Aura* const aura;                            ///< `this` in the case bodies
    ObjectGuid const& casterGUID;                ///< the function's local `ObjectGuid casterGUID`, the aura's caster
    Unit*& triggerTarget;                        ///< the function's local `Unit* triggerTarget`, which bodies set
    uint32& trigger_spell_id;                    ///< the function's local `uint32 trigger_spell_id`, which bodies set
    SpellEntry const* const& triggeredSpellInfo; ///< the function's local, the trigger spell looked up
    SpellEntry const* const& auraSpellInfo;      ///< the function's local `SpellEntry const* auraSpellInfo`
    uint32 const& auraId;                        ///< the function's local `uint32 auraId`, the aura's spell id
    Unit*& target;                               ///< the function's local `Unit* target`, the aura's target
    Unit*& triggerCaster;                        ///< the function's local `Unit* triggerCaster`, which a body may set
};

/// `Aura::TriggerSpell`, no trigger spell in the effect, SPELLFAMILY_GENERIC's `switch (auraId)`: a label
/// that breaks goes on to the re-read of the trigger spell; the `default:` does nothing.
struct AuraTriggerSpellGenericSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_TRIGGER_SPELL_GENERIC;
    typedef void Value;
    typedef AuraTriggerSpellContext Context;
};

/// Registers every `SpellAuras.cpp` handler on `registry` (defined in AurasHandlers.cpp).
/// Returns the number of rows it registered, a site's `default:` counting as one; a row whose key was taken,
/// or a second default at a site, does not change the table, so a caller that registers on an empty table
/// and finds fewer keys and defaults than rows has a duplicate.
uint32 RegisterAurasHandlers(SpellHandlerRegistry& registry);

#endif
