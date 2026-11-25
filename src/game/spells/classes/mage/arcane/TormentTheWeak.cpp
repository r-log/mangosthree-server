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

#include "spells/classes/mage/Mage.h"
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"
#include "spells/SpellScriptQueries.h"
#include "entities/Unit.h"
#include "DBCStores.h"
#include "Log.h"

#include <algorithm>
#include <iterator>

namespace
{
    uint32 const TORMENT_THE_WEAK[] = { 29447, 55339, 55340 };

    /// 29447, 55339, 55340: Torment the Weak, run for the rank the caster holds: his mage spells of the Arcane school
    /// deal the rank's percent more damage to a target that is snared or slowed (Mage::TormentTheWeakMultiplier,
    /// Mage::IsSnaredOrSlowed).
    SpellHandlerOutcome<void> TormentTheWeakDamageDone(DamageDoneContext& ctx)
    {
        int32 percent = 0;
        for (uint32 rank : TORMENT_THE_WEAK)
        {
            if (SpellAuraHolder* holder = ctx.caster.GetSpellAuraHolder(rank))
            {
                percent = AuraHandle(*holder).Amount(EFFECT_INDEX_0);
            }
        }
        uint32 const family = ctx.spell.GetSpellFamilyName();
        bool const slowed = Mage::IsSnaredOrSlowed(ctx.victim);
        ctx.doneTotalMod *= Mage::TormentTheWeakMultiplier(percent, family, ctx.spell.SchoolMask, slowed);
        return SpellHandlerOutcome<void>::Continue();
    }

    /// The aura types whose negative amount slows the unit they are on, for Torment the Weak: its movement, its
    /// attacks or its casts.
    AuraType const SLOW_AURA_TYPES[] =
    {
        SPELL_AURA_MOD_DECREASE_SPEED,
        SPELL_AURA_MOD_ATTACKSPEED, SPELL_AURA_MOD_ATTACKSPEED_2, SPELL_AURA_MOD_MELEE_HASTE,
        SPELL_AURA_MOD_RANGED_HASTE, SPELL_AURA_MOD_MELEE_RANGED_HASTE, SPELL_AURA_HASTE_RANGED,
        SPELL_AURA_MOD_MELEE_ATTACK_SPEED, SPELL_AURA_MOD_RANGED_ATTACK_SPEED,
        SPELL_AURA_MOD_CASTING_SPEED_NOT_STACK, SPELL_AURA_HASTE_SPELLS,
        SPELL_AURA_HASTE_ALL, SPELL_AURA_SLOW_ALL
    };
}

bool Mage::CountsAsSlow(uint32 auraType, int32 amount)
{
    if (amount >= 0)
    {
        return false;
    }
    for (AuraType type : SLOW_AURA_TYPES)
    {
        if (auraType == uint32(type))
        {
            return true;
        }
    }
    return false;
}

bool Mage::IsSnaredOrSlowed(Unit const& unit)
{
    for (AuraType type : SLOW_AURA_TYPES)
    {
        if (CountsAsSlow(type, unit.GetMaxNegativeAuraModifier(type)))
        {
            return true;
        }
    }
    return false;
}

float Mage::TormentTheWeakMultiplier(int32 percent, uint32 spellFamily, uint32 schoolMask, bool targetSlowed)
{
    if (percent <= 0 || !targetSlowed || spellFamily != SPELLFAMILY_MAGE ||
        (schoolMask & SPELL_SCHOOL_MASK_ARCANE) == 0)
    {
        return 1.0f;
    }
    return (100.0f + percent) / 100.0f;
}

namespace Mage
{
    uint32 RegisterTormentTheWeak(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<DamageDoneHook>(registry, TORMENT_THE_WEAK, &TormentTheWeakDamageDone);
    }
}
