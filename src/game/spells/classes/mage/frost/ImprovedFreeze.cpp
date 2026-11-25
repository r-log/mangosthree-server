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
#include "spells/classes/mage/frost/FrostShared.h"
#include "spells/pipeline/Spell.h"
#include "spells/data/SpellMgr.h"
#include "spells/SpellModMgr.h"
#include "entities/Unit.h"
#include "entities/Creature.h"
#include "entities/Pet.h"
#include "DBCStores.h"
#include "Utilities/Util.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

namespace
{
    uint32 const IMPROVED_FREEZE_RANK_ONE = 86259;
    uint32 const IMPROVED_FREEZE_RANK_TWO = 86260;
    uint32 const IMPROVED_FREEZE_RANK_THREE = 86314;

    /// Improved Freeze's text: "a $s1% chance to grant 2 charges of Fingers of Frost". No field of the data holds
    /// the 2.
    uint32 const IMPROVED_FREEZE_CHARGES = 2;

    /// 33395: a Water Elemental's Freeze: once the cast is over, for a pet whose owner knows Improved Freeze (86314,
    /// 86260 or 86259), one roll of the rank's chance for the whole cast when it hit one or more enemies (the context's
    /// count; Mage::ImprovedFreezeCharges), and on a pass the owner is given two charges of Fingers of Frost
    /// (Mage::GiveFingers). Nothing for a pet without an owner or an owner without the talent.
    SpellHandlerOutcome<void> FreezeAfterCast(AfterCastContext& ctx)
    {
        Unit* owner = ctx.caster.GetOwner();
        if (!owner)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        uint32 const talent = owner->HasAura(IMPROVED_FREEZE_RANK_THREE) ? IMPROVED_FREEZE_RANK_THREE :
                              owner->HasAura(IMPROVED_FREEZE_RANK_TWO) ? IMPROVED_FREEZE_RANK_TWO :
                              owner->HasAura(IMPROVED_FREEZE_RANK_ONE) ? IMPROVED_FREEZE_RANK_ONE : 0;
        uint32 const charges = Mage::ImprovedFreezeCharges(ctx.enemiesHit, Mage::ImprovedFreezeChance(talent),
                                                           [](int32 chance) { return roll_chance_i(chance); });
        if (charges)
        {
            Mage::GiveFingers(*owner, charges);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const WATER_ELEMENTAL_FREEZE[] = { 33395 };
}

int32 Mage::ImprovedFreezeChance(uint32 talentSpell)
{
    if (talentSpell != IMPROVED_FREEZE_RANK_ONE && talentSpell != IMPROVED_FREEZE_RANK_TWO &&
        talentSpell != IMPROVED_FREEZE_RANK_THREE)
    {
        return 0;
    }
    return EffectValue(talentSpell, EFFECT_INDEX_0);
}

uint32 Mage::ImprovedFreezeCharges(uint32 enemiesHit, int32 chance, std::function<bool(int32)> const& roll)
{
    if (enemiesHit == 0 || chance <= 0 || !roll(chance))
    {
        return 0;
    }
    return IMPROVED_FREEZE_CHARGES;
}

namespace Mage
{
    uint32 RegisterImprovedFreeze(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<AfterCastHook>(registry, WATER_ELEMENTAL_FREEZE, &FreezeAfterCast);
    }
}
