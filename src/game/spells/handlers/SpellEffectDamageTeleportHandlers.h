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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_SPELLEFFECTDAMAGETELEPORTHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_SPELLEFFECTDAMAGETELEPORTHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"
#include "WorldHandlers/Spell.h"

/**
 * @file SpellEffectDamageTeleportHandlers.h
 * @brief The `Spell::EffectSchoolDMG`, `Spell::EffectTriggerSpell` and `Spell::EffectTeleportUnits` sites of the
 * spell handler registry.
 *
 * The handlers are file-static functions in SpellEffectDamageTeleportHandlers.cpp; this header holds what the
 * registry and tests see. The school-damage context holds the spell's target list by its own type, so this
 * header includes the spell's.
 */

/// What the bodies of `Spell::EffectSchoolDMG`'s SPELLFAMILY_GENERIC switch read and write: the spell's caster,
/// its unit target, the damage, the spell's targets and the damage effect.
struct SpellEffectSchoolDmgContext
{
    SpellEffectSchoolDmgContext(Unit*& casterMember, Unit*& targetMember, int32& damageMember,
                                Spell::TargetList& targetsMember, SpellEffectEntry const* const& effectParameter)
        : m_caster(casterMember), unitTarget(targetMember), damage(damageMember), m_UniqueTargetInfo(targetsMember),
          effect(effectParameter) {}

    Unit*& m_caster;                        ///< the spell's member `Unit* m_caster;`
    Unit*& unitTarget;                      ///< the spell's member `Unit* unitTarget;`
    int32& damage;                          ///< the spell's member `int32 damage;`
    Spell::TargetList& m_UniqueTargetInfo;  ///< the spell's member `TargetList m_UniqueTargetInfo;`
    SpellEffectEntry const* const& effect;  ///< the function's parameter `SpellEffectEntry const* effect`
};

/// `Spell::EffectSchoolDMG`, SPELLFAMILY_GENERIC: its `switch (m_spellInfo->ID)`. The labels set or divide the
/// damage or give a kill credit; there is no `default:`, so a miss continues after the switch, as every label
/// does.
struct SpellEffectSchoolDmgSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_SCHOOL_DAMAGE;
    typedef void Value;
    typedef SpellEffectSchoolDmgContext Context;
};

/// What the bodies of `Spell::EffectTriggerSpell`'s switch read: the spell's unit target, its caster, the item it
/// is cast with and its original caster.
struct SpellEffectTriggerSpellContext
{
    SpellEffectTriggerSpellContext(Unit*& targetMember, Unit*& casterMember, Item*& castItemMember,
                                   ObjectGuid& originalCasterMember)
        : unitTarget(targetMember), m_caster(casterMember), m_CastItem(castItemMember),
          m_originalCasterGUID(originalCasterMember) {}

    Unit*& unitTarget;                 ///< the spell's member `Unit* unitTarget;`
    Unit*& m_caster;                   ///< the spell's member `Unit* m_caster;`
    Item*& m_CastItem;                 ///< the spell's member `Item* m_CastItem;`
    ObjectGuid& m_originalCasterGUID;  ///< the spell's member `ObjectGuid m_originalCasterGUID;`
};

/// `Spell::EffectTriggerSpell`, before the triggered spell is looked up: its `switch (triggered_spell_id)`, keyed
/// on the effect's trigger spell. The labels cast or remove spells and end the effect, Mirror Image's goes on to
/// the triggered spell; there is no `default:`, so a miss continues after the switch.
struct SpellEffectTriggerSpellSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_TRIGGER_SPELL;
    typedef void Value;
    typedef SpellEffectTriggerSpellContext Context;
};

/// What the body of `Spell::EffectTeleportUnits`' first switch reads: the spell's unit target and the spell's
/// entry.
struct SpellEffectTeleportRecallContext
{
    SpellEffectTeleportRecallContext(Unit*& targetMember, SpellEntry const*& spellInfoMember)
        : unitTarget(targetMember), m_spellInfo(spellInfoMember) {}

    Unit*& unitTarget;               ///< the spell's member `Unit* unitTarget;`
    SpellEntry const*& m_spellInfo;  ///< the spell's member `SpellEntry const* m_spellInfo;`
};

/// `Spell::EffectTeleportUnits`, before the target type is read: its first `switch (m_spellInfo->ID)`. The label
/// ends the effect for a player above the scroll's level and continues otherwise; there is no `default:`, so a
/// miss continues after the switch.
struct SpellEffectTeleportRecallSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_TELEPORT_RECALL;
    typedef void Value;
    typedef SpellEffectTeleportRecallContext Context;
};

/// What the bodies of `Spell::EffectTeleportUnits`' second switch read: the spell's caster.
struct SpellEffectTeleportPostContext
{
    explicit SpellEffectTeleportPostContext(Unit*& casterMember)
        : m_caster(casterMember) {}

    Unit*& m_caster;  ///< the spell's member `Unit* m_caster;`
};

/// `Spell::EffectTeleportUnits`, after a table-coordinates teleport: its second `switch (m_spellInfo->ID)`. The
/// labels cast a transporter's side effect and end the effect; there is no `default:`, so a miss reaches the
/// end of the function.
struct SpellEffectTeleportPostSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_TELEPORT_POST;
    typedef void Value;
    typedef SpellEffectTeleportPostContext Context;
};

/// Registers every `Spell::EffectSchoolDMG`, `Spell::EffectTriggerSpell` and `Spell::EffectTeleportUnits` handler
/// on `registry` (defined in SpellEffectDamageTeleportHandlers.cpp). Returns the number of rows it registered; a
/// row whose key was taken does not change the table, so a caller that registers on an empty table and finds
/// fewer keys than rows has a duplicate.
uint32 RegisterSpellEffectDamageTeleportHandlers(SpellHandlerRegistry& registry);

#endif
