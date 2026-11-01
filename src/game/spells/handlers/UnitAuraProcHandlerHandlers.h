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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_UNITAURAPROCHANDLERHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_UNITAURAPROCHANDLERHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"
#include "Object/Unit.h"

/**
 * @file UnitAuraProcHandlerHandlers.h
 * @brief The per-spell switches of `Unit`'s aura proc handlers in the spell handler registry.
 *
 * The handlers are file-static functions in UnitAuraProcHandlerHandlers.cpp; this header holds what the registry and
 * tests see.
 */

/// What the bodies of `Unit::HandleDummyAuraProc`'s per-spell switches read and set: the unit, its spell cooldowns,
/// the function's parameters and locals. The bodies set the triggered spell, its target and its base points, and one
/// raises the function's copy of the aura's amount. Built, it checks that this thread owns the unit
/// (`Unit::AssertSpellSeamOwner`); a context with no unit checks nothing.
struct DummyAuraProcContext
{
    DummyAuraProcContext(Unit* thisUnit, SpellCooldownMgr& spellCooldownMgr, Unit* const& victimParameter,
                         uint32 const& damageParameter, Aura* const& triggeredByAuraParameter,
                         SpellEntry const* const& procSpellParameter, uint32 const& procFlagParameter,
                         uint32 const& cooldownParameter, SpellEntry const* const& dummySpellLocal,
                         SpellEffectIndex const& effIndexLocal,
                         SpellClassOptionsEntry const* const& procClassOptionsLocal, int32& triggerAmountLocal,
                         Item* const& castItemLocal, uint32& triggeredSpellIdLocal, Unit*& targetLocal,
                         int32 (&basepointsLocal)[MAX_EFFECT_INDEX])
        : unit(thisUnit), m_spellCooldownMgr(spellCooldownMgr), pVictim(victimParameter), damage(damageParameter),
          triggeredByAura(triggeredByAuraParameter), procSpell(procSpellParameter), procFlag(procFlagParameter),
          cooldown(cooldownParameter), dummySpell(dummySpellLocal), effIndex(effIndexLocal),
          procClassOptions(procClassOptionsLocal), triggerAmount(triggerAmountLocal), castItem(castItemLocal),
          triggered_spell_id(triggeredSpellIdLocal), target(targetLocal), basepoints(basepointsLocal)
    {
        if (unit)
        {
            unit->AssertSpellSeamOwner();
        }
    }

    Unit* const unit;                               ///< `this` in the case bodies
    SpellCooldownMgr& m_spellCooldownMgr;           ///< the unit's spell cooldowns
    Unit* const& pVictim;                           ///< the function's parameter `Unit* pVictim`
    uint32 const& damage;                           ///< the function's parameter `uint32 damage`
    Aura* const& triggeredByAura;                   ///< the function's parameter `Aura* triggeredByAura`
    SpellEntry const* const& procSpell;             ///< the function's parameter `SpellEntry const* procSpell`
    uint32 const& procFlag;                         ///< the function's parameter `uint32 procFlag`
    uint32 const& cooldown;                         ///< the function's parameter `uint32 cooldown`
    SpellEntry const* const& dummySpell;            ///< the function's local `dummySpell`, the aura's spell
    SpellEffectIndex const& effIndex;               ///< the function's local `effIndex`, the aura's effect index
    SpellClassOptionsEntry const* const& procClassOptions; ///< the function's local `procClassOptions`
    int32& triggerAmount;                           ///< the function's local `triggerAmount`, which a body sets
    Item* const& castItem;                          ///< the function's local `castItem`
    uint32& triggered_spell_id;                     ///< the function's local `triggered_spell_id`, which bodies set
    Unit*& target;                                  ///< the function's local `target`, which bodies set
    int32 (&basepoints)[MAX_EFFECT_INDEX];          ///< the function's local `basepoints`, which bodies set
};

/// `Unit::HandleDummyAuraProc`, SPELLFAMILY_GENERIC's `switch (dummySpell->ID)`. A label sets what the proc casts or
/// ends the proc with its result; a miss goes on to the cast after the family switch.
struct DummyAuraProcGenericSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_GENERIC;
    typedef SpellAuraProcResult Value;
    typedef DummyAuraProcContext Context;
};

/// `Unit::HandleDummyAuraProc`, SPELLFAMILY_MAGE's `switch (dummySpell->ID)`, after the checks by spell icon. A label
/// sets what the proc casts or ends the proc with its result; a miss goes on to the cast after the family switch.
struct DummyAuraProcMageSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_MAGE;
    typedef SpellAuraProcResult Value;
    typedef DummyAuraProcContext Context;
};

/// `Unit::HandleDummyAuraProc`, SPELLFAMILY_WARLOCK's `switch (dummySpell->ID)`, after the checks by family flag and
/// spell icon. A label sets what the proc casts; a miss goes on to the cast after the family switch.
struct DummyAuraProcWarlockSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_WARLOCK;
    typedef SpellAuraProcResult Value;
    typedef DummyAuraProcContext Context;
};

/// `Unit::HandleDummyAuraProc`, SPELLFAMILY_PRIEST's `switch (dummySpell->ID)`, after the checks by family flag and
/// spell icon. A label sets what the proc casts or ends the proc with its result; a miss goes on to the cast after the
/// family switch.
struct DummyAuraProcPriestSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_PRIEST;
    typedef SpellAuraProcResult Value;
    typedef DummyAuraProcContext Context;
};

/// `Unit::HandleDummyAuraProc`, SPELLFAMILY_DRUID's `switch (dummySpell->ID)`, before the checks by spell icon. A label
/// sets what the proc casts or ends the proc with its result; a miss goes on to the checks by spell icon.
struct DummyAuraProcDruidSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_DRUID;
    typedef SpellAuraProcResult Value;
    typedef DummyAuraProcContext Context;
};

/// `Unit::HandleDummyAuraProc`, SPELLFAMILY_ROGUE's `switch (dummySpell->ID)`, before the checks by spell icon. A label
/// sets what the proc casts or fails the proc; a miss goes on to the checks by spell icon.
struct DummyAuraProcRogueSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_ROGUE;
    typedef SpellAuraProcResult Value;
    typedef DummyAuraProcContext Context;
};

/// `Unit::HandleDummyAuraProc`, SPELLFAMILY_PALADIN's `switch (dummySpell->ID)`, after the checks by spell id and spell
/// icon. A label sets what the proc casts or ends the proc with its result; a miss goes on to the cast after the family
/// switch.
struct DummyAuraProcPaladinSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_PALADIN;
    typedef SpellAuraProcResult Value;
    typedef DummyAuraProcContext Context;
};

/// `Unit::HandleDummyAuraProc`, SPELLFAMILY_SHAMAN's `switch (dummySpell->ID)`, before the checks by spell icon, family
/// flag and spell id. A label sets what the proc casts or ends the proc with its result; a miss goes on to those
/// checks.
struct DummyAuraProcShamanSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_SHAMAN;
    typedef SpellAuraProcResult Value;
    typedef DummyAuraProcContext Context;
};

/// What the body of `Unit::HandleHasteAuraProc`'s per-spell switch reads and sets: the unit, the function's
/// parameters and locals. The body sets the triggered spell, its target and its base points. Built, it checks
/// that this thread owns the unit (`Unit::AssertSpellSeamOwner`); a context with no unit checks nothing.
struct HasteAuraProcContext
{
    HasteAuraProcContext(Unit* thisUnit, Unit* const& victimParameter, uint32 const& damageParameter,
                         uint32& triggeredSpellIdLocal, Unit*& targetLocal, int32& basepoints0Local)
        : unit(thisUnit), pVictim(victimParameter), damage(damageParameter), triggered_spell_id(triggeredSpellIdLocal),
          target(targetLocal), basepoints0(basepoints0Local)
    {
        if (unit)
        {
            unit->AssertSpellSeamOwner();
        }
    }

    Unit* const unit;                               ///< `this` in the case bodies
    Unit* const& pVictim;                           ///< the function's parameter `Unit* pVictim`
    uint32 const& damage;                           ///< the function's parameter `uint32 damage`
    uint32& triggered_spell_id;                     ///< the function's local `triggered_spell_id`, which the body sets
    Unit*& target;                                  ///< the function's local `target`, which the body sets
    int32& basepoints0;                             ///< the function's local `basepoints0`, which the body sets
};

/// `Unit::HandleHasteAuraProc`, SPELLFAMILY_ROGUE's `switch (hasteSpell->ID)`. A label sets what the proc casts or
/// fails the proc; a miss goes on after the family switch.
struct HasteAuraProcRogueSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_UNIT_HASTE_AURA_PROC_ROGUE;
    typedef SpellAuraProcResult Value;
    typedef HasteAuraProcContext Context;
};

/// What the body of `Unit::HandleSpellCritChanceAuraProc`'s per-spell switch reads and sets: the unit, the function's
/// parameter `triggeredByAura` and two locals. The body sets the triggered spell and its target. Built, it checks
/// that this thread owns the unit (`Unit::AssertSpellSeamOwner`); a context with no unit checks nothing.
struct SpellCritChanceAuraProcContext
{
    SpellCritChanceAuraProcContext(Unit* thisUnit, Aura* const& triggeredByAuraParameter, uint32& triggeredSpellIdLocal,
                                   Unit*& targetLocal)
        : unit(thisUnit), triggeredByAura(triggeredByAuraParameter), triggered_spell_id(triggeredSpellIdLocal),
          target(targetLocal)
    {
        if (unit)
        {
            unit->AssertSpellSeamOwner();
        }
    }

    Unit* const unit;                               ///< `this` in the case bodies
    Aura* const& triggeredByAura;                   ///< the function's parameter `Aura* triggeredByAura`
    uint32& triggered_spell_id;                     ///< the function's local `triggered_spell_id`, which the body sets
    Unit*& target;                                  ///< the function's local `target`, which the body sets
};

/// `Unit::HandleSpellCritChanceAuraProc`, SPELLFAMILY_MAGE's `switch (triggeredByAuraSpell->ID)`. A label sets what
/// the proc casts or fails the proc; a miss goes on after the family switch.
struct SpellCritChanceAuraProcMageSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_UNIT_SPELL_CRIT_CHANCE_AURA_PROC_MAGE;
    typedef SpellAuraProcResult Value;
    typedef SpellCritChanceAuraProcContext Context;
};

/// What the bodies of `Unit::HandleProcTriggerSpellAuraProc`'s two per-spell switches read and set: the unit, the
/// function's parameters and locals. The bodies set the spell to trigger, its target and its base points; the second
/// switch is keyed by that spell, and its bodies never set it. Built, it checks that this thread owns the
/// unit (`Unit::AssertSpellSeamOwner`); a context with no unit checks nothing.
struct ProcTriggerSpellAuraProcContext
{
    ProcTriggerSpellAuraProcContext(Unit* thisUnit, Unit* const& victimParameter, uint32 const& damageParameter,
                                    Aura* const& triggeredByAuraParameter, SpellEntry const* const& procSpellParameter,
                                    uint32 const& procFlagsParameter, uint32 const& procExParameter,
                                    SpellEntry const* const& auraSpellInfoLocal,
                                    SpellClassOptionsEntry const* const& procClassOptionsLocal,
                                    int32 const& triggerAmountLocal, uint32& triggerSpellIdLocal, Unit*& targetLocal,
                                    int32 (&basepointsLocal)[MAX_EFFECT_INDEX], Item* const& castItemLocal)
        : unit(thisUnit), pVictim(victimParameter), damage(damageParameter), triggeredByAura(triggeredByAuraParameter),
          procSpell(procSpellParameter), procFlags(procFlagsParameter), procEx(procExParameter),
          auraSpellInfo(auraSpellInfoLocal), procClassOptions(procClassOptionsLocal), triggerAmount(triggerAmountLocal),
          trigger_spell_id(triggerSpellIdLocal), target(targetLocal), basepoints(basepointsLocal),
          castItem(castItemLocal)
    {
        if (unit)
        {
            unit->AssertSpellSeamOwner();
        }
    }

    Unit* const unit;                               ///< `this` in the case bodies
    Unit* const& pVictim;                           ///< the function's parameter `Unit* pVictim`
    uint32 const& damage;                           ///< the function's parameter `uint32 damage`
    Aura* const& triggeredByAura;                   ///< the function's parameter `Aura* triggeredByAura`
    SpellEntry const* const& procSpell;             ///< the function's parameter `SpellEntry const* procSpell`
    uint32 const& procFlags;                        ///< the function's parameter `uint32 procFlags`
    uint32 const& procEx;                           ///< the function's parameter `uint32 procEx`
    SpellEntry const* const& auraSpellInfo;         ///< the function's local `auraSpellInfo`, the aura's spell
    SpellClassOptionsEntry const* const& procClassOptions; ///< the function's local `procClassOptions`
    int32 const& triggerAmount;                     ///< the function's local `triggerAmount`, the aura's amount
    uint32& trigger_spell_id;                       ///< the function's local `trigger_spell_id`, which bodies set
    Unit*& target;                                  ///< the function's local `target`, which bodies set
    int32 (&basepoints)[MAX_EFFECT_INDEX];          ///< the function's local `basepoints`, which bodies set
    Item* const& castItem;                          ///< the function's local `castItem`
};

/// `Unit::HandleProcTriggerSpellAuraProc`, SPELLFAMILY_GENERIC's `switch (auraSpellInfo->ID)`. A label sets what the
/// proc casts or ends the proc with its result; a miss goes on after the family switch to the checks of the spell to
/// trigger.
struct ProcTriggerSpellAuraProcGenericSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_UNIT_PROC_TRIGGER_SPELL_AURA_PROC_GENERIC;
    typedef SpellAuraProcResult Value;
    typedef ProcTriggerSpellAuraProcContext Context;
};

/// `Unit::HandleProcTriggerSpellAuraProc`'s `switch (trigger_spell_id)`, after the checks of the spell to trigger. A
/// label checks the proc, sets the cast's target or base points, ends a category cooldown or ends the proc with its
/// result; a miss goes on to the cooldown check and the cast.
struct ProcTriggerSpellAuraProcTriggeredSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_UNIT_PROC_TRIGGER_SPELL_AURA_PROC_TRIGGERED;
    typedef SpellAuraProcResult Value;
    typedef ProcTriggerSpellAuraProcContext Context;
};

/// Registers every handler of the proc sites on `registry` (defined in UnitAuraProcHandlerHandlers.cpp). Returns the
/// number of rows it registered; a row whose key was taken does not change the table, so a caller that registers on
/// an empty table and finds fewer keys than rows has a duplicate.
uint32 RegisterUnitAuraProcHandlerHandlers(SpellHandlerRegistry& registry);

#endif
