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

#include "spells/handlers/SpellEffectSkillEnchantPetHandlers.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "WorldHandlers/Spell.h"

/// 66765, 66809, 67331, 67333: Meteor Fists; 69055: Bone Slice; 71021: Saber Lash: the final damage is
/// divided among the targets the effect hits
static SpellHandlerOutcome<void> EffectWeaponDmgDivideAmongTargets(SpellEffectWeaponDmgContext& ctx)
{
        // for spells with divided damage to targets
    uint32 count = 0;
    for(Spell::TargetList::const_iterator ihit = ctx.m_UniqueTargetInfo.begin(); ihit != ctx.m_UniqueTargetInfo.end(); ++ihit)
        if (ihit->effectMask & (1<<ctx.effect->EffectIndex))
        {
            ++count;
        }

    ctx.totalDamagePercentMod /= float(count);  // divide to all targets
    return SpellHandlerOutcome<void>::Continue();
}

template <class Site>
struct SpellEffectSkillEnchantPetRow
{
    uint32 spellId;
    typename SpellHandler<Site>::Function function;
};

/// Registers every row of one site's table on `registry`; answers the number of rows.
template <class Site, std::size_t N>
static uint32 RegisterSpellEffectSkillEnchantPetRows(SpellHandlerRegistry& registry,
                                                     SpellEffectSkillEnchantPetRow<Site> const (&rows)[N])
{
    for (SpellEffectSkillEnchantPetRow<Site> const& row : rows)
    {
        registry.Register<Site>(row.spellId, row.function);
    }
    return uint32(N);
}

uint32 RegisterSpellEffectSkillEnchantPetHandlers(SpellHandlerRegistry& registry)
{
    static SpellEffectSkillEnchantPetRow<SpellEffectWeaponDmgSite> const weaponDamage[] =
    {
        { 66765, &EffectWeaponDmgDivideAmongTargets },
        { 66809, &EffectWeaponDmgDivideAmongTargets },
        { 67331, &EffectWeaponDmgDivideAmongTargets },
        { 67333, &EffectWeaponDmgDivideAmongTargets },
        { 69055, &EffectWeaponDmgDivideAmongTargets },
        { 71021, &EffectWeaponDmgDivideAmongTargets },
    };

    return RegisterSpellEffectSkillEnchantPetRows(registry, weaponDamage);
}
