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

#include "spells/classes/priest/DivineAegis.h"
#include "spells/classes/priest/Shield.h"
#include "spells/data/SpellMgr.h"
#include "entities/Unit.h"

#include <cstdint>

namespace
{
    uint32 const DIVINE_AEGIS_SHIELD = 47753;
    uint32 const PRAYER_OF_HEALING = 596;

    /// The talent's three ranks, whose effect 0 holds the percent of the heal the shield takes (10, 20, 30).
    uint32 const DIVINE_AEGIS_RANKS[] = { 47509, 47511, 47515 };

    /// The percent of the priest's maximum health the sum of an addition onto his own shield on one unit is held to;
    /// the client's data holds no such number.
    uint32 const AEGIS_CAP_PERCENT = 40;

    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// 47509, 47511, 47515: Divine Aegis: its spell_proc_event row lets the normal and the critical hits of the
    /// priest's helpful spells and heal ticks through; the script refuses each event that makes no shield
    /// (Priest::AegisFromHeal), so only a critical heal and a heal of Prayer of Healing reach the proc.
    SpellHandlerOutcome<bool> DivineAegisCheckProc(CheckProcContext& ctx)
    {
        if (!Priest::AegisFromHeal(ctx.procFlag, ctx.procEx, ctx.procSpell ? ctx.procSpell->ID : 0, ctx.damage))
        {
            return SpellHandlerOutcome<bool>::Return(false);
        }
        return SpellHandlerOutcome<bool>::Continue();
    }

    /// 47509, 47511, 47515: Divine Aegis: the priest casts the shield (47753) at the unit he healed, triggered by the
    /// talent, for the shield the heal makes (Priest::AegisShield: his rank's percent, the aura's effect 0 amount, of
    /// the heal the event carries, raised by his Shield Discipline mastery) added onto what is left of his own shield
    /// on that unit, the sum held to his cap but never below what was left nor below the new shield
    /// (Priest::AegisAmount, Priest::AegisCap: 40 percent of his maximum health). The new holder takes the place of his
    /// old one; another caster's shield on the unit stays as it is. FAILED, and nothing cast, for an event with no unit
    /// healed or a heal that makes no shield.
    ProcOutcome DivineAegisProc(OnProcContext& ctx)
    {
        if (ctx.effIndex != EFFECT_INDEX_0 || !ctx.victim)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const fresh = Priest::AegisShield(ctx.damage, ctx.triggerAmount, Priest::AbsorbMastery(ctx.caster));
        if (fresh <= 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        SpellAuraHolder* own = ctx.victim->GetSpellAuraHolder(DIVINE_AEGIS_SHIELD, ctx.caster.GetObjectGuid());
        int32 const left = own ? AuraHandle(*own).Amount(EFFECT_INDEX_0) : 0;
        int32 const amount = Priest::AegisAmount(fresh, left, Priest::AegisCap(ctx.caster.GetMaxHealth()));
        int32 const basePoints[MAX_EFFECT_INDEX] = { amount, 0, 0 };
        return ProcOutcome::Return(ctx.CastTriggered(*ctx.victim, DIVINE_AEGIS_SHIELD, basePoints)
                                   ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED);
    }
}

uint32 Priest::RegisterDivineAegis(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<CheckProcHook>(registry, DIVINE_AEGIS_RANKS, &DivineAegisCheckProc);
    rows += RegisterRanks<OnProcHook>(registry, DIVINE_AEGIS_RANKS, &DivineAegisProc);
    return rows;
}

bool Priest::AegisFromHeal(uint32 procFlag, uint32 procEx, uint32 spellId, uint32 amount)
{
    bool const helpfulHit = (procFlag & PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL) != 0;
    bool const healTick = (procFlag & PROC_FLAG_ON_DO_PERIODIC) && (procEx & PROC_EX_PERIODIC_POSITIVE);
    if ((!helpfulHit && !healTick) || !amount || !(procEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT)))
    {
        return false;
    }
    return (procEx & PROC_EX_CRITICAL_HIT) || spellId == PRAYER_OF_HEALING;
}

int32 Priest::AegisShield(uint32 heal, int32 rankPercent, int32 masteryPercent)
{
    if (!heal || rankPercent <= 0)
    {
        return 0;
    }
    int64 const mastery = 100 + (masteryPercent > 0 ? masteryPercent : 0);
    int64 const shield = int64(heal) * rankPercent * mastery / 10000;
    return shield < INT32_MAX ? int32(shield) : INT32_MAX;
}

int32 Priest::AegisCap(uint32 maxHealth)
{
    return int32(uint64(maxHealth) * AEGIS_CAP_PERCENT / 100);
}

int32 Priest::AegisAmount(int32 fresh, int32 left, int32 cap)
{
    if (fresh <= 0)
    {
        return fresh;
    }
    int64 const kept = left > 0 ? left : 0;
    int64 const sum = kept + fresh < cap ? kept + fresh : cap;
    int64 const larger = kept > fresh ? kept : fresh;
    return int32(sum > larger ? sum : larger);
}
