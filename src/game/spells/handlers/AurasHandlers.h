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
#include "Server/SharedDefines.h"

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
    Unit*& triggerCaster;                        ///< the function's local `Unit* triggerCaster`, which bodies set
};

/// `Aura::TriggerSpell`, no trigger spell in the effect, SPELLFAMILY_GENERIC's `switch (auraId)`: a label
/// that breaks goes on to the re-read of the trigger spell; the `default:` does nothing.
struct AuraTriggerSpellGenericSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_TRIGGER_SPELL_GENERIC;
    typedef void Value;
    typedef AuraTriggerSpellContext Context;
};

/// `Aura::TriggerSpell`, no trigger spell in the effect, SPELLFAMILY_MAGE's `switch (auraId)`: the label
/// returns from the member; the `default:` does nothing.
struct AuraTriggerSpellMageSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_TRIGGER_SPELL_MAGE;
    typedef void Value;
    typedef AuraTriggerSpellContext Context;
};

/// `Aura::TriggerSpell`, no trigger spell in the effect, SPELLFAMILY_HUNTER's `switch (auraId)`: the label
/// breaks to the re-read of the trigger spell; the `default:` does nothing.
struct AuraTriggerSpellHunterSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_TRIGGER_SPELL_HUNTER;
    typedef void Value;
    typedef AuraTriggerSpellContext Context;
};

/// `Aura::TriggerSpell`, no trigger spell in the effect, SPELLFAMILY_DRUID's `switch (auraId)`: each label
/// returns from the member; the `default:` does nothing.
struct AuraTriggerSpellDruidSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_TRIGGER_SPELL_DRUID;
    typedef void Value;
    typedef AuraTriggerSpellContext Context;
};

/// `Aura::TriggerSpell`, no trigger spell in the effect, SPELLFAMILY_SHAMAN's `switch (auraId)`: each label
/// returns from the member; the `default:` does nothing.
struct AuraTriggerSpellShamanSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_TRIGGER_SPELL_SHAMAN;
    typedef void Value;
    typedef AuraTriggerSpellContext Context;
};

/// `Aura::TriggerSpell`, a trigger spell that needs custom code, its `switch (auraId)`: no `default:`; a label that
/// breaks goes on to the default cast.
struct AuraTriggerSpellCustomSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_TRIGGER_SPELL_CUSTOM;
    typedef void Value;
    typedef AuraTriggerSpellContext Context;
};

/// What the bodies of `Aura::PeriodicTick`'s periodic-damage switch read: the aura and the function's locals.
struct AuraPeriodicDamageTickContext
{
    AuraPeriodicDamageTickContext(Aura* thisAura, Unit*& targetLocal, SpellEntry const* const& spellProtoLocal,
                                  Unit*& casterLocal)
        : aura(thisAura), target(targetLocal), spellProto(spellProtoLocal), pCaster(casterLocal) {}

    Aura* const aura;                    ///< `this` in the case bodies
    Unit*& target;                       ///< the function's local `Unit* target`, the aura's target
    SpellEntry const* const& spellProto; ///< the function's local `SpellEntry const* spellProto`, the aura's spell
    Unit*& pCaster;                      ///< the damage block's local `Unit* pCaster`, the aura's caster
};

/// `Aura::PeriodicTick`, a periodic damage tick, its `switch (GetId())` before the damage: a label that breaks goes
/// on to the damage; the `default:` does nothing.
struct AuraPeriodicDamageTickSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_PERIODIC_DAMAGE_TICK;
    typedef void Value;
    typedef AuraPeriodicDamageTickContext Context;
};

/// What the bodies of `Aura::PeriodicTick`'s mana-leech switch read and write: the aura and the block's locals, by
/// reference where a body sets one.
struct AuraPeriodicManaLeechTickContext
{
    AuraPeriodicManaLeechTickContext(Aura* thisAura, Unit*& targetLocal, Powers const& powerLocal,
                                     uint32& damageLocal, int32& drainAmountLocal)
        : aura(thisAura), target(targetLocal), power(powerLocal), pdamage(damageLocal),
          drain_amount(drainAmountLocal) {}

    Aura* const aura;    ///< `this` in the case bodies
    Unit*& target;       ///< the function's local `Unit* target`, the aura's target
    Powers const& power; ///< the leech block's local `Powers power`, the power drained
    uint32& pdamage;     ///< the leech block's local `uint32 pdamage`, the amount to drain, which a body sets
    int32& drain_amount; ///< the leech block's local `int32 drain_amount`, the amount drained, which a body sets
};

/// `Aura::PeriodicTick`, a mana-leech tick, its `switch (GetId())` after the drain: no `default:`; every label
/// continues after the switch.
struct AuraPeriodicManaLeechTickSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_AURA_PERIODIC_MANA_LEECH_TICK;
    typedef void Value;
    typedef AuraPeriodicManaLeechTickContext Context;
};

/// Registers every `SpellAuras.cpp` handler on `registry` (defined in AurasHandlers.cpp).
/// Returns the number of rows it registered, a site's `default:` counting as one; a row whose key was taken,
/// or a second default at a site, does not change the table, so a caller that registers on an empty table
/// and finds fewer keys and defaults than rows has a duplicate.
uint32 RegisterAurasHandlers(SpellHandlerRegistry& registry);

#endif
