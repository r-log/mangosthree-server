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
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"
#include "spells/SpellModMgr.h"
#include "spells/SpellScriptQueries.h"
#include "combat/SpellBonus.h"
#include "combat/ThreatManager.h"
#include "entities/Unit.h"
#include "DBCStores.h"
#include "Log.h"
#include "Utilities/Util.h"

#include <algorithm>
#include <iterator>
#include <vector>

namespace
{
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// Ignite's damage over time, the spell the talent's ranks cast.
    uint32 const IGNITE_DOT = 12654;

    /// The Fire hits that do not ignite: Living Bomb's explosion, Flame Orb's hits and Fire Power's explosion.
    uint32 const NOT_IGNITING[] = { 44461, 82739, 83619 };

    /// The ticks Ignite (12654) deals: its duration over its periodic effect's period; 0 when the spell store has no
    /// 12654 or its effect no period.
    uint32 IgniteTicks()
    {
        SpellEntry const* ignite = sSpellStore.LookupEntry(IGNITE_DOT);
        SpellEffectEntry const* effect = ignite ? ignite->GetSpellEffect(EFFECT_INDEX_0) : NULL;
        int32 const duration = GetSpellDuration(ignite);
        if (!effect || !effect->EffectAuraPeriod || duration <= 0)
        {
            return 0;
        }
        return uint32(duration) / effect->EffectAuraPeriod;
    }

    /// 11119, 11120, 12846: Ignite: a critical strike of one of the mage's direct Fire spells puts the aura's percent of
    /// its damage on the victim as Ignite (12654), two ticks over its duration. The explosion of Living Bomb (44461),
    /// Flame Orb's hits (82739) and Fire Power's explosion (83619) do not ignite. A share below one point per tick
    /// banks nothing, so a running Ignite is left as it is.
    ProcOutcome IgniteProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        if (!(ctx.procEx & PROC_EX_CRITICAL_HIT))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        if (ctx.procSpell &&
            std::find(std::begin(NOT_IGNITING), std::end(NOT_IGNITING), ctx.procSpell->ID) != std::end(NOT_IGNITING))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        if (!ctx.victim || !ctx.victim->IsAlive() || ctx.triggerAmount <= 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }

        int32 const tick = Combat::DotShareTick(float(ctx.damage), uint32(ctx.triggerAmount), IgniteTicks());
        if (tick <= 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const basePoints[MAX_EFFECT_INDEX] = { tick, 0, 0 };
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, *ctx.victim, IGNITE_DOT, basePoints));
    }

    /// 29074, 29075, 29076: Master of Elements: after a spell the proc accepts, gives back the aura's percent of the
    /// spell's base mana cost through 29077. An orb's hit, which costs nothing, counts its share of the orb spell's cost
    /// (Mage::OrbCostShare).
    ProcOutcome MasterOfElementsProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        if (!ctx.procSpell)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }

        int32 const orbShare = Mage::OrbCostShare(ctx.procSpell->ID, ctx.caster);
        int32 const cost = orbShare > 0 ? orbShare
                         : int32(ctx.procSpell->GetManaCost() + ctx.procSpell->GetManaCostPercentage() * ctx.caster.GetCreateMana() / 100);
        int32 const basePoints[MAX_EFFECT_INDEX] = { cost * ctx.triggerAmount / 100, 0, 0 };
        if (basePoints[EFFECT_INDEX_0] <= 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, ctx.caster, 29077, basePoints));
    }

    uint32 const HOT_STREAK_BUFF = 48108;
    uint32 const PYROBLAST_INSTANT = 92315;
    uint32 const IMPROVED_HOT_STREAK_RANK_1 = 44446;
    uint32 const IMPROVED_HOT_STREAK_RANK_2 = 44448;

    /// 44445: Hot Streak: each critical strike of Fireball, Frostfire Bolt, Scorch, Pyroblast or Fire Blast (the proc
    /// row's spells; the script leaves Pyroblast! 92315 out) rolls HotStreakChance of the caster's sheet crit for the
    /// school of the spell; when that fails and the caster knows Improved Hot Streak (44448: 100, 44446: 50), two such
    /// crits in a row roll its percent. A non-crit of those spells starts the count again. Success casts Hot Streak
    /// (48108) on the caster. The count lives in this holder's script word of effect 0.
    ProcOutcome HotStreakProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        if (ctx.effIndex != EFFECT_INDEX_0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_OK);
        }
        if (!ctx.procSpell)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        if (ctx.procSpell->ID == PYROBLAST_INSTANT)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_OK);
        }

        bool const crit = (ctx.procEx & PROC_EX_CRITICAL_HIT) != 0;
        float const sheetCrit = ctx.caster.GetTypeId() == TYPEID_PLAYER
            ? ctx.caster.GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + GetFirstSchoolInMask(GetSpellSchoolMask(ctx.procSpell)))
            : 0.0f;
        int32 const improved = ctx.caster.HasAura(IMPROVED_HOT_STREAK_RANK_2) ? 100
                             : ctx.caster.HasAura(IMPROVED_HOT_STREAK_RANK_1) ? 50 : 0;
        bool const baseRoll = crit && roll_chance_f(Mage::HotStreakChance(sheetCrit));
        bool const improvedRoll = crit && !baseRoll && improved > 0 && roll_chance_i(improved);
        if (!Mage::HotStreakStep(ctx.aura.ScriptWord(EFFECT_INDEX_0), crit, baseRoll, improvedRoll))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_OK);
        }
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, ctx.caster, HOT_STREAK_BUFF));
    }

    uint32 const ITEM_MAGE_T10_2P_BONUS = 70752;
    uint32 const PUSHING_THE_LIMIT = 70753;

    /// 48108: Hot Streak (the buff): when it is used up, removed in the default mode as casting Pyroblast! removes it,
    /// and its caster holds Item - Mage T10 2P Bonus (70752), the mage casts Pushing the Limit (70753) on himself for
    /// that caster. Its running out, a dispel or a cancel gives nothing. The script ends the removal's boosts.
    SpellHandlerOutcome<void> HotStreakBuffRemove(AuraRemoveContext& ctx)
    {
        if (Mage::PushingTheLimitOnRemove(ctx.mode, ctx.caster && ctx.caster->HasAura(ITEM_MAGE_T10_2P_BONUS)))
        {
            ctx.CastTriggered(ctx.target, PUSHING_THE_LIMIT);
        }
        return SpellHandlerOutcome<void>::Return();
    }

    /// 31641, 31642: Blazing Speed: a proc-trigger aura whose own trigger spell (18350) the client's data lacks. Its
    /// proc casts Blazing Speed (31643) instead, through Mage::CastProcTriggerSpell with no target of its own; the
    /// aura's amount as base points when the aura carries a value. No row of the legacy site of trigger spells
    /// (ProcTriggerSpellAuraProcTriggeredSite) names 31643.
    ProcOutcome BlazingSpeedProc(OnProcContext& ctx)
    {
        bool const withValue = Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE);
        if (!withValue && !Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        if (!IsBlazingSpeedRank(ctx.spell.ID))
        {
            sLog.outError("Blazing Speed: spell %u is no rank of Blazing Speed", ctx.spell.ID);
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }

        int32 const basePoints[MAX_EFFECT_INDEX] = { withValue ? ctx.triggerAmount : 0, 0, 0 };
        return ProcOutcome::Return(Mage::CastProcTriggerSpell(ctx, 31643, NULL, basePoints));
    }

    /// 44457: Living Bomb: when its dummy effect is un-applied because the aura ran out (Mage::LivingBombExplodes),
    /// the target casts the effect's amount, the explosion (44461), at itself, triggered by this aura effect; any
    /// other removal casts nothing. Every un-apply ends the dummy aura handler; an apply goes on to its other steps.
    SpellHandlerOutcome<void> LivingBombDummyAura(DummyAuraContext& ctx)
    {
        if (ctx.apply)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        if (Mage::LivingBombExplodes(ctx.mode))
        {
            ctx.CastTriggeredByAura(ctx.target, ctx.target, uint32(ctx.amount));
        }
        return SpellHandlerOutcome<void>::Return();
    }

    /// Combustion's DoT, 83853.
    uint32 const COMBUSTION_DOT = 83853;

    /// The caster's DoTs Combustion combines besides Ignite: Pyroblast (11366) and Pyroblast! (92315), Living Bomb
    /// (44457) and Frostfire Bolt (44614), whose periodic effect Glyph of Frostfire gives. Flamestrike's DoT is not
    /// combined.
    uint32 const COMBUSTION_SHARES[] = { 11366, 92315, 44457, 44614 };

    /// 11129: Combustion: its script effect puts Combustion (83853) on the target with a tick equal to the per-second
    /// damage of the caster's Pyroblast (11366, 92315), Living Bomb (44457), Frostfire Bolt (44614) and Ignite (12654)
    /// on the target at this moment; Ignite's part is taken before the caster's Flashburn, which the engine applies to
    /// the new DoT. Flamestrike is not combined. With nothing to combine the DoT is still applied, with a tick of 0. The
    /// script ends the effect, so the mage family switch, which has nothing for 11129, does not run.
    SpellHandlerOutcome<void> CombustionEffect(DummyEffectContext& ctx)
    {
        if (ctx.effIndex != EFFECT_INDEX_0 || !ctx.target)
        {
            return SpellHandlerOutcome<void>::Return();
        }

        ObjectGuid const caster = ctx.caster.GetObjectGuid();
        std::vector<PeriodicDamageView> found;
        std::vector<Mage::DotShare> shares;
        FindPeriodicDamage(*ctx.target, caster, COMBUSTION_SHARES, sizeof(COMBUSTION_SHARES) / sizeof(COMBUSTION_SHARES[0]),
                           found);
        for (PeriodicDamageView const& view : found)
        {
            Mage::DotShare const share = { view.amount, view.periodMs };
            shares.push_back(share);
        }

        uint32 const igniteDot[] = { IGNITE_DOT };
        Mage::DotShare ignite = { 0, 0 };
        bool const hasIgnite = FindPeriodicDamage(*ctx.target, caster, igniteDot, 1, found) > 0;
        if (hasIgnite)
        {
            ignite.tick = found[0].amount;
            ignite.periodMs = found[0].periodMs;
        }

        // the caster's mastery on Ignite: the step SpellDamageBonusDone takes for a spell without damage modifiers
        float igniteMultiplier = 1.0f;
        if (SpellModMgr* mods = ctx.caster.GetSpellMods())
        {
            mods->ApplySpellMod(IGNITE_DOT, SPELLMOD_DOT, igniteMultiplier, true);
        }

        int32 const tick = Mage::CombustionTick(shares.empty() ? NULL : &shares[0], shares.size(), hasIgnite ? &ignite : NULL,
                                                igniteMultiplier);
        int32 const basePoints[MAX_EFFECT_INDEX] = { tick, 0, 0 };
        ctx.CastTriggered(*ctx.target, COMBUSTION_DOT, basePoints);
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 const PYROMANIAC_RANK_1 = 34293;
    uint32 const PYROMANIAC_RANK_2 = 34295;
    uint32 const PYROMANIAC_HASTE = 83582;

    /// The DoTs Pyromaniac counts: Flamestrike's ground effect (2120), Pyroblast (11366), Ignite (12654), Living Bomb
    /// (44457), Frostfire Bolt (44614), Combustion (83853) and Pyroblast! (92315).
    uint32 const PYROMANIAC_DOTS[] = { 2120, 11366, 12654, 44457, 44614, 83853, 92315 };

    /// When `caster`, the caster of a holder of a DoT Pyromaniac counts that was just added to `target` or taken off
    /// it, is a player in the world on `target`'s map who knows Pyromaniac (34295, else 34293), his burning enemies are
    /// counted again: Pyromaniac's haste (83582) is cast on him with the rank's value when three or more burn and he
    /// lacks it, and taken off him when fewer burn and he has it. A caster anywhere else is left as he is.
    void PyromaniacRecount(Unit* caster, Unit& target)
    {
        if (!caster || caster->GetTypeId() != TYPEID_PLAYER || !caster->IsInWorld() || caster->FindMap() != target.FindMap())
        {
            return;
        }
        uint32 const talent = caster->HasAura(PYROMANIAC_RANK_2) ? PYROMANIAC_RANK_2
                            : caster->HasAura(PYROMANIAC_RANK_1) ? PYROMANIAC_RANK_1 : 0;
        if (!talent)
        {
            return;
        }

        bool const active = Mage::PyromaniacActive(Mage::PyromaniacTargets(*caster));
        bool const hasHaste = caster->HasAura(PYROMANIAC_HASTE);
        if (active && !hasHaste)
        {
            int32 const basePoints[MAX_EFFECT_INDEX] = { Mage::PyromaniacHaste(talent), 0, 0 };
            caster->CastCustomSpell(caster, PYROMANIAC_HASTE, &basePoints[EFFECT_INDEX_0], NULL, NULL, true);
        }
        else if (!active && hasHaste)
        {
            caster->RemoveAurasDueToSpell(PYROMANIAC_HASTE);
        }
    }

    /// 2120, 11366, 12654, 44457, 83853, 92315: the DoTs Pyromaniac counts, when their holder is added: the caster's
    /// burning enemies are counted again with the new holder on its target (PyromaniacRecount). The holder's boosts
    /// still run.
    SpellHandlerOutcome<void> PyromaniacDotApply(AuraApplyContext& ctx)
    {
        PyromaniacRecount(ctx.caster, ctx.target);
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 44614: Frostfire Bolt: when its holder is added, the caster's burning enemies are counted again for Pyromaniac,
    /// as PyromaniacDotApply counts them. The holder's boosts still run.
    SpellHandlerOutcome<void> FrostfireBoltAuraApply(AuraApplyContext& ctx)
    {
        return PyromaniacDotApply(ctx);
    }

    /// The seven DoTs Pyromaniac counts, when their holder is removed: the caster's burning enemies are counted again
    /// with the holder gone (PyromaniacRecount). A removal by re-application (AURA_REMOVE_BY_STACK) changes nothing:
    /// the apply that follows counts. The holder's boosts still run.
    SpellHandlerOutcome<void> PyromaniacDotRemove(AuraRemoveContext& ctx)
    {
        if (ctx.mode != AURA_REMOVE_BY_STACK)
        {
            PyromaniacRecount(ctx.caster, ctx.target);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 34293, 34295: Pyromaniac: when the talent's holder is added (a login, a respec, a new rank), Pyromaniac's haste
    /// (83582) is taken off the mage; the next DoT it counts puts the haste back when it is due. The holder's boosts
    /// still run.
    SpellHandlerOutcome<void> PyromaniacTalentApply(AuraApplyContext& ctx)
    {
        ctx.RemoveAura(ctx.target, PYROMANIAC_HASTE);
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 34293, 34295: Pyromaniac: when the talent's holder is removed, Pyromaniac's haste (83582) is taken off the mage.
    /// The holder's boosts still run.
    SpellHandlerOutcome<void> PyromaniacTalentRemove(AuraRemoveContext& ctx)
    {
        ctx.RemoveAura(ctx.target, PYROMANIAC_HASTE);
        return SpellHandlerOutcome<void>::Continue();
    }

    /// Fire Blast, whose cooldown Impact clears.
    uint32 const FIRE_BLAST = 2136;

    /// 11103, 12357: Impact: when its proc-trigger effect procs, Fire Blast's (2136) cooldown is cleared and the effect's
    /// trigger spell, Impact (64343), is cast on the caster.
    ProcOutcome ImpactProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        ctx.caster.RemoveSpellCooldown(FIRE_BLAST, true);
        return ProcOutcome::Return(Mage::CastProcTriggerSpell(ctx, Mage::EffectTriggerSpell(ctx), &ctx.caster));
    }

    /// The caster's DoTs Impact copies, in the order it copies them: Pyroblast (11366), Pyroblast! (92315), Living Bomb
    /// (44457), Ignite (12654) and Combustion (83853).
    uint32 const IMPACT_SPREAD_DOTS[] = { 11366, 92315, 44457, 12654, 83853 };

    /// Living Bomb, which Impact copies to IMPACT_BOMB_TARGETS enemies at most.
    uint32 const IMPACT_LIVING_BOMB = 44457;
    int32 const IMPACT_BOMB_TARGETS = 2;

    /// 12355: Impact: for each enemy its script effect reaches other than the stunned target, copies the caster's
    /// Pyroblast, Living Bomb, Ignite and Combustion from the stunned target to it, Living Bomb to two enemies at most;
    /// an enemy under a confuse or fear is left alone, and so is one already holding the caster's aura of that spell.
    /// The stun is the spell's own effect 0. The script ends the effect.
    SpellHandlerOutcome<void> ImpactSpreadEffect(DummyEffectContext& ctx)
    {
        Unit* const stunned = ctx.castTarget;
        if (ctx.effIndex != EFFECT_INDEX_1 || !ctx.target || !stunned || ctx.target == stunned ||
            ctx.target->HasAuraType(SPELL_AURA_MOD_CONFUSE) || ctx.target->HasAuraType(SPELL_AURA_MOD_FEAR))
        {
            return SpellHandlerOutcome<void>::Return();
        }

        ObjectGuid const caster = ctx.caster.GetObjectGuid();
        for (uint32 spellId : IMPACT_SPREAD_DOTS)
        {
            // the cast's word counts the Living Bomb copies tried over all of its targets
            if (Mage::ImpactSpreads(spellId, ctx.word))
            {
                CopyAuraHolder(*stunned, *ctx.target, spellId, caster);
            }
        }
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 const IGNITE[] = { 11119, 11120, 12846 };
    uint32 const MASTER_OF_ELEMENTS[] = { 29074, 29075, 29076 };
    uint32 const HOT_STREAK[] = { 44445 };
    uint32 const HOT_STREAK_BUFFS[] = { HOT_STREAK_BUFF };
    uint32 const BLAZING_SPEED[] = { 31641, 31642 };
    uint32 const LIVING_BOMB[] = { 44457 };
    uint32 const COMBUSTION[] = { 11129 };
    uint32 const IMPACT[] = { 11103, 12357 };
    uint32 const IMPACT_SPREAD[] = { 12355 };
    uint32 const PYROMANIAC_DOTS_APPLIED[] = { 2120, 11366, 12654, 44457, 83853, 92315 };
    uint32 const FROSTFIRE_BOLT[] = { 44614 };
    uint32 const PYROMANIAC[] = { PYROMANIAC_RANK_1, PYROMANIAC_RANK_2 };
}

bool Mage::CountCritInARow(int32& crits, bool crit)
{
    if (!crit)
    {
        crits = 0;
        return false;
    }
    if (++crits < 2)
    {
        return false;
    }
    crits = 0;
    return true;
}

float Mage::HotStreakChance(float sheetCritPercent)
{
    float const crit = sheetCritPercent > 0.0f ? sheetCritPercent : 0.0f;
    float const chance = 95.0f - 2.73f * crit;
    return chance > 0.0f ? chance : 0.0f;
}

bool Mage::PushingTheLimitOnRemove(AuraRemoveMode mode, bool casterHasT10TwoPiece)
{
    return mode == AURA_REMOVE_BY_DEFAULT && casterHasT10TwoPiece;
}

bool Mage::LivingBombExplodes(AuraRemoveMode mode)
{
    return mode == AURA_REMOVE_BY_EXPIRE;
}

bool Mage::HotStreakStep(int32& crits, bool crit, bool baseRollPassed, bool improvedRollPassed)
{
    if (crit && baseRollPassed)
    {
        crits = 0;
        return true;
    }
    return CountCritInARow(crits, crit) && improvedRollPassed;
}

int32 Mage::CombustionTick(DotShare const* shares, size_t count, DotShare const* ignite, float igniteMultiplier)
{
    double perSecond = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        if (shares[i].periodMs)
        {
            perSecond += double(shares[i].tick) * 1000.0 / double(shares[i].periodMs);
        }
    }
    if (ignite && ignite->periodMs)
    {
        double const tick = igniteMultiplier > 0.0f ? double(ignite->tick) / double(igniteMultiplier) : double(ignite->tick);
        perSecond += tick * 1000.0 / double(ignite->periodMs);
    }
    return perSecond > 0.0 ? int32(perSecond) : 0;
}

bool Mage::PyromaniacCounts(uint32 spellId)
{
    return std::find(std::begin(PYROMANIAC_DOTS), std::end(PYROMANIAC_DOTS), spellId) != std::end(PYROMANIAC_DOTS);
}

bool Mage::PyromaniacActive(uint32 targets)
{
    return targets >= 3;
}

uint32 Mage::PyromaniacTargets(Unit& mage)
{
    uint32 burning = 0;
    for (HostileReference* ref = mage.GetHostileRefManager().getFirst(); ref; ref = ref->next())
    {
        Unit* enemy = ref->getSource()->getOwner();
        if (!enemy || !enemy->IsAlive() || !enemy->IsInWorld() || enemy->FindMap() != mage.FindMap())
        {
            continue;
        }
        for (uint32 spellId : PYROMANIAC_DOTS)
        {
            if (enemy->GetSpellAuraHolder(spellId, mage.GetObjectGuid()))
            {
                ++burning;
                break;
            }
        }
    }
    return burning;
}

int32 Mage::PyromaniacHaste(uint32 talentSpell)
{
    if (std::find(std::begin(PYROMANIAC), std::end(PYROMANIAC), talentSpell) == std::end(PYROMANIAC))
    {
        return 0;
    }
    SpellEntry const* talent = sSpellStore.LookupEntry(talentSpell);
    return talent ? talent->CalculateSimpleValue(EFFECT_INDEX_0) : 0;
}

bool Mage::ImpactSpreads(uint32 spellId, int32& bombsSpread)
{
    uint32 const* const dotsEnd = std::end(IMPACT_SPREAD_DOTS);
    if (std::find(std::begin(IMPACT_SPREAD_DOTS), dotsEnd, spellId) == dotsEnd)
    {
        return false;
    }
    if (spellId != IMPACT_LIVING_BOMB)
    {
        return true;
    }
    if (bombsSpread >= IMPACT_BOMB_TARGETS)
    {
        return false;
    }
    ++bombsSpread;
    return true;
}

uint32 Mage::RegisterFire(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<OnProcHook>(registry, IGNITE, &IgniteProc);
    rows += RegisterRanks<OnProcHook>(registry, MASTER_OF_ELEMENTS, &MasterOfElementsProc);
    rows += RegisterRanks<OnProcHook>(registry, HOT_STREAK, &HotStreakProc);
    rows += RegisterRanks<AuraRemoveHook>(registry, HOT_STREAK_BUFFS, &HotStreakBuffRemove);
    rows += RegisterRanks<OnProcHook>(registry, BLAZING_SPEED, &BlazingSpeedProc);
    rows += RegisterRanks<DummyAuraHook>(registry, LIVING_BOMB, &LivingBombDummyAura);
    rows += RegisterRanks<AuraApplyHook>(registry, PYROMANIAC_DOTS_APPLIED, &PyromaniacDotApply);
    rows += RegisterRanks<AuraApplyHook>(registry, FROSTFIRE_BOLT, &FrostfireBoltAuraApply);
    rows += RegisterRanks<AuraRemoveHook>(registry, PYROMANIAC_DOTS, &PyromaniacDotRemove);
    rows += RegisterRanks<AuraApplyHook>(registry, PYROMANIAC, &PyromaniacTalentApply);
    rows += RegisterRanks<AuraRemoveHook>(registry, PYROMANIAC, &PyromaniacTalentRemove);
    rows += RegisterRanks<DummyEffectHook>(registry, COMBUSTION, &CombustionEffect);
    rows += RegisterRanks<OnProcHook>(registry, IMPACT, &ImpactProc);
    rows += RegisterRanks<DummyEffectHook>(registry, IMPACT_SPREAD, &ImpactSpreadEffect);
    return rows;
}
