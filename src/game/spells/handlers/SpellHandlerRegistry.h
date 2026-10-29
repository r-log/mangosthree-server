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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_SPELLHANDLERREGISTRY_H
#define MANGOSSERVER_SPELLS_HANDLERS_SPELLHANDLERREGISTRY_H

#include "Platform/Define.h"

#include <cstddef>
#include <map>

/**
 * @file SpellHandlerRegistry.h
 * @brief The spell handler registry (decoupling D11, design/2026-09-28-unit-reopening.md 3(b)).
 *
 * The per-spell-ID switches inside the spell handlers become rows of one table keyed by
 * (site, spell id). A *site* is one named top-level spell-ID switch together with its key
 * expression; siblings in one function and family are different sites, and a spell id handled by
 * two sites is two keys.
 *
 * The contract, for every site:
 * - a handler takes the site's context by reference; the context holds the site's live-out
 *   locals (by reference, so a handler's write reaches the code after the switch);
 * - a handler returns SpellHandlerOutcome::Return(value) -- the case's `return` (no value in a
 *   void function, else the function's return value) -- or SpellHandlerOutcome::Continue() -- the
 *   case's `break`; at a site standing in a loop, SpellHandlerOutcome::LoopContinue() is the case's
 *   `continue`, and a site whose handlers answer it tests IsLoopContinue() after the dispatch and
 *   continues;
 * - a miss runs the site's own `default:` body, registered with RegisterDefault(), if it has
 *   one; otherwise Dispatch() answers Miss() and the site continues after where its switch stood;
 * - labels that share a body register one function under each of their ids.
 *
 * The registry is consulted exactly where the switch stood, after every check before it.
 *
 * A site is a traits type:
 * @code
 * struct MySite
 * {
 *     static uint32 const Key = SPELL_HANDLER_SITE_...;  // unique per site
 *     typedef void Value;                              // or the function's return type
 *     typedef MySiteContext Context;                   // the live-out locals
 * };
 * @endcode
 * A key is registered through exactly one traits type; the stored function pointer is cast back
 * to that traits type's handler type only.
 */

/// The sites the game registers. One value per site, never reused.
enum SpellHandlerSite
{
    /// `Aura::HandleAuraDummy`, AT APPLY, SPELLFAMILY_WARRIOR's `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_DUMMY_APPLY_WARRIOR = 1,
    /// `Aura::HandleAuraDummy`, AT APPLY & REMOVE, SPELLFAMILY_DRUID's `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_DUMMY_DRUID = 3,
    /// `Aura::HandleAuraDummy`, AT REMOVE: the family-independent `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_DUMMY_REMOVE = 5,
    /// `Aura::HandleAuraDummy`, AT REMOVE, the hunter quest-tame block: its `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_DUMMY_QUEST_TAME = 6,
    /// `Aura::HandleAuraDummy`, AT APPLY & REMOVE, SPELLFAMILY_GENERIC's `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_DUMMY_APPLY_REMOVE_GENERIC = 7,
    /// `Aura::HandleAuraTransform`, AT APPLY with no creature entry: its `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_TRANSFORM = 8,
    /// `Aura::HandleModThreat`, at a real apply or remove on a living target: its `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_THREAT = 9,
    /// `Spell::EffectTransmitted`, before the game object entry is looked up: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_TRANSMITTED = 10,
    /// `Aura::HandleAuraProcTriggerSpell`, at a real apply or remove: its `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_PROC_TRIGGER = 11,
    /// `Aura::HandlePeriodicTriggerSpell`, at remove: its `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_PERIODIC_TRIGGER = 12,
    /// `Aura::HandlePeriodicEnergize`, at an apply that is not a load: its `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_PERIODIC_ENERGIZE = 13,
    /// `Aura::HandleAuraPeriodicDummy`, SPELLFAMILY_ROGUE's `switch(GetSpellProto()->ID)`.
    SPELL_HANDLER_SITE_AURA_PERIODIC_DUMMY_ROGUE = 14,
    /// `Aura::HandleAuraModIncreaseHealth`: its `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_INCREASE_HEALTH = 15,
    /// `Spell::EffectEnergize`, before the amount is shrunk by level: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_ENERGIZE = 16,
    /// `Spell::EffectActivateObject`, the custom-use misc value: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_ACTIVATE_OBJECT = 17,
    /// `Spell::EffectResurrect`, before the request is sent: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_RESURRECT = 18,
    /// `Spell::SetTargetMap`, TARGET_ALL_ENEMY_IN_AREA, after the area is filled: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_TARGET_ALL_ENEMY_IN_AREA = 19,
    /// `Spell::SetTargetMap`, TARGET_EFFECT_SELECT, SPELL_EFFECT_DUMMY: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_TARGET_EFFECT_DUMMY = 20,
    /// `Spell::CheckCast`, an effect that applies SPELL_AURA_DUMMY: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_CHECK_CAST_AURA_DUMMY = 21,
    /// `Spell::CheckTarget`, after the player-only attribute check: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_CHECK_TARGET = 22,
    /// `Spell::EffectWeaponDmg`, SPELLFAMILY_GENERIC: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_WEAPON_DAMAGE = 23,
    /// `Spell::EffectSchoolDMG`, SPELLFAMILY_GENERIC: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_SCHOOL_DAMAGE = 24,
    /// `Spell::EffectTriggerSpell`, before the triggered spell is looked up: its `switch (triggered_spell_id)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_TRIGGER_SPELL = 25,
    /// `Spell::EffectTeleportUnits`, before the target type is read: its first `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_TELEPORT_RECALL = 26,
    /// `Spell::EffectTeleportUnits`, after a table-coordinates teleport: its second `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_TELEPORT_POST = 27,
    /// `Spell::EffectDummy`, SPELLFAMILY_MAGE: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_MAGE = 28,
    /// `Spell::EffectDummy`, SPELLFAMILY_WARRIOR: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_WARRIOR = 29,
    /// `Spell::EffectDummy`, SPELLFAMILY_ROGUE: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_ROGUE = 30,
    /// `Spell::EffectDummy`, SPELLFAMILY_HUNTER: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_HUNTER = 31,
    /// `Spell::EffectDummy`, SPELLFAMILY_PALADIN: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_PALADIN = 32,
    /// `Spell::EffectDummy`, SPELLFAMILY_GENERIC: its `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_GENERIC = 33,
    /// `Aura::HandleAuraDummy`, AT APPLY, SPELLFAMILY_GENERIC's `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_DUMMY_APPLY_GENERIC = 34,
    /// `Aura::HandleAuraDummy`, AT APPLY & REMOVE, SPELLFAMILY_ROGUE's `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_DUMMY_ROGUE = 35,
    /// `Aura::HandleAuraDummy`, AT APPLY & REMOVE, SPELLFAMILY_HUNTER's `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_DUMMY_HUNTER = 36,
    /// `Aura::TriggerSpell`, no trigger spell in the effect, SPELLFAMILY_GENERIC's `switch (auraId)`.
    SPELL_HANDLER_SITE_AURA_TRIGGER_SPELL_GENERIC = 37,
    /// `Aura::TriggerSpell`, no trigger spell in the effect, SPELLFAMILY_MAGE's `switch (auraId)`.
    SPELL_HANDLER_SITE_AURA_TRIGGER_SPELL_MAGE = 38,
    /// `Aura::TriggerSpell`, no trigger spell in the effect, SPELLFAMILY_HUNTER's `switch (auraId)`.
    SPELL_HANDLER_SITE_AURA_TRIGGER_SPELL_HUNTER = 39,
    /// `Aura::TriggerSpell`, no trigger spell in the effect, SPELLFAMILY_DRUID's `switch (auraId)`.
    SPELL_HANDLER_SITE_AURA_TRIGGER_SPELL_DRUID = 40,
    /// `Aura::TriggerSpell`, no trigger spell in the effect, SPELLFAMILY_SHAMAN's `switch (auraId)`.
    SPELL_HANDLER_SITE_AURA_TRIGGER_SPELL_SHAMAN = 41,
    /// `Aura::TriggerSpell`, a trigger spell that needs custom code: its `switch (auraId)`.
    SPELL_HANDLER_SITE_AURA_TRIGGER_SPELL_CUSTOM = 42,
    /// `Aura::PeriodicTick`, a periodic damage tick, the health-level removals: its `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_PERIODIC_DAMAGE_TICK = 43,
    /// `Aura::PeriodicTick`, a mana-leech tick, its special cases: its `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_PERIODIC_MANA_LEECH_TICK = 44,
    /// `Aura::PeriodicDummyTick`, SPELLFAMILY_GENERIC's `switch (spell->ID)`.
    SPELL_HANDLER_SITE_AURA_PERIODIC_DUMMY_TICK_GENERIC = 45,
    /// `Aura::PeriodicDummyTick`, SPELLFAMILY_MAGE's `switch (spell->ID)`.
    SPELL_HANDLER_SITE_AURA_PERIODIC_DUMMY_TICK_MAGE = 46,
    /// `Aura::PeriodicDummyTick`, SPELLFAMILY_DRUID's `switch (spell->ID)`.
    SPELL_HANDLER_SITE_AURA_PERIODIC_DUMMY_TICK_DRUID = 47,
    /// `Aura::PeriodicDummyTick`, SPELLFAMILY_HUNTER's `switch (spell->ID)`.
    SPELL_HANDLER_SITE_AURA_PERIODIC_DUMMY_TICK_HUNTER = 48,
    /// `SpellAuraHolder::HandleSpellSpecificBoosts`, SPELLFAMILY_GENERIC's `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_BOOST_GENERIC = 49,
    /// `SpellAuraHolder::HandleSpellSpecificBoosts`, SPELLFAMILY_MAGE's `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_BOOST_MAGE = 50,
    /// `SpellAuraHolder::HandleSpellSpecificBoosts`, SPELLFAMILY_PRIEST's `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_BOOST_PRIEST = 51,
    /// `SpellAuraHolder::HandleSpellSpecificBoosts`, SPELLFAMILY_HUNTER's `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_BOOST_HUNTER = 52,
    /// `SpellAuraHolder::HandleSpellSpecificBoosts`, SPELLFAMILY_DEATHKNIGHT's `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_BOOST_DEATH_KNIGHT = 53,
    /// `Spell::EffectScriptEffect`, SPELLFAMILY_WARLOCK's `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_SCRIPT_WARLOCK = 54,
    /// `Spell::EffectScriptEffect`, SPELLFAMILY_PRIEST's `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_SCRIPT_PRIEST = 55,
    /// `Spell::EffectScriptEffect`, SPELLFAMILY_HUNTER's `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_SCRIPT_HUNTER = 56,
    /// `Spell::EffectScriptEffect`, SPELLFAMILY_POTION's `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_SCRIPT_POTION = 57,
    /// `Spell::EffectScriptEffect`, SPELLFAMILY_DEATHKNIGHT's `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_SCRIPT_DEATH_KNIGHT = 58,
    /// `Spell::EffectScriptEffect`, SPELLFAMILY_GENERIC's `switch (m_spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_EFFECT_SCRIPT_GENERIC = 59,
    /// `SpellMgr::GetSpellAllowedInLocationError`, after the battleground checks: its `switch (spellInfo->ID)`.
    SPELL_HANDLER_SITE_SPELL_ALLOWED_IN_LOCATION = 60,
    /// `Unit::SpellDamageBonusTaken`, the loop over the dummy auras: its `switch ((*i)->GetId())`.
    SPELL_HANDLER_SITE_UNIT_SPELL_DAMAGE_TAKEN = 61,
    /// `Unit::SpellHealingBonusDone`, Lifebloom, the loop over the owner's dummy auras: its
    /// `switch ((*i)->GetId())`.
    SPELL_HANDLER_SITE_UNIT_SPELL_HEALING_DONE = 62,
    /// `Unit::MeleeDamageBonusTaken`, the loop over the dummy auras: its `switch ((*i)->GetId())`.
    SPELL_HANDLER_SITE_UNIT_MELEE_DAMAGE_TAKEN = 63,
    /// `Unit::HandleDummyAuraProc`, SPELLFAMILY_GENERIC's `switch (dummySpell->ID)`.
    SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_GENERIC = 64,
    /// `Unit::HandleDummyAuraProc`, SPELLFAMILY_MAGE's `switch (dummySpell->ID)`.
    SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_MAGE = 65,
    /// `Unit::HandleDummyAuraProc`, SPELLFAMILY_WARLOCK's `switch (dummySpell->ID)`.
    SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_WARLOCK = 66,
    /// `Unit::HandleDummyAuraProc`, SPELLFAMILY_PRIEST's `switch (dummySpell->ID)`.
    SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_PRIEST = 67,
    /// `Unit::HandleDummyAuraProc`, SPELLFAMILY_DRUID's `switch (dummySpell->ID)`.
    SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_DRUID = 68,
    /// `Unit::HandleDummyAuraProc`, SPELLFAMILY_ROGUE's `switch (dummySpell->ID)`.
    SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_ROGUE = 69,
    /// `Unit::HandleDummyAuraProc`, SPELLFAMILY_PALADIN's `switch (dummySpell->ID)`.
    SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_PALADIN = 70,
    /// `Unit::HandleDummyAuraProc`, SPELLFAMILY_SHAMAN's `switch (dummySpell->ID)`.
    SPELL_HANDLER_SITE_UNIT_DUMMY_AURA_PROC_SHAMAN = 71,
    /// `Unit::HandleHasteAuraProc`, SPELLFAMILY_ROGUE's `switch (hasteSpell->ID)`.
    SPELL_HANDLER_SITE_UNIT_HASTE_AURA_PROC_ROGUE = 72,
    /// `Unit::HandleSpellCritChanceAuraProc`, SPELLFAMILY_MAGE's `switch (triggeredByAuraSpell->ID)`.
    SPELL_HANDLER_SITE_UNIT_SPELL_CRIT_CHANCE_AURA_PROC_MAGE = 73,
    /// `Unit::HandleProcTriggerSpellAuraProc`, SPELLFAMILY_GENERIC's `switch (auraSpellInfo->ID)`.
    SPELL_HANDLER_SITE_UNIT_PROC_TRIGGER_SPELL_AURA_PROC_GENERIC = 74,
    /// `Unit::HandleProcTriggerSpellAuraProc`'s `switch (trigger_spell_id)`.
    SPELL_HANDLER_SITE_UNIT_PROC_TRIGGER_SPELL_AURA_PROC_TRIGGERED = 75,
    /// `Aura::HandleAuraDummy`, AT APPLY, SPELLFAMILY_HUNTER's `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_DUMMY_APPLY_HUNTER = 76,
    /// `Aura::HandleAuraDummy`, AT APPLY, SPELLFAMILY_SHAMAN's `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_DUMMY_APPLY_SHAMAN = 77,
    /// `Aura::HandleAuraDummy`, AT APPLY & REMOVE, SPELLFAMILY_PALADIN's `switch (GetId())`.
    SPELL_HANDLER_SITE_AURA_DUMMY_PALADIN = 78,
    /// `Aura::PeriodicDummyTick`, SPELLFAMILY_ROGUE's `switch (spell->ID)`.
    SPELL_HANDLER_SITE_AURA_PERIODIC_DUMMY_TICK_ROGUE = 79
};

/// What a handler (or a dispatch) answers. `V` is the site's value type.
template <typename V>
class SpellHandlerOutcome
{
    public:
        /// The case's `return value;`.
        static SpellHandlerOutcome Return(V value) { return SpellHandlerOutcome(KIND_RETURN, value); }
        /// The case's `break;`: the site continues after where its switch stood.
        static SpellHandlerOutcome Continue() { return SpellHandlerOutcome(KIND_CONTINUE, V()); }
        /// The case's `continue;`: the loop around the site's switch takes its next iteration.
        static SpellHandlerOutcome LoopContinue() { return SpellHandlerOutcome(KIND_LOOP_CONTINUE, V()); }
        /// No handler and no default at the site: the site continues after where its switch stood.
        static SpellHandlerOutcome Miss() { return SpellHandlerOutcome(KIND_MISS, V()); }

        bool IsReturn() const { return m_kind == KIND_RETURN; }
        bool IsContinue() const { return m_kind == KIND_CONTINUE; }
        bool IsLoopContinue() const { return m_kind == KIND_LOOP_CONTINUE; }
        bool IsMiss() const { return m_kind == KIND_MISS; }
        /// The returned value; meaningful only when IsReturn().
        V GetValue() const { return m_value; }

    private:
        enum Kind { KIND_MISS, KIND_CONTINUE, KIND_LOOP_CONTINUE, KIND_RETURN };

        SpellHandlerOutcome(Kind kind, V value) : m_kind(kind), m_value(value) {}

        Kind m_kind;
        V m_value;
};

/// The outcome at a site in a void function: `return;`, `break;` or, in a loop, `continue;`.
template <>
class SpellHandlerOutcome<void>
{
    public:
        /// The case's `return;`.
        static SpellHandlerOutcome Return() { return SpellHandlerOutcome(KIND_RETURN); }
        /// The case's `break;`: the site continues after where its switch stood.
        static SpellHandlerOutcome Continue() { return SpellHandlerOutcome(KIND_CONTINUE); }
        /// The case's `continue;`: the loop around the site's switch takes its next iteration.
        static SpellHandlerOutcome LoopContinue() { return SpellHandlerOutcome(KIND_LOOP_CONTINUE); }
        /// No handler and no default at the site: the site continues after where its switch stood.
        static SpellHandlerOutcome Miss() { return SpellHandlerOutcome(KIND_MISS); }

        bool IsReturn() const { return m_kind == KIND_RETURN; }
        bool IsContinue() const { return m_kind == KIND_CONTINUE; }
        bool IsLoopContinue() const { return m_kind == KIND_LOOP_CONTINUE; }
        bool IsMiss() const { return m_kind == KIND_MISS; }

    private:
        enum Kind { KIND_MISS, KIND_CONTINUE, KIND_LOOP_CONTINUE, KIND_RETURN };

        explicit SpellHandlerOutcome(Kind kind) : m_kind(kind) {}

        Kind m_kind;
};

/// A site's handler type: SpellHandlerOutcome<Site::Value> (*)(Site::Context&).
template <class Site>
struct SpellHandler
{
    typedef SpellHandlerOutcome<typename Site::Value> Outcome;
    typedef Outcome (*Function)(typename Site::Context& context);
};

/// The (site, spell id) table.
class SpellHandlerRegistry
{
    public:
        /// The game's table: every site's registration function has run on it once. First built at
        /// world init (World::SetInitialWorldSettings, after the spell tables load).
        static SpellHandlerRegistry const& Game();

        /// Registers `function` for (Site, spellId). False, and nothing changes, when the key is
        /// taken or `function` is NULL.
        template <class Site>
        bool Register(uint32 spellId, typename SpellHandler<Site>::Function function)
        {
            return Insert(Site::Key, spellId, reinterpret_cast<ErasedFunction>(function));
        }

        /// Registers Site's `default:` body. False, and nothing changes, when Site has one already
        /// or `function` is NULL.
        template <class Site>
        bool RegisterDefault(typename SpellHandler<Site>::Function function)
        {
            return InsertDefault(Site::Key, reinterpret_cast<ErasedFunction>(function));
        }

        /// The function registered for (Site, spellId); NULL (the miss marker) when there is none.
        template <class Site>
        typename SpellHandler<Site>::Function Find(uint32 spellId) const
        {
            return reinterpret_cast<typename SpellHandler<Site>::Function>(Lookup(Site::Key, spellId));
        }

        /// Site's `default:` body; NULL when the site has none.
        template <class Site>
        typename SpellHandler<Site>::Function FindDefault() const
        {
            return reinterpret_cast<typename SpellHandler<Site>::Function>(LookupDefault(Site::Key));
        }

        /// What the switch at Site did for spellId: the registered handler's outcome; on a miss,
        /// the site's default's outcome; with neither, Miss().
        template <class Site>
        typename SpellHandler<Site>::Outcome Dispatch(uint32 spellId, typename Site::Context& context) const
        {
            if (typename SpellHandler<Site>::Function function = Find<Site>(spellId))
            {
                return function(context);
            }
            if (typename SpellHandler<Site>::Function onMiss = FindDefault<Site>())
            {
                return onMiss(context);
            }
            return SpellHandler<Site>::Outcome::Miss();
        }

        /// The number of (site, spell id) keys, and of them the ones at `site`.
        std::size_t Count() const { return m_handlers.size(); }
        std::size_t CountAt(uint32 site) const;
        /// The number of sites with a registered `default:` body.
        std::size_t CountDefaults() const { return m_defaults.size(); }

    private:
        typedef void (*ErasedFunction)();

        static uint64 MakeKey(uint32 site, uint32 spellId) { return (uint64(site) << 32) | spellId; }

        bool Insert(uint32 site, uint32 spellId, ErasedFunction function);
        bool InsertDefault(uint32 site, ErasedFunction function);
        ErasedFunction Lookup(uint32 site, uint32 spellId) const;
        ErasedFunction LookupDefault(uint32 site) const;

        std::map<uint64, ErasedFunction> m_handlers;
        std::map<uint32, ErasedFunction> m_defaults;
};

#endif
