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
 *   case's `break`;
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
    SPELL_HANDLER_SITE_AURA_INCREASE_HEALTH = 15
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
        /// No handler and no default at the site: the site continues after where its switch stood.
        static SpellHandlerOutcome Miss() { return SpellHandlerOutcome(KIND_MISS, V()); }

        bool IsReturn() const { return m_kind == KIND_RETURN; }
        bool IsContinue() const { return m_kind == KIND_CONTINUE; }
        bool IsMiss() const { return m_kind == KIND_MISS; }
        /// The returned value; meaningful only when IsReturn().
        V GetValue() const { return m_value; }

    private:
        enum Kind { KIND_MISS, KIND_CONTINUE, KIND_RETURN };

        SpellHandlerOutcome(Kind kind, V value) : m_kind(kind), m_value(value) {}

        Kind m_kind;
        V m_value;
};

/// The outcome at a site in a void function: `return;` or `break;`.
template <>
class SpellHandlerOutcome<void>
{
    public:
        /// The case's `return;`.
        static SpellHandlerOutcome Return() { return SpellHandlerOutcome(KIND_RETURN); }
        /// The case's `break;`: the site continues after where its switch stood.
        static SpellHandlerOutcome Continue() { return SpellHandlerOutcome(KIND_CONTINUE); }
        /// No handler and no default at the site: the site continues after where its switch stood.
        static SpellHandlerOutcome Miss() { return SpellHandlerOutcome(KIND_MISS); }

        bool IsReturn() const { return m_kind == KIND_RETURN; }
        bool IsContinue() const { return m_kind == KIND_CONTINUE; }
        bool IsMiss() const { return m_kind == KIND_MISS; }

    private:
        enum Kind { KIND_MISS, KIND_CONTINUE, KIND_RETURN };

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
