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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_SPELLEFFECTOBJECTCOMBATHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_SPELLEFFECTOBJECTCOMBATHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"

class GameObject;
class Item;
class Unit;
struct SpellEntry;

/**
 * @file SpellEffectObjectCombatHandlers.h
 * @brief The `Spell::EffectActivateObject` and `Spell::EffectResurrect` sites of the spell handler registry.
 *
 * The handlers are file-static functions in SpellEffectObjectCombatHandlers.cpp; this header holds what the registry
 * and tests see.
 */

/// What the bodies of `Spell::EffectActivateObject`'s custom-use switch read: the game object the spell targets, the
/// spell's caster, and the spell's entry.
struct SpellEffectActivateObjectContext
{
    SpellEffectActivateObjectContext(GameObject*& targetMember, Unit*& casterMember, SpellEntry const*& spellInfoMember)
        : gameObjTarget(targetMember), m_caster(casterMember), m_spellInfo(spellInfoMember) {}

    GameObject*& gameObjTarget;     ///< the spell's member `GameObject* gameObjTarget;`
    Unit*& m_caster;                ///< the spell's member `Unit* m_caster;`
    SpellEntry const*& m_spellInfo; ///< the spell's member `SpellEntry const* m_spellInfo;`
};

/// `Spell::EffectActivateObject`, the custom-use misc value's `switch (m_spellInfo->ID)`: the labels summon at the
/// game object, change its flags or spend it; there is no `default:`, so a miss continues after the switch.
struct SpellEffectActivateObjectSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_ACTIVATE_OBJECT;
    typedef void Value;
    typedef SpellEffectActivateObjectContext Context;
};

/// What the bodies of `Spell::EffectResurrect`'s switch read: the spell's caster, the item it is cast with, and the
/// spell's entry.
struct SpellEffectResurrectContext
{
    SpellEffectResurrectContext(Unit*& casterMember, Item*& castItemMember, SpellEntry const*& spellInfoMember)
        : m_caster(casterMember), m_CastItem(castItemMember), m_spellInfo(spellInfoMember) {}

    Unit*& m_caster;                ///< the spell's member `Unit* m_caster;`
    Item*& m_CastItem;              ///< the spell's member `Item* m_CastItem;`
    SpellEntry const*& m_spellInfo; ///< the spell's member `SpellEntry const* m_spellInfo;`
};

/// `Spell::EffectResurrect`, its `switch (m_spellInfo->ID)` before the request is sent: the labels roll for failure
/// and return on it; the `default:` changes nothing; a success and the default continue after the switch.
struct SpellEffectResurrectSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_RESURRECT;
    typedef void Value;
    typedef SpellEffectResurrectContext Context;
};

/// Registers every `Spell::EffectActivateObject` and `Spell::EffectResurrect` handler on `registry` (defined in
/// SpellEffectObjectCombatHandlers.cpp). Returns the number of rows it registered, a site's `default:` counting as
/// one; a row whose key was taken, or a second default, does not change the table, so a caller that registers on an
/// empty table and finds fewer keys and defaults than rows has a duplicate.
uint32 RegisterSpellEffectObjectCombatHandlers(SpellHandlerRegistry& registry);

#endif
