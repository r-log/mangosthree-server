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
    uint32 const IMPROVED_MANA_GEM[] = { 31584, 31585 };
    uint32 const IMPROVED_MANA_GEM_BUFF = 83098;
    uint32 const MANA_GEMS[] = { 5513, 5514, 8007, 8008, 22044, 33312, 36799 };

    /// 5405: Replenish Mana, the Mana Gem's spell: once its cast from a Mana Gem (Mage::IsManaGem) is over, a mage who
    /// knows a rank of Improved Mana Gem casts 83098 on himself, both of its effects (spell damage and healing done) at
    /// the rank's percent of his maximum mana (Mage::ImprovedManaGemAmount). A cast from any other item or from no item,
    /// and a caster without the talent, cast nothing.
    SpellHandlerOutcome<void> ReplenishManaAfterCast(AfterCastContext& ctx)
    {
        if (!Mage::IsManaGem(ctx.castItemEntry))
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        int32 percent = 0;
        for (uint32 rank : IMPROVED_MANA_GEM)
        {
            if (SpellAuraHolder* holder = ctx.caster.GetSpellAuraHolder(rank))
            {
                percent = AuraHandle(*holder).Amount(EFFECT_INDEX_0);
            }
        }
        int32 const amount = Mage::ImprovedManaGemAmount(ctx.caster.GetMaxPower(POWER_MANA), percent);
        if (amount > 0)
        {
            int32 const basePoints[MAX_EFFECT_INDEX] = { amount, amount, 0 };
            ctx.CastTriggered(ctx.caster, IMPROVED_MANA_GEM_BUFF, basePoints);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const REPLENISH_MANA[] = { 5405 };
}

int32 Mage::ImprovedManaGemAmount(uint32 maxMana, int32 percent)
{
    if (percent <= 0)
    {
        return 0;
    }
    return int32(uint64(maxMana) * uint64(percent) / 100);
}

bool Mage::IsManaGem(uint32 itemEntry)
{
    return std::find(std::begin(MANA_GEMS), std::end(MANA_GEMS), itemEntry) != std::end(MANA_GEMS);
}

namespace Mage
{
    uint32 RegisterImprovedManaGem(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<AfterCastHook>(registry, REPLENISH_MANA, &ReplenishManaAfterCast);
    }
}
