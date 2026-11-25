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

#include "spells/classes/priest/Shield.h"
#include "entities/Unit.h"

#include <cstddef>

namespace
{
    uint32 const POWER_WORD_SHIELD = 17;
    uint32 const SHIELD_DISCIPLINE = 77484;

    /// The id the shield's script holds at the aura-apply hook.
    uint32 const SHIELD_IDS[] = { POWER_WORD_SHIELD };

    /// Improved Power Word: Shield's ranks, the highest first: a priest knows one of them.
    uint32 const IMPROVED_POWER_WORD_SHIELD[] = { 14768, 14748 };

    /// The effect 0 amount of `unit`'s holder of the first of `ranks` he holds; 0 when he holds none.
    template <std::size_t N>
    int32 RankAmount(Unit const& unit, uint32 const (&ranks)[N])
    {
        for (uint32 rank : ranks)
        {
            if (SpellAuraHolder* holder = unit.GetSpellAuraHolder(rank))
            {
                return AuraHandle(*holder).Amount(EFFECT_INDEX_0);
            }
        }
        return 0;
    }

    /// 17: Power Word: Shield: at the holder's add, its amount, the base and the spell power part the absorb aura took,
    /// is raised by the effect 0 percent of the Improved Power Word: Shield rank its caster holds and by his Shield
    /// Discipline mastery (Priest::ShieldAmount, Priest::AbsorbMastery), so the holder's boosts, Glyph of Power Word:
    /// Shield's heal among them, read the raised amount. The write tells the clients a raised amount; they were sent the
    /// holder's slot before the absorb aura took its spell power part, so a shield left as it is shows them its base
    /// until a hit sends its amount. A shield loaded with its owner keeps the amount it was saved with, and a shield
    /// whose caster cannot be found is left as it is. The holder's boosts still run.
    SpellHandlerOutcome<void> ShieldAuraApply(AuraApplyContext& ctx)
    {
        if (!ctx.real || ctx.target.IsLoading() || !ctx.caster)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        int32 const amount = ctx.aura.Amount(EFFECT_INDEX_0);
        int32 const raised = Priest::ShieldAmount(amount, RankAmount(*ctx.caster, IMPROVED_POWER_WORD_SHIELD),
                                                  Priest::AbsorbMastery(*ctx.caster));
        ctx.aura.SetAmount(EFFECT_INDEX_0, raised);
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterShield(SpellHandlerRegistry& registry)
{
    return RegisterRanks<AuraApplyHook>(registry, SHIELD_IDS, &ShieldAuraApply);
}

int32 Priest::AbsorbMastery(Unit const& caster)
{
    SpellAuraHolder* holder = caster.GetSpellAuraHolder(SHIELD_DISCIPLINE);
    return holder ? AuraHandle(*holder).Amount(EFFECT_INDEX_0) : 0;
}

int32 Priest::ShieldAmount(int32 amount, int32 improvedPercent, int32 masteryPercent)
{
    if (amount <= 0)
    {
        return amount;
    }
    int64 const improved = 100 + (improvedPercent > 0 ? improvedPercent : 0);
    int64 const mastery = 100 + (masteryPercent > 0 ? masteryPercent : 0);
    return int32(int64(amount) * improved * mastery / 10000);
}
