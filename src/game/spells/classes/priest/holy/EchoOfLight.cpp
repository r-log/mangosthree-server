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

#include "spells/classes/priest/holy/EchoOfLight.h"
#include "spells/data/SpellMgr.h"
#include "combat/SpellBonus.h"
#include "entities/Unit.h"
#include "DBCStores.h"

#include <cstdint>

namespace
{
    /// The Holy mastery's aura, whose effect 0 holds the mastery coefficient (1.25) and whose proc flags (0x4000) let
    /// the priest's helpful hits through.
    uint32 const ECHO_OF_LIGHT[] = { 77485 };

    /// Echo of Light's heal over time: its effect 0 a periodic heal every 1000 ms, over 6000 ms.
    uint32 const ECHO_OF_LIGHT_HEAL = 77489;

    /// The heals Echo of Light leaves out: Guardian Spirit's heal, Spirit of Redemption, its own heal and Lightwell's
    /// Renew.
    uint32 const ECHO_LEFT_OUT[] = { 48153, 27827, ECHO_OF_LIGHT_HEAL, 7001 };

    /// Prayer of Mending's heal, whose critical heals echo without their critical bonus.
    uint32 const PRAYER_OF_MENDING_HEAL = 33110;

    /// The heal the critical heal bonus is read back from: what Unit::SpellCriticalHealingBonus makes of it gives the
    /// factor the engine applied.
    uint32 const CRIT_PROBE = 1000000;

    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// 77485: Echo of Light: its proc flags let every helpful hit of the priest through; the script answers true for a
    /// direct heal of a living priest (Priest::EchoFromHeal, Priest::HealsAtHit), so the engine's class-mask check,
    /// which the aura's own mask would fail, does not run, and false for every other event.
    SpellHandlerOutcome<bool> EchoOfLightCheckProc(CheckProcContext& ctx)
    {
        bool const echoes = ctx.owner.IsAlive() && ctx.procSpell &&
                            Priest::EchoFromHeal(ctx.procFlag, ctx.procEx, ctx.procSpell->ID,
                                                 Priest::HealsAtHit(*ctx.procSpell), ctx.damage);
        return SpellHandlerOutcome<bool>::Return(echoes);
    }

    /// 77485: Echo of Light: the priest casts its heal over time (77489) at the unit he healed, triggered by the
    /// mastery, its tick the Echo of the heal spread over the ticks of 77489's duration (Priest::EchoTick): his heal
    /// mastery's percent (Priest::HealMastery) of the whole heal the event carries, or for a critical heal of Prayer
    /// of Mending of the heal before the critical bonus his Unit::SpellCriticalHealingBonus applied
    /// (Priest::HealBeforeCrit). When the unit holds his Echo already, what its ticks still due heal
    /// (Priest::EchoTicksLeft of its time left, through AuraHandle, times its tick) is added and the sum spread over
    /// one tick more: the new holder keeps the old one's time to its next tick and lasts that long past 77489's
    /// duration (Unit::AddSpellAuraHolder). The cast's hit starts no procs. CANT_TRIGGER for effect 1, a second dummy
    /// (125); FAILED, with nothing cast, for no unit healed, no spell, a store without 77489 or a tick of 0.
    ProcOutcome EchoOfLightProc(OnProcContext& ctx)
    {
        if (ctx.effIndex != EFFECT_INDEX_0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
        }
        SpellEntry const* echo = sSpellStore.LookupEntry(ECHO_OF_LIGHT_HEAL);
        SpellEffectEntry const* tick = echo ? echo->GetSpellEffect(EFFECT_INDEX_0) : NULL;
        if (!ctx.victim || !ctx.procSpell || !tick)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const period = int32(tick->EffectAuraPeriod);

        uint32 heal = ctx.damage;
        if (Priest::EchoesHealBeforeCrit(ctx.procSpell->ID, ctx.procEx))
        {
            heal = Priest::HealBeforeCrit(heal, CRIT_PROBE,
                                          ctx.caster.SpellCriticalHealingBonus(ctx.procSpell, CRIT_PROBE, ctx.victim));
        }
        double total = Priest::EchoAmount(heal, Priest::HealMastery(ctx.caster));
        uint32 ticks = Priest::EchoTicksLeft(GetSpellDuration(echo), period);
        if (SpellAuraHolder* own = ctx.victim->GetSpellAuraHolder(ECHO_OF_LIGHT_HEAL, ctx.caster.GetObjectGuid()))
        {
            AuraHandle const left(*own);
            uint32 const due = Priest::EchoTicksLeft(left.Duration(), period);
            total += double(due) * double(left.Amount(EFFECT_INDEX_0));
            ticks += due ? 1 : 0;
        }

        int32 const amount = Priest::EchoTick(total, ticks);
        if (amount <= 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const basePoints[MAX_EFFECT_INDEX] = { amount, 0, 0 };
        return ProcOutcome::Return(ctx.CastTriggered(*ctx.victim, ECHO_OF_LIGHT_HEAL, basePoints)
                                   ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED);
    }
}

uint32 Priest::RegisterEchoOfLight(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<CheckProcHook>(registry, ECHO_OF_LIGHT, &EchoOfLightCheckProc);
    rows += RegisterRanks<OnProcHook>(registry, ECHO_OF_LIGHT, &EchoOfLightProc);
    return rows;
}

bool Priest::HealsAtHit(SpellEntry const& spell)
{
    return IsSpellHaveEffect(&spell, SPELL_EFFECT_HEAL) || IsSpellHaveEffect(&spell, SPELL_EFFECT_HEAL_PCT) ||
           IsSpellHaveEffect(&spell, SPELL_EFFECT_HEAL_MAX_HEALTH);
}

bool Priest::EchoFromHeal(uint32 procFlag, uint32 procEx, uint32 spellId, bool healsAtHit, uint32 heal)
{
    bool const helpfulHit = (procFlag & PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL) != 0;
    bool const tick = (procFlag & PROC_FLAG_ON_DO_PERIODIC) || (procEx & PROC_EX_PERIODIC_POSITIVE);
    bool const hit = (procEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT)) != 0;
    if (!helpfulHit || tick || !hit || !healsAtHit || !heal)
    {
        return false;
    }
    for (uint32 leftOut : ECHO_LEFT_OUT)
    {
        if (spellId == leftOut)
        {
            return false;
        }
    }
    return true;
}

float Priest::HealMastery(Unit const& caster)
{
    SpellEntry const* mastery = sSpellStore.LookupEntry(ECHO_OF_LIGHT[0]);
    if (!mastery || caster.GetTypeId() != TYPEID_PLAYER)
    {
        return 0.0f;
    }
    float const coefficient = MasteryCoefficient(mastery, mastery->GetSpellEffect(EFFECT_INDEX_0));
    return Combat::MasteryAmount(coefficient, caster.GetFloatValue(PLAYER_MASTERY));
}

bool Priest::EchoesHealBeforeCrit(uint32 spellId, uint32 procEx)
{
    return spellId == PRAYER_OF_MENDING_HEAL && (procEx & PROC_EX_CRITICAL_HIT) != 0;
}

uint32 Priest::HealBeforeCrit(uint32 heal, uint32 probe, uint32 probeCrit)
{
    if (!probeCrit)
    {
        return heal;
    }
    uint64 const before = (uint64(heal) * probe + probeCrit / 2) / probeCrit;
    return before < UINT32_MAX ? uint32(before) : UINT32_MAX;
}

double Priest::EchoAmount(uint32 heal, float masteryPercent)
{
    if (masteryPercent <= 0.0f)
    {
        return 0.0;
    }
    return double(heal) * double(masteryPercent) / 100.0;
}

uint32 Priest::EchoTicksLeft(int32 remainingMs, int32 periodMs)
{
    if (remainingMs <= 0 || periodMs <= 0)
    {
        return 0;
    }
    return uint32((int64(remainingMs) + periodMs - 1) / periodMs);
}

int32 Priest::EchoTick(double total, uint32 ticks)
{
    if (!ticks || !(total > 0.0))
    {
        return 0;
    }
    double const tick = total / double(ticks) + 0.5;
    return tick < double(INT32_MAX) ? int32(tick) : INT32_MAX;
}
