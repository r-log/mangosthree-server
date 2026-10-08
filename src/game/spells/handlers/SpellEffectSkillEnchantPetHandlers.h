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

#ifndef MANGOSSERVER_SPELLS_HANDLERS_SPELLEFFECTSKILLENCHANTPETHANDLERS_H
#define MANGOSSERVER_SPELLS_HANDLERS_SPELLEFFECTSKILLENCHANTPETHANDLERS_H

#include "spells/handlers/SpellHandlerRegistry.h"
#include "WorldHandlers/Spell.h"

/**
 * @file SpellEffectSkillEnchantPetHandlers.h
 * @brief The `Spell::EffectWeaponDmg` site of the spell handler registry.
 *
 * The handlers are file-static functions in SpellEffectSkillEnchantPetHandlers.cpp; this header holds what
 * the registry and tests see. The context holds the spell's target list by its own type, so this header
 * includes the spell's.
 */

/// What the bodies of `Spell::EffectWeaponDmg`'s SPELLFAMILY_GENERIC switch read and write: the spell's
/// targets, the weapon damage effect and the multiplier applied to the final damage.
struct SpellEffectWeaponDmgContext
{
    SpellEffectWeaponDmgContext(Spell::TargetList& targetsMember, SpellEffectEntry const* const& effectParameter,
                                float& totalModLocal)
        : m_UniqueTargetInfo(targetsMember), effect(effectParameter), totalDamagePercentMod(totalModLocal) {}

    Spell::TargetList& m_UniqueTargetInfo;  ///< the spell's member `TargetList m_UniqueTargetInfo;`
    SpellEffectEntry const* const& effect;  ///< the function's parameter `SpellEffectEntry const* effect`
    float& totalDamagePercentMod;           ///< the function's local `float totalDamagePercentMod`
};

/// `Spell::EffectWeaponDmg`, SPELLFAMILY_GENERIC: its `switch (m_spellInfo->ID)`. The labels divide the
/// final damage among the targets the effect hits; there is no `default:`, so a miss continues after the
/// switch, as every label does.
struct SpellEffectWeaponDmgSite
{
    static constexpr uint32 Key = SPELL_HANDLER_SITE_SPELL_EFFECT_WEAPON_DAMAGE;
    typedef void Value;
    typedef SpellEffectWeaponDmgContext Context;
};

/// Registers every `Spell::EffectWeaponDmg` handler on `registry` (defined in
/// SpellEffectSkillEnchantPetHandlers.cpp). Returns the number of rows it registered; a row whose key was
/// taken does not change the table, so a caller that registers on an empty table and finds fewer keys than
/// rows has a duplicate.
uint32 RegisterSpellEffectSkillEnchantPetHandlers(SpellHandlerRegistry& registry);

#endif
