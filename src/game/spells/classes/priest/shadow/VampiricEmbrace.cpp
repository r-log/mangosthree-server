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

#include "spells/classes/priest/shadow/VampiricEmbrace.h"
#include "spells/data/SpellMgr.h"
#include "entities/Unit.h"
#include "DBCStores.h"

#include <cstdint>

namespace
{
    /// Vampiric Embrace: its first effect is a dummy aura whose value, 6, is the percent of the damage the priest is
    /// healed for.
    uint32 const VAMPIRIC_EMBRACE[] = { 15286 };

    /// The heal the aura casts at the priest: its effect 0 heals the party around the caster, the caster among them,
    /// and its effect 1 heals the caster.
    uint32 const VAMPIRIC_EMBRACE_HEAL = 15290;

    /// The party's share of the heal: the client's text reads "$/2;15286s1%" for the party, half of the percent the
    /// priest is healed for ("$15286s1%").
    int32 const VAMPIRIC_EMBRACE_PARTY_DIVISOR = 2;

    /// The percent is of a hundred.
    int32 const PERCENT = 100;

    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// A heal amount as the int32 a cast's base points take.
    int32 AsPoints(uint64 amount)
    {
        return amount < uint64(INT32_MAX) ? int32(amount) : INT32_MAX;
    }

    /// 15286: Vampiric Embrace: for a hit or a tick of the priest's that heals (Priest::VampiricEmbraceHeals: not
    /// damage he dealt himself, not an area spell's, and with damage), the priest casts the heal (15290) at himself,
    /// triggered by the aura, with the party's share on its effect 0 (Priest::VampiricEmbracePartyHeal) and his own on
    /// its effect 1 (Priest::VampiricEmbraceSelfHeal) as base points; a hit that heals neither casts nothing. The
    /// aura's effect 0 amount is the percent. FAILED for any event it refuses and for an effect other than the first;
    /// OK once the heal is cast, with no hidden cooldown.
    ProcOutcome VampiricEmbraceProc(OnProcContext& ctx)
    {
        bool const selfDamage = ctx.victim == &ctx.caster;
        bool const areaSpell = ctx.procSpell && IsAreaOfEffectSpell(ctx.procSpell);
        if (ctx.effIndex != EFFECT_INDEX_0 || !Priest::VampiricEmbraceHeals(selfDamage, areaSpell, ctx.damage))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const party = Priest::VampiricEmbracePartyHeal(ctx.triggerAmount, ctx.damage);
        int32 const self = Priest::VampiricEmbraceSelfHeal(ctx.triggerAmount, ctx.damage);
        if (party > 0 || self > 0)
        {
            int32 const basePoints[MAX_EFFECT_INDEX] = { party, self, 0 };
            ctx.CastTriggered(ctx.caster, VAMPIRIC_EMBRACE_HEAL, basePoints);
        }
        return ProcOutcome::Return(SPELL_AURA_PROC_OK);
    }
}

uint32 Priest::RegisterVampiricEmbrace(SpellHandlerRegistry& registry)
{
    return RegisterRanks<OnProcHook>(registry, VAMPIRIC_EMBRACE, &VampiricEmbraceProc);
}

bool Priest::VampiricEmbraceHeals(bool selfDamage, bool areaSpell, uint32 damage)
{
    return !selfDamage && !areaSpell && damage > 0;
}

int32 Priest::VampiricEmbracePartyHeal(int32 percent, uint32 damage)
{
    if (percent <= 0)
    {
        return 0;
    }
    return AsPoints(uint64(percent) * damage / uint64(PERCENT * VAMPIRIC_EMBRACE_PARTY_DIVISOR));
}

int32 Priest::VampiricEmbraceSelfHeal(int32 percent, uint32 damage)
{
    if (percent <= 0)
    {
        return 0;
    }
    return AsPoints(uint64(percent) * damage / uint64(PERCENT)) - VampiricEmbracePartyHeal(percent, damage);
}
