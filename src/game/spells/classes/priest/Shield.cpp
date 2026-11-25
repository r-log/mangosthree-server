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
    uint32 const RAPTURE_ENERGIZE = 47755;
    uint32 const RAPTURE_LOCKOUT = 63853;

    /// The ids the shield's scripts hold at the aura-apply and aura-remove hooks.
    uint32 const SHIELD_IDS[] = { POWER_WORD_SHIELD };

    /// Improved Power Word: Shield's ranks and Rapture's, the highest first: a priest knows one rank of each.
    uint32 const IMPROVED_POWER_WORD_SHIELD[] = { 14768, 14748 };
    uint32 const RAPTURE[] = { 47537, 47536, 47535 };

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

    /// 17: Power Word: Shield: when damage used up its last point or it was dispelled (Priest::RaptureFires), a caster
    /// who knows Rapture and does not hold its lockout (63853) casts at himself the energize (47755) for the effect 0
    /// percent of his rank of his maximum mana (Priest::RaptureMana), then the lockout, which holds the next 12 s. The
    /// shield may be on any unit: the mana and the lockout are the caster's. Any other removal, a caster who cannot be
    /// found and a rank that holds no percent cast nothing. The removal's boosts still run.
    SpellHandlerOutcome<void> ShieldAuraRemove(AuraRemoveContext& ctx)
    {
        if (!ctx.caster || !Priest::RaptureFires(ctx.mode) || ctx.caster->HasAura(RAPTURE_LOCKOUT))
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        int32 const mana = Priest::RaptureMana(ctx.caster->GetMaxPower(POWER_MANA), RankAmount(*ctx.caster, RAPTURE));
        if (mana <= 0)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        int32 const basePoints[MAX_EFFECT_INDEX] = { mana, 0, 0 };
        ctx.caster->CastCustomSpell(ctx.caster, RAPTURE_ENERGIZE, &basePoints[EFFECT_INDEX_0], NULL, NULL, true);
        ctx.caster->CastSpell(ctx.caster, RAPTURE_LOCKOUT, true);
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterShield(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<AuraApplyHook>(registry, SHIELD_IDS, &ShieldAuraApply);
    rows += RegisterRanks<AuraRemoveHook>(registry, SHIELD_IDS, &ShieldAuraRemove);
    return rows;
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

bool Priest::RaptureFires(AuraRemoveMode mode)
{
    return mode == AURA_REMOVE_BY_SHIELD_BREAK || mode == AURA_REMOVE_BY_DISPEL;
}

int32 Priest::RaptureMana(uint32 maxMana, int32 percent)
{
    if (percent <= 0)
    {
        return 0;
    }
    return int32(uint64(maxMana) * uint64(percent) / 100);
}
