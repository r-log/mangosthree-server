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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_SPELLTARGETINGHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_SPELLTARGETINGHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"

#include <list>

class Spell;
class SpellCastTargets;
class Unit;
struct SpellEffectEntry;
struct SpellEntry;

/**
 * @file SpellTargetingHandlers.h
 * @brief The three `Spell::SetTargetMap` sites and the two `Spell::GetSpellRangeAndRadius` sites of the spell
 *        handler registry.
 *
 * The handlers are file-static functions in SpellTargetingHandlers.cpp; this header holds what the registry
 * and tests see.
 */

/// What the bodies of `Spell::SetTargetMap`'s TARGET_ALL_ENEMY_IN_AREA switch read and write: the spell's
/// caster, the target list being filled and the most targets the effect may hit.
struct SpellTargetAllEnemyInAreaContext
{
    SpellTargetAllEnemyInAreaContext(Unit*& casterMember, std::list<Unit*>& targetsParameter,
                                     uint32 const& maxTargetsLocal)
        : m_caster(casterMember), targetUnitMap(targetsParameter), unMaxTargets(maxTargetsLocal) {}

    Unit*& m_caster;                    ///< the spell's member `Unit* m_caster;`
    std::list<Unit*>& targetUnitMap;    ///< the function's parameter `UnitList& targetUnitMap`
    uint32 const& unMaxTargets;         ///< the function's local `uint32 unMaxTargets`
};

/// `Spell::SetTargetMap`, TARGET_ALL_ENEMY_IN_AREA, after the area is filled: its `switch (m_spellInfo->ID)`.
/// The labels drop the caster's victim from the targets or keep only the furthest ones; the `default:` keeps
/// the area's targets; every outcome continues after the switch.
struct SpellTargetAllEnemyInAreaSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_TARGET_ALL_ENEMY_IN_AREA;
    typedef void Value;
    typedef SpellTargetAllEnemyInAreaContext Context;
};

/// What the bodies of `Spell::SetTargetMap`'s TARGET_EFFECT_SELECT, SPELL_EFFECT_DUMMY switch read and write:
/// the spell, its caster, its entry, its targets and the target list being filled.
struct SpellTargetEffectDummyContext
{
    SpellTargetEffectDummyContext(Spell* thisSpell, Unit*& casterMember, SpellEntry const*& spellInfoMember,
                                  SpellCastTargets& targetsMember, std::list<Unit*>& targetsParameter)
        : spell(thisSpell), m_caster(casterMember), m_spellInfo(spellInfoMember), m_targets(targetsMember),
          targetUnitMap(targetsParameter) {}

    Spell* spell;                       ///< the spell itself
    Unit*& m_caster;                    ///< the spell's member `Unit* m_caster;`
    SpellEntry const*& m_spellInfo;     ///< the spell's member `SpellEntry const* m_spellInfo;`
    SpellCastTargets& m_targets;        ///< the spell's member `SpellCastTargets m_targets;`
    std::list<Unit*>& targetUnitMap;    ///< the function's parameter `UnitList& targetUnitMap`
};

/// `Spell::SetTargetMap`, TARGET_EFFECT_SELECT, SPELL_EFFECT_DUMMY: its `switch (m_spellInfo->ID)`. Cannibalize
/// targets the first corpse in range or fails the cast; the `default:` targets the unit target, if any; every outcome
/// continues after the switch.
struct SpellTargetEffectDummySite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_TARGET_EFFECT_DUMMY;
    typedef void Value;
    typedef SpellTargetEffectDummyContext Context;
};

/// What the bodies of `Spell::SetTargetMap`'s TARGET_ALL_FRIENDLY_UNITS_AROUND_CASTER switch read and write: the
/// spell, its caster, the target list being filled and the effect's radius.
struct SpellTargetAllFriendlyUnitsAroundCasterContext
{
    SpellTargetAllFriendlyUnitsAroundCasterContext(Spell* thisSpell, Unit*& casterMember,
                                                   std::list<Unit*>& targetsParameter, float const& radiusLocal)
        : spell(thisSpell), m_caster(casterMember), targetUnitMap(targetsParameter), radius(radiusLocal) {}

    Spell* spell;                       ///< the spell itself
    Unit*& m_caster;                    ///< the spell's member `Unit* m_caster;`
    std::list<Unit*>& targetUnitMap;    ///< the function's parameter `UnitList& targetUnitMap`
    float const& radius;                ///< the function's local `float radius`
};

/// `Spell::SetTargetMap`, TARGET_ALL_FRIENDLY_UNITS_AROUND_CASTER: its `switch (m_spellInfo->ID)`. Divine Hymn and
/// Hymn of Hope fill the targets by health or mana; there is no registered default, so a miss fills the friendly
/// units around the casting object.
struct SpellTargetAllFriendlyUnitsAroundCasterSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_TARGET_ALL_FRIENDLY_UNITS_AROUND_CASTER;
    typedef void Value;
    typedef SpellTargetAllFriendlyUnitsAroundCasterContext Context;
};

/// What the bodies of `Spell::GetSpellRangeAndRadius`'s switches read and write: the spell, its caster and entry,
/// the effect, and the radius and most targets the function answers.
struct SpellRangeAndRadiusContext
{
    SpellRangeAndRadiusContext(Spell const* thisSpell, Unit* const& casterMember, SpellEntry const* const& spellInfoMember,
                               SpellEffectEntry const* spellEffectParameter, float& radiusParameter,
                               uint32& maxTargetsParameter)
        : spell(thisSpell), m_caster(casterMember), m_spellInfo(spellInfoMember), spellEffect(spellEffectParameter),
          radius(radiusParameter), unMaxTargets(maxTargetsParameter) {}

    Spell const* spell;                     ///< the spell itself
    Unit* const& m_caster;                  ///< the spell's member `Unit* m_caster;`
    SpellEntry const* const& m_spellInfo;   ///< the spell's member `SpellEntry const* m_spellInfo;`
    SpellEffectEntry const* spellEffect;    ///< the function's parameter `SpellEffectEntry const* spellEffect`
    float& radius;                          ///< the function's parameter `float& radius`
    uint32& unMaxTargets;                   ///< the function's parameter `uint32& unMaxTargets`
};

/// `Spell::GetSpellRangeAndRadius`, the custom target amounts, SPELLFAMILY_GENERIC's `switch (m_spellInfo->ID)`:
/// Conflagration sets the most targets by the map's difficulty; there is no registered default, so a miss leaves the
/// spell to the fixed amounts of FindSpellMaxTargets; every outcome continues after the switch.
struct SpellMaxTargetsGenericSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_MAX_TARGETS_GENERIC;
    typedef void Value;
    typedef SpellRangeAndRadiusContext Context;
};

/// `Spell::GetSpellRangeAndRadius`, the custom radius cases, SPELLFAMILY_GENERIC's `switch (m_spellInfo->ID)`: the
/// labels compute the radius from the effect, an aura's remaining duration or the caster's scale; there is no
/// registered default, so a miss leaves the spell to HasInstanceVisibilityRadius; every outcome continues after it.
struct SpellRadiusGenericSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_RADIUS_GENERIC;
    typedef void Value;
    typedef SpellRangeAndRadiusContext Context;
};

/// Registers every `Spell::SetTargetMap` handler on `registry` (defined in SpellTargetingHandlers.cpp).
/// Returns the number of rows it registered, each site's `default:` counting as one; a row whose key was
/// taken, or a second default, does not change the table, so a caller that registers on an empty table and
/// finds fewer keys and defaults than rows has a duplicate.
uint32 RegisterSpellTargetingHandlers(SpellHandlerRegistry& registry);

#endif
