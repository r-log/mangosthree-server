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

#include "spells/classes/priest/shadow/ShadowOrbs.h"
#include "spells/data/SpellMgr.h"
#include "combat/SpellBonus.h"
#include "entities/Unit.h"
#include "DBCStores.h"

namespace
{
    /// The Shadow mastery's aura, whose proc flags take the priest's harmful hits and his periodic damage and whose
    /// effect 0 holds the mastery coefficient (1.45).
    uint32 const SHADOW_ORB_POWER = 77486;

    /// The orbs: Shadow Orb, whose first effect is a dummy aura, three stacks for 60 s.
    uint32 const SHADOW_ORB = 77487;

    /// The ids each row is registered under.
    uint32 const SHADOW_ORB_POWER_ROW[] = { SHADOW_ORB_POWER };
    uint32 const SHADOW_ORB_ROW[] = { SHADOW_ORB };

    /// Empowered Shadow: the periodic shadow damage of the priest raised for 15 s; its effect 0 value, 10, is what an
    /// orb adds before the mastery.
    uint32 const EMPOWERED_SHADOW = 95799;

    /// The periodic damage that gives an orb: Shadow Word: Pain and Mind Flay.
    uint32 const SHADOW_WORD_PAIN = 589;
    uint32 const MIND_FLAY = 15407;

    /// The direct hits that spend the orbs: Mind Blast and Mind Spike.
    uint32 const MIND_BLAST = 8092;
    uint32 const MIND_SPIKE = 73510;

    /// 77486: Shadow Orb Power: its proc flags let every harmful hit and tick of the priest through, and its effects
    /// name no class mask, so the engine's check alone would let every harmful hit and tick through as well. The script answers true for the damage tick of Shadow Word:
    /// Pain or Mind Flay (Priest::OrbFromTick) and false for every other event, so the engine's class mask and hit
    /// kind do not run for the aura, which has no spell_proc_event row, and its chance is the client's 10 plus what
    /// Harnessed Shadows adds to it.
    SpellHandlerOutcome<bool> ShadowOrbPowerCheckProc(CheckProcContext& ctx)
    {
        uint32 const spellId = ctx.procSpell ? ctx.procSpell->ID : 0;
        return SpellHandlerOutcome<bool>::Return(Priest::OrbFromTick(spellId, ctx.procFlag, ctx.damage));
    }

    /// 77487: Shadow Orb: the orbs' first effect is a dummy aura, so this row runs for every spell its holder deals. A
    /// direct hit of Mind Blast or Mind Spike deals the orbs' percent more for each orb he holds
    /// (Priest::ShadowOrbMultiplier); every other spell, and a tick, is left as it is.
    SpellHandlerOutcome<void> ShadowOrbDamageDone(DamageDoneContext& ctx)
    {
        uint32 const orbs = Priest::ShadowOrbs(ctx.caster);
        if (orbs)
        {
            ctx.doneTotalMod *= Priest::ShadowOrbMultiplier(ctx.spell.ID, ctx.damageType, orbs,
                                                            Priest::ShadowOrbPercent(ctx.caster));
        }
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterShadowOrbs(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<CheckProcHook>(registry, SHADOW_ORB_POWER_ROW, &ShadowOrbPowerCheckProc);
    rows += RegisterRanks<DamageDoneHook>(registry, SHADOW_ORB_ROW, &ShadowOrbDamageDone);
    return rows;
}

bool Priest::OrbFromTick(uint32 spellId, uint32 procFlag, uint32 damage)
{
    return (procFlag & PROC_FLAG_ON_DO_PERIODIC) && damage > 0 && (spellId == SHADOW_WORD_PAIN || spellId == MIND_FLAY);
}

bool Priest::SpendsShadowOrbs(uint32 spellId)
{
    return spellId == MIND_BLAST || spellId == MIND_SPIKE;
}

float Priest::ShadowOrbPercent(Unit const& caster)
{
    SpellEntry const* empowered = sSpellStore.LookupEntry(EMPOWERED_SHADOW);
    SpellEntry const* power = sSpellStore.LookupEntry(SHADOW_ORB_POWER);
    if (!empowered || !power || caster.GetTypeId() != TYPEID_PLAYER)
    {
        return 0.0f;
    }
    float const coefficient = MasteryCoefficient(power, power->GetSpellEffect(EFFECT_INDEX_0));
    return float(empowered->CalculateSimpleValue(EFFECT_INDEX_0)) +
           Combat::MasteryAmount(coefficient, caster.GetFloatValue(PLAYER_MASTERY));
}

uint32 Priest::ShadowOrbs(Unit const& caster)
{
    if (SpellAuraHolder* holder = caster.GetSpellAuraHolder(SHADOW_ORB, caster.GetObjectGuid()))
    {
        return AuraHandle(*holder).Stacks();
    }
    return 0;
}

float Priest::ShadowOrbMultiplier(uint32 spellId, DamageEffectType damageType, uint32 stacks, float percent)
{
    if (damageType != SPELL_DIRECT_DAMAGE || !SpendsShadowOrbs(spellId))
    {
        return 1.0f;
    }
    return 1.0f + float(stacks) * percent / 100.0f;
}

int32 Priest::EmpoweredShadowPoints(float percent)
{
    if (percent <= 0.0f)
    {
        return 0;
    }
    return int32(percent + 0.5f);
}

void Priest::ConsumeShadowOrbs(AfterHitContext& ctx)
{
    if (!ShadowOrbs(ctx.caster))
    {
        return;
    }
    int32 const points = EmpoweredShadowPoints(ShadowOrbPercent(ctx.caster));
    ctx.RemoveAura(ctx.caster, SHADOW_ORB);
    if (points > 0)
    {
        int32 const basePoints[MAX_EFFECT_INDEX] = { points, points, 0 };
        ctx.CastTriggered(ctx.caster, EMPOWERED_SHADOW, basePoints);
    }
}
