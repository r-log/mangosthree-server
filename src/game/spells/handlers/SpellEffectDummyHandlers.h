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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_SPELLEFFECTDUMMYHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_SPELLEFFECTDUMMYHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"
#include "WorldHandlers/Spell.h"

/**
 * @file SpellEffectDummyHandlers.h
 * @brief The `Spell::EffectDummy` sites of the spell handler registry.
 *
 * The handlers are file-static functions in SpellEffectDummyHandlers.cpp; this header holds what the
 * registry and tests see. The paladin context holds the spell's target list by its own type, so this
 * header includes the spell's.
 */

/// What the bodies of `Spell::EffectDummy`'s SPELLFAMILY_MAGE switch read and write: the spell, its caster, its unit
/// target, the damage (which the spell's item creation reads as the stack size) and the dummy effect.
struct SpellEffectDummyMageContext
{
    SpellEffectDummyMageContext(Spell* thisSpell, Unit*& casterMember, Unit*& targetMember, int32& damageMember,
                                SpellEffectEntry const* const& effectParameter)
        : spell(thisSpell), m_caster(casterMember), unitTarget(targetMember), damage(damageMember),
          effect(effectParameter) {}

    Spell* const spell;                     ///< the spell itself
    Unit*& m_caster;                        ///< the spell's member `Unit* m_caster;`
    Unit*& unitTarget;                      ///< the spell's member `Unit* unitTarget;`
    int32& damage;                          ///< the spell's member `int32 damage;`
    SpellEffectEntry const* const& effect;  ///< the function's parameter `SpellEffectEntry const* effect`
};

/// `Spell::EffectDummy`, SPELLFAMILY_MAGE: its `switch (m_spellInfo->ID)`. Every label ends the effect; there is no
/// `default:`, so a miss goes on to the Conjure Mana Gem check after the switch.
struct SpellEffectDummyMageSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_MAGE;
    typedef void Value;
    typedef SpellEffectDummyMageContext Context;
};

/// What the bodies of `Spell::EffectDummy`'s SPELLFAMILY_WARRIOR switch read: the spell's caster, its unit target and
/// the damage.
struct SpellEffectDummyWarriorContext
{
    SpellEffectDummyWarriorContext(Unit*& casterMember, Unit*& targetMember, int32& damageMember)
        : m_caster(casterMember), unitTarget(targetMember), damage(damageMember) {}

    Unit*& m_caster;    ///< the spell's member `Unit* m_caster;`
    Unit*& unitTarget;  ///< the spell's member `Unit* unitTarget;`
    int32& damage;      ///< the spell's member `int32 damage;`
};

/// `Spell::EffectDummy`, SPELLFAMILY_WARRIOR, after the class-mask checks: its `switch (m_spellInfo->ID)`. Every label
/// ends the effect, Move's after Change Facing's body; there is no `default:`, so a miss leaves the family case for the
/// pet aura, script and database script steps.
struct SpellEffectDummyWarriorSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_WARRIOR;
    typedef void Value;
    typedef SpellEffectDummyWarriorContext Context;
};

/// What the bodies of `Spell::EffectDummy`'s SPELLFAMILY_ROGUE switch read: the spell's caster and its unit target.
struct SpellEffectDummyRogueContext
{
    SpellEffectDummyRogueContext(Unit*& casterMember, Unit*& targetMember)
        : m_caster(casterMember), unitTarget(targetMember) {}

    Unit*& m_caster;    ///< the spell's member `Unit* m_caster;`
    Unit*& unitTarget;  ///< the spell's member `Unit* unitTarget;`
};

/// `Spell::EffectDummy`, SPELLFAMILY_ROGUE: its `switch (m_spellInfo->ID)`. Every label ends the effect; there is no
/// `default:`, so a miss leaves the family case for the pet aura, script and database script steps.
struct SpellEffectDummyRogueSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_ROGUE;
    typedef void Value;
    typedef SpellEffectDummyRogueContext Context;
};

/// What the bodies of `Spell::EffectDummy`'s SPELLFAMILY_HUNTER switch read: the spell's caster, its unit target and
/// the dummy effect.
struct SpellEffectDummyHunterContext
{
    SpellEffectDummyHunterContext(Unit*& casterMember, Unit*& targetMember,
                                  SpellEffectEntry const* const& effectParameter)
        : m_caster(casterMember), unitTarget(targetMember), effect(effectParameter) {}

    Unit*& m_caster;                        ///< the spell's member `Unit* m_caster;`
    Unit*& unitTarget;                      ///< the spell's member `Unit* unitTarget;`
    SpellEffectEntry const* const& effect;  ///< the function's parameter `SpellEffectEntry const* effect`
};

/// `Spell::EffectDummy`, SPELLFAMILY_HUNTER, after the class-mask checks: its `switch (m_spellInfo->ID)`. Every label
/// ends the effect; there is no `default:`, so a miss leaves the family case for the pet aura, script and database
/// script steps.
struct SpellEffectDummyHunterSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_HUNTER;
    typedef void Value;
    typedef SpellEffectDummyHunterContext Context;
};

/// What the bodies of `Spell::EffectDummy`'s SPELLFAMILY_PALADIN switch read and write: the spell, its caster, its unit
/// target, its entry, its base points, its targets and the dummy effect.
struct SpellEffectDummyPaladinContext
{
    SpellEffectDummyPaladinContext(Spell* thisSpell, Unit*& casterMember, Unit*& targetMember,
                                   SpellEntry const*& spellInfoMember, int32 (&basePointsMember)[MAX_EFFECT_INDEX],
                                   Spell::TargetList& targetsMember, SpellEffectEntry const* const& effectParameter)
        : spell(thisSpell), m_caster(casterMember), unitTarget(targetMember), m_spellInfo(spellInfoMember),
          m_currentBasePoints(basePointsMember), m_UniqueTargetInfo(targetsMember), effect(effectParameter) {}

    Spell* const spell;                              ///< the spell itself
    Unit*& m_caster;                                 ///< the spell's member `Unit* m_caster;`
    Unit*& unitTarget;                               ///< the spell's member `Unit* unitTarget;`
    SpellEntry const*& m_spellInfo;                  ///< the spell's member `SpellEntry const* m_spellInfo;`
    int32 (&m_currentBasePoints)[MAX_EFFECT_INDEX];  ///< the spell's member array `m_currentBasePoints`
    Spell::TargetList& m_UniqueTargetInfo;           ///< the spell's member `TargetList m_UniqueTargetInfo;`
    SpellEffectEntry const* const& effect;           ///< the function's parameter `SpellEffectEntry const* effect`
};

/// `Spell::EffectDummy`, SPELLFAMILY_PALADIN, after the Holy Shock icon switch: its `switch (m_spellInfo->ID)`. Every
/// label ends the effect; there is no `default:`, so a miss leaves the family case for the pet aura, script and
/// database script steps.
struct SpellEffectDummyPaladinSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_DUMMY_PALADIN;
    typedef void Value;
    typedef SpellEffectDummyPaladinContext Context;
};

/// Registers every `Spell::EffectDummy` handler on `registry` (defined in SpellEffectDummyHandlers.cpp).
/// Returns the number of rows it registered; a row whose key was taken does not change the table, so a caller
/// that registers on an empty table and finds fewer keys than rows has a duplicate.
uint32 RegisterSpellEffectDummyHandlers(SpellHandlerRegistry& registry);

#endif
