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


#ifndef MANGOSSERVER_SPELLS_HANDLERS_SPELLEFFECTSCRIPTHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_SPELLEFFECTSCRIPTHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"

class Item;
class ObjectGuid;
class Spell;
class SpellCastTargets;
class Unit;
struct SpellEffectEntry;
struct SpellEntry;

/**
 * @file SpellEffectScriptHandlers.h
 * @brief The `Spell::EffectScriptEffect` sites of the spell handler registry.
 *
 * The handlers are file-static functions in SpellEffectScriptHandlers.cpp; this header holds what the
 * registry and tests see.
 */

/// What the bodies of `Spell::EffectScriptEffect`'s switches read: the spell, its entry, its targets, its caster, its
/// original caster's guid and that caster, its unit and item targets, the damage and the script effect.
struct SpellEffectScriptContext
{
    SpellEffectScriptContext(Spell* thisSpell, SpellEntry const*& spellInfoMember, SpellCastTargets& targetsMember,
                             Unit*& casterMember, ObjectGuid& originalCasterGuidMember, Unit*& originalCasterMember,
                             Unit*& targetMember, Item*& itemTargetMember, int32& damageMember,
                             SpellEffectEntry const* const& effectParameter)
        : spell(thisSpell), m_spellInfo(spellInfoMember), m_targets(targetsMember), m_caster(casterMember),
          m_originalCasterGUID(originalCasterGuidMember), m_originalCaster(originalCasterMember),
          unitTarget(targetMember), itemTarget(itemTargetMember), damage(damageMember), effect(effectParameter) {}

    Spell* const spell;                    ///< the spell itself
    SpellEntry const*& m_spellInfo;        ///< the spell's member `SpellEntry const* m_spellInfo;`
    SpellCastTargets& m_targets;           ///< the spell's member `SpellCastTargets m_targets;`
    Unit*& m_caster;                       ///< the spell's member `Unit* m_caster;`
    ObjectGuid& m_originalCasterGUID;      ///< the spell's member `ObjectGuid m_originalCasterGUID;`
    Unit*& m_originalCaster;               ///< the spell's member `Unit* m_originalCaster;`
    Unit*& unitTarget;                     ///< the spell's member `Unit* unitTarget;`
    Item*& itemTarget;                     ///< the spell's member `Item* itemTarget;`
    int32& damage;                         ///< the spell's member `int32 damage;`
    SpellEffectEntry const* const& effect; ///< the function's parameter `SpellEffectEntry const* effect`
};

/// `Spell::EffectScriptEffect`, SPELLFAMILY_GENERIC's `switch (m_spellInfo->ID)`: a registered label that returns
/// ends the effect; one that breaks, and a miss, since there is no `default:`, go on to the database script.
struct SpellEffectScriptGenericSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_SCRIPT_GENERIC;
    typedef void Value;
    typedef SpellEffectScriptContext Context;
};

/// `Spell::EffectScriptEffect`, SPELLFAMILY_WARLOCK's `switch (m_spellInfo->ID)`: every label ends the effect; there
/// is no `default:`, so a miss goes on to the database script.
struct SpellEffectScriptWarlockSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_SCRIPT_WARLOCK;
    typedef void Value;
    typedef SpellEffectScriptContext Context;
};

/// `Spell::EffectScriptEffect`, SPELLFAMILY_PRIEST's `switch (m_spellInfo->ID)`: the label ends the effect; the
/// `default:` does nothing, so the effect goes on to the database script.
struct SpellEffectScriptPriestSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_SCRIPT_PRIEST;
    typedef void Value;
    typedef SpellEffectScriptContext Context;
};

/// `Spell::EffectScriptEffect`, SPELLFAMILY_HUNTER's `switch (m_spellInfo->ID)`: every label ends the effect; the
/// `default:` does nothing, so the effect goes on to the database script.
struct SpellEffectScriptHunterSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_SCRIPT_HUNTER;
    typedef void Value;
    typedef SpellEffectScriptContext Context;
};

/// `Spell::EffectScriptEffect`, SPELLFAMILY_POTION's `switch (m_spellInfo->ID)`: a label that returns ends the effect;
/// one that breaks goes on to the database script; there is no `default:`.
struct SpellEffectScriptPotionSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_SCRIPT_POTION;
    typedef void Value;
    typedef SpellEffectScriptContext Context;
};

/// `Spell::EffectScriptEffect`, SPELLFAMILY_DEATHKNIGHT's `switch (m_spellInfo->ID)`: a label that returns ends the
/// effect; one that breaks goes on to the database script; there is no `default:`.
struct SpellEffectScriptDeathKnightSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_SCRIPT_DEATH_KNIGHT;
    typedef void Value;
    typedef SpellEffectScriptContext Context;
};

/// Registers every `SpellEffectScript.cpp` handler on `registry` (defined in SpellEffectScriptHandlers.cpp).
/// Returns the number of rows it registered, a site's `default:` counting as one; a row whose key was taken,
/// or a second default at a site, does not change the table, so a caller that registers on an empty table
/// and finds fewer keys and defaults than rows has a duplicate.
uint32 RegisterSpellEffectScriptHandlers(SpellHandlerRegistry& registry);

#endif
