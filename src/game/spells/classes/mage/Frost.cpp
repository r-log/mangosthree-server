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
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    uint32 const SHATTER_RANK_ONE = 11170;
    uint32 const SHATTER_RANK_TWO = 12982;
    uint32 const FROSTBURN = 76613;
    uint32 const FINGERS_OF_FROST_BUFF = 44544;
    uint32 const FROSTBOLT = 116;
    uint32 const ICE_LANCE_SPELL = 30455;
    uint32 const DEEP_FREEZE = 44572;
    uint32 const DEEP_FREEZE_DAMAGE = 71757;
    uint32 const FROSTFIRE_BOLT = 44614;
    uint32 const FIREBALL = 133;
    uint32 const BRAIN_FREEZE_BUFF = 57761;
    uint32 const FROST_ARMOR_CHILL = 7321;
    uint32 const FROSTFIRE_ORB_RANK_ONE_HIT = 95969;
    uint32 const EARLY_FROST_RANK_ONE = 83049;
    uint32 const EARLY_FROST_RANK_TWO = 83050;
    uint32 const EARLY_FROST_LOCKOUT_RANK_ONE = 83162;
    uint32 const EARLY_FROST_LOCKOUT_RANK_TWO = 83239;
    uint32 const PIERCING_CHILL_RANK_ONE = 83156;
    uint32 const PIERCING_CHILL_RANK_TWO = 83157;
    uint32 const PIERCING_CHILL_CHILL = 83154;
    uint32 const PERMAFROST_PET_HEAL = 91394;
    uint32 const IMPROVED_CONE_OF_COLD_RANK_ONE = 11190;
    uint32 const IMPROVED_CONE_OF_COLD_RANK_TWO = 12489;
    uint32 const IMPROVED_CONE_OF_COLD_FREEZE_ONE = 83301;
    uint32 const IMPROVED_CONE_OF_COLD_FREEZE_TWO = 83302;
    uint32 const GLYPH_OF_ICE_BLOCK = 56372;

    /// Frost Nova's (122) cooldown category.
    uint32 const FROST_NOVA_CATEGORY = 35;

    /// Fingers of Frost's text: "Ice Lance deals 25% more damage" while a charge is up. No field of the data holds it.
    int32 const ICE_LANCE_FINGERS_PERCENT = 25;

    /// Every mage spell with a damage effect that Shatter's class mask covers (the data's mask over the mage's spells
    /// and the spells they cast).
    uint32 const SHATTER_SPELLS[] =
    {
        116, 120, 122, 133, 1449, 2120, 2136, 2948, 7268, 11113, 11366, 30451, 30455, 31661, 34913, 42208, 44425, 44457,
        44461, 44614, 71757, 82739, 84721, 88148, 92315, 95969
    };

    /// Every mage spell with a damage effect that Frostburn's class mask covers.
    uint32 const FROSTBURN_SPELLS[] =
    {
        116, 120, 122, 133, 1449, 2120, 2136, 2948, 7268, 11113, 11366, 30451, 30455, 31661, 34913, 42208, 44425, 44461,
        44614, 71757, 88148, 92315
    };

    /// The value of effect `index` of spell `spellId` in the spell store; 0 when the store lacks the spell.
    int32 EffectValue(uint32 spellId, SpellEffectIndex index)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
        return spell ? spell->CalculateSimpleValue(index) : 0;
    }

    /// The radius in yards of effect `index` of spell `spellId` in the spell store; 0 when the store lacks the spell,
    /// the effect or its radius.
    float EffectRadius(uint32 spellId, SpellEffectIndex index)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
        SpellEffectEntry const* effect = spell ? spell->GetSpellEffect(index) : NULL;
        return effect ? GetSpellRadius(sSpellRadiusStore.LookupEntry(effect->GetRadiusIndex())) : 0.0f;
    }

    /// The cast time in ms that the caster's cast of `spell` in one of his current spell slots started with, when that
    /// cast has not been launched yet: the cast whose hit the crit-chance and damage-done hooks reckon as it goes off.
    /// 0 when it is not above 0, and when no slot holds such a cast, as for a triggered cast, which stands in none.
    uint32 CastTimeGoingOff(Unit const& caster, SpellEntry const& spell)
    {
        Spell const* cast = caster.FindCurrentSpellBySpellId(spell.ID);
        if (!cast || cast->getState() != SPELL_STATE_PREPARING || cast->GetCastTime() <= 0)
        {
            return 0;
        }
        return uint32(cast->GetCastTime());
    }

    /// Every spell of SHATTER_SPELLS: Shatter multiplies the whole crit chance by its factor when the spell counts its
    /// target as frozen, Deep Freeze's damage (71757) and a periodic tick by the target's own state alone. The caster's
    /// flat crit-chance modifiers for the spell are added after this hook, so the chance is raised to the factor times
    /// the chance with them, less them.
    SpellHandlerOutcome<void> FrostCritChance(CritChanceContext& ctx)
    {
        if (Mage::CountsAsFrozen(ctx.caster, ctx.victim, ctx.spell, ctx.damageType,
                                 CastTimeGoingOff(ctx.caster, ctx.spell)) != Mage::NOT_FROZEN)
        {
            float flatMods = 0.0f;
            if (SpellModMgr* mods = ctx.caster.GetSpellMods())
            {
                mods->ApplySpellMod(ctx.spell.ID, SPELLMOD_CRITICAL_CHANCE, flatMods);
            }
            float const factor = Mage::ShatterFactor(ctx.caster.HasAura(SHATTER_RANK_ONE), ctx.caster.HasAura(SHATTER_RANK_TWO));
            ctx.critChance = Mage::ShatterCritChance(ctx.critChance, factor, flatMods);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// Every spell of FROSTBURN_SPELLS, when it counts its target as frozen: Frostburn's mastery amount raises the
    /// damage done; Frostbolt also takes Shatter's effect-1 percent for the rank the caster knows, and Ice Lance its
    /// own effect-1 percent (100: double damage) and ICE_LANCE_FINGERS_PERCENT more while it uses a Fingers of Frost
    /// charge. Deep Freeze's damage (71757), cast only at a target no stun holds, takes Frostburn at any target,
    /// whenever it is cast. A periodic effect's amount counts the target by its own state alone. Ice Lance and
    /// Frostfire Bolt travel to their target, and their hit is reckoned at the launch, before FrostLaunch takes the
    /// charge and Brain Freeze's buff.
    SpellHandlerOutcome<void> FrostDamageDone(DamageDoneContext& ctx)
    {
        uint32 const castTime = CastTimeGoingOff(ctx.caster, ctx.spell);
        if (ctx.spell.ID != DEEP_FREEZE_DAMAGE &&
            Mage::CountsAsFrozen(ctx.caster, ctx.victim, ctx.spell, ctx.damageType, castTime) == Mage::NOT_FROZEN)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        if (SpellAuraHolder* holder = ctx.caster.GetSpellAuraHolder(FROSTBURN))
        {
            ctx.doneTotalMod *= (100.0f + AuraHandle(*holder).Amount(EFFECT_INDEX_0)) / 100.0f;
        }
        if (ctx.spell.ID == FROSTBOLT)
        {
            uint32 const shatter = ctx.caster.HasAura(SHATTER_RANK_TWO) ? SHATTER_RANK_TWO :
                                   ctx.caster.HasAura(SHATTER_RANK_ONE) ? SHATTER_RANK_ONE : 0;
            if (shatter)
            {
                ctx.doneTotalMod *= (100.0f + EffectValue(shatter, EFFECT_INDEX_1)) / 100.0f;
            }
        }
        else if (ctx.spell.ID == ICE_LANCE_SPELL)
        {
            ctx.doneTotalMod *= Mage::IceLanceMultiplier(true, Mage::UsesFingers(ctx.caster, ctx.spell, castTime),
                                                         EffectValue(ICE_LANCE_SPELL, EFFECT_INDEX_1),
                                                         ICE_LANCE_FINGERS_PERCENT);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 30455, 44572, 44614, 133: as the cast goes off, before its cast-end proc, takes the Fingers of Frost charge it
    /// uses (UsesFingers), then takes Brain Freeze's buff off the caster when the buff made the cast instant
    /// (UsesBrainFreeze), whatever then becomes of the cast: one that misses, meets an immunity or finds its target
    /// dead takes both all the same.
    SpellHandlerOutcome<void> FrostLaunch(LaunchContext& ctx)
    {
        if (Mage::UsesFingers(ctx.caster, ctx.spell, ctx.castTime))
        {
            Mage::UseFingersCharge(ctx.caster);
        }
        if (Mage::UsesBrainFreeze(ctx.caster, ctx.spell, ctx.castTime))
        {
            ctx.caster.RemoveAurasDueToSpell(BRAIN_FREEZE_BUFF);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 44543, 44545, 83074 (Fingers of Frost) and 44546, 44548, 44549 (Brain Freeze): each talent refuses the procs of
    /// the sources RefusesChillSource names.
    SpellHandlerOutcome<bool> ChillSourceCheckProc(CheckProcContext& ctx)
    {
        if (ctx.procSpell && Mage::RefusesChillSource(ctx.procSpell->ID))
        {
            return SpellHandlerOutcome<bool>::Return(false);
        }
        return SpellHandlerOutcome<bool>::Continue();
    }

    /// 71761: Deep Freeze Immunity State, which every mage holds: its proc casts its trigger spell (71757, Deep Freeze's
    /// damage) only at a creature whose template is immune to stuns, through Mage::CastProcTriggerSpell.
    ProcOutcome DeepFreezeImmunityProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        Creature* victimCreature = ctx.victim ? ctx.victim->ToCreature() : NULL;
        if (!victimCreature || (victimCreature->GetCreatureInfo()->MechanicImmuneMask & (1 << (MECHANIC_STUN - 1))) == 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        return ProcOutcome::Return(Mage::CastProcTriggerSpell(ctx, Mage::EffectTriggerSpell(ctx), NULL));
    }

    /// 44543, 44545, 83074: Fingers of Frost: its proc rolls the aura's amount as a percent chance, and on a pass casts
    /// its trigger spell (44544) through Mage::CastProcTriggerSpell.
    ProcOutcome FingersOfFrostProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        if (!roll_chance_i(ctx.triggerAmount))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        return ProcOutcome::Return(Mage::CastProcTriggerSpell(ctx, Mage::EffectTriggerSpell(ctx), NULL));
    }

    /// 116: as a Frostbolt cast begins, the Early Frost lockout of the rank its caster knows (Mage::EarlyFrostLockout)
    /// is cast on him; while it lasts, its modifiers cancel the talent's on his next Frostbolts' cast time and global
    /// cooldown. This cast's own are already set, and a cast that is interrupted keeps the lockout it started.
    SpellHandlerOutcome<void> FrostboltCastStart(CastStartContext& ctx)
    {
        uint32 const lockout = Mage::EarlyFrostLockout(ctx.caster.HasAura(EARLY_FROST_RANK_ONE),
                                                       ctx.caster.HasAura(EARLY_FROST_RANK_TWO),
                                                       ctx.caster.HasAura(EARLY_FROST_LOCKOUT_RANK_ONE) ||
                                                       ctx.caster.HasAura(EARLY_FROST_LOCKOUT_RANK_TWO));
        if (lockout)
        {
            ctx.CastTriggered(ctx.caster, lockout);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 116: Frostbolt: after a hit that was a critical strike, for a caster who knows Piercing Chill (83157, or 83156):
    /// of the enemies within the chill's radius (effect 0 of 83154) of the target, the talent's count nearest to the
    /// target, the target never among them (Mage::NearestOthers), each get the chill (83154), cast at each one through
    /// the talent's aura. Nothing for a heal or a hit that was no crit.
    SpellHandlerOutcome<void> FrostboltAfterHit(AfterHitContext& ctx)
    {
        if (ctx.heal || !ctx.crit)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        uint32 const talent = ctx.caster.HasAura(PIERCING_CHILL_RANK_TWO) ? PIERCING_CHILL_RANK_TWO :
                              ctx.caster.HasAura(PIERCING_CHILL_RANK_ONE) ? PIERCING_CHILL_RANK_ONE : 0;
        int32 const count = Mage::PiercingChillCount(talent);
        if (count <= 0)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        std::list<Unit*> others;
        ctx.EnemiesAround(ctx.target, EffectRadius(PIERCING_CHILL_CHILL, EFFECT_INDEX_0), others);
        Mage::NearestOthers(others, &ctx.target, ctx.target, count);
        for (Unit* other : others)
        {
            ctx.CastTriggeredByAura(*other, PIERCING_CHILL_CHILL, talent, EFFECT_INDEX_0);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 83154: Piercing Chill: of the units the engine found for an effect, keeps the cast's explicit target alone,
    /// once, and makes the list final; empties it for a cast with no unit target or one the engine did not find.
    SpellHandlerOutcome<void> PiercingChillTargets(SelectTargetsContext& ctx)
    {
        bool const found = ctx.castTarget &&
                           std::find(ctx.targets.begin(), ctx.targets.end(), ctx.castTarget) != ctx.targets.end();
        ctx.targets.clear();
        if (found)
        {
            ctx.targets.push_back(ctx.castTarget);
        }
        return SpellHandlerOutcome<void>::Return();
    }

    /// 11175, 12569, 12571: Permafrost: its dummy effect's proc heals the caster's pet through 91394 for the effect's
    /// percent of the hit's damage (Mage::PermafrostHeal), cast at the pet with that heal as its value
    /// (Mage::CastProcSpell). FAILED, with nothing cast, when he has no living pet or the heal is 0.
    ProcOutcome PermafrostProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        Pet* pet = ctx.caster.GetPet();
        if (!pet || !pet->IsAlive())
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const heal = Mage::PermafrostHeal(ctx.damage, ctx.triggerAmount);
        if (heal <= 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const basePoints[MAX_EFFECT_INDEX] = { heal, 0, 0 };
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, *pet, PERMAFROST_PET_HEAL, basePoints));
    }

    /// 120: Cone of Cold: after its damage, for a caster who knows Improved Cone of Cold (12489, or 11190), the living
    /// unit it hit casts the rank's freeze (Mage::ImprovedConeOfColdFreeze) on itself, for the caster. A unit immune to
    /// the freeze as a spell of the caster's is not frozen: the freeze shows Immune at it instead. Nothing for a heal.
    SpellHandlerOutcome<void> ConeOfColdAfterHit(AfterHitContext& ctx)
    {
        if (ctx.heal || !ctx.target.IsAlive())
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        uint32 const talent = ctx.caster.HasAura(IMPROVED_CONE_OF_COLD_RANK_TWO) ? IMPROVED_CONE_OF_COLD_RANK_TWO :
                              ctx.caster.HasAura(IMPROVED_CONE_OF_COLD_RANK_ONE) ? IMPROVED_CONE_OF_COLD_RANK_ONE : 0;
        uint32 const freeze = Mage::ImprovedConeOfColdFreeze(talent);
        SpellEntry const* freezeSpell = freeze ? sSpellStore.LookupEntry(freeze) : NULL;
        if (!freezeSpell)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        if (ctx.target.IsImmuneToSpell(freezeSpell, false))
        {
            ctx.ShowImmune(ctx.target, freeze);
            return SpellHandlerOutcome<void>::Continue();
        }
        ctx.CastTriggeredFrom(ctx.target, ctx.target, freeze);
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 45438: Ice Block: as the cast goes off, a caster who holds Glyph of Ice Block (56372) has the cooldowns of
    /// Frost Nova's category ended, and his client told. Nothing for a caster without the glyph.
    SpellHandlerOutcome<void> IceBlockLaunch(LaunchContext& ctx)
    {
        if (ctx.caster.HasAura(GLYPH_OF_ICE_BLOCK))
        {
            ctx.caster.RemoveSpellCategoryCooldown(FROST_NOVA_CATEGORY, true);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const DEEP_FREEZE_IMMUNITY[] = { 71761 };
    uint32 const FINGERS_OF_FROST[] = { 44543, 44545, 83074 };
    uint32 const BRAIN_FREEZE[] = { 44546, 44548, 44549 };
    uint32 const FROST_LAUNCHES[] = { 30455, 44572, 44614, 133 };
    uint32 const FROSTBOLT_CAST_START[] = { FROSTBOLT };
    uint32 const FROSTBOLT_HIT[] = { 116 };
    uint32 const PIERCING_CHILL_CHILLS[] = { 83154 };
    uint32 const PERMAFROST[] = { 11175, 12569, 12571 };
    uint32 const CONE_OF_COLD_HIT[] = { 120 };
    uint32 const ICE_BLOCK[] = { 45438 };
}

uint32 Mage::RegisterFrost(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<DamageDoneHook>(registry, FROSTBURN_SPELLS, &FrostDamageDone);
    rows += RegisterRanks<CritChanceHook>(registry, SHATTER_SPELLS, &FrostCritChance);
    rows += RegisterRanks<OnProcHook>(registry, DEEP_FREEZE_IMMUNITY, &DeepFreezeImmunityProc);
    rows += RegisterRanks<OnProcHook>(registry, FINGERS_OF_FROST, &FingersOfFrostProc);
    rows += RegisterRanks<CheckProcHook>(registry, FINGERS_OF_FROST, &ChillSourceCheckProc);
    rows += RegisterRanks<CheckProcHook>(registry, BRAIN_FREEZE, &ChillSourceCheckProc);
    rows += RegisterRanks<LaunchHook>(registry, FROST_LAUNCHES, &FrostLaunch);
    rows += RegisterRanks<CastStartHook>(registry, FROSTBOLT_CAST_START, &FrostboltCastStart);
    rows += RegisterRanks<AfterHitHook>(registry, FROSTBOLT_HIT, &FrostboltAfterHit);
    rows += RegisterRanks<SelectTargetsHook>(registry, PIERCING_CHILL_CHILLS, &PiercingChillTargets);
    rows += RegisterRanks<OnProcHook>(registry, PERMAFROST, &PermafrostProc);
    rows += RegisterRanks<AfterHitHook>(registry, CONE_OF_COLD_HIT, &ConeOfColdAfterHit);
    rows += RegisterRanks<LaunchHook>(registry, ICE_BLOCK, &IceBlockLaunch);
    return rows;
}

Mage::FrozenKind Mage::CountsAsFrozen(Unit const& caster, Unit const& target, SpellEntry const& spell,
                                      DamageEffectType damageType, uint32 castTime)
{
    if (target.IsFrozen())
    {
        return REALLY_FROZEN;
    }
    if (damageType == DOT)
    {
        return NOT_FROZEN;
    }
    if (UsesFingers(caster, spell, castTime))
    {
        return FINGERS;
    }
    return NOT_FROZEN;
}

bool Mage::UsesFingers(Unit const& caster, SpellEntry const& spell, uint32 castTime)
{
    if (!caster.HasAura(FINGERS_OF_FROST_BUFF))
    {
        return false;
    }
    return spell.ID == ICE_LANCE_SPELL || spell.ID == DEEP_FREEZE ||
           (spell.ID == FROSTFIRE_BOLT && UsesBrainFreeze(caster, spell, castTime));
}

bool Mage::UsesBrainFreeze(Unit const& caster, SpellEntry const& spell, uint32 castTime)
{
    return castTime == 0 && (spell.ID == FIREBALL || spell.ID == FROSTFIRE_BOLT) && caster.HasAura(BRAIN_FREEZE_BUFF);
}

bool Mage::RefusesChillSource(uint32 procSpellId)
{
    return procSpellId == FROST_ARMOR_CHILL || procSpellId == FROSTFIRE_ORB_RANK_ONE_HIT;
}

bool Mage::UseFingersCharge(Unit& mage)
{
    if (!mage.HasAura(FINGERS_OF_FROST_BUFF))
    {
        return false;
    }
    mage.RemoveAuraHolderFromStack(FINGERS_OF_FROST_BUFF);
    return true;
}

float Mage::IceLanceMultiplier(bool countsAsFrozen, bool fingersUp, int32 doubledPercent, int32 fingersPercent)
{
    float const frozen = countsAsFrozen ? (100.0f + doubledPercent) / 100.0f : 1.0f;
    return fingersUp ? frozen * (100.0f + fingersPercent) / 100.0f : frozen;
}

float Mage::ShatterFactor(bool knowsRankOne, bool knowsRankTwo)
{
    return knowsRankTwo ? 3.0f : knowsRankOne ? 2.0f : 1.0f;
}

float Mage::ShatterCritChance(float chance, float factor, float flatMods)
{
    return (chance + flatMods) * factor - flatMods;
}

bool Mage::ShatterCovers(uint32 spellId)
{
    return std::find(std::begin(SHATTER_SPELLS), std::end(SHATTER_SPELLS), spellId) != std::end(SHATTER_SPELLS);
}

bool Mage::FrostburnCovers(uint32 spellId)
{
    return std::find(std::begin(FROSTBURN_SPELLS), std::end(FROSTBURN_SPELLS), spellId) != std::end(FROSTBURN_SPELLS);
}

uint32 Mage::EarlyFrostLockout(bool knowsRankOne, bool knowsRankTwo, bool lockoutUp)
{
    if (lockoutUp)
    {
        return 0;
    }
    return knowsRankTwo ? EARLY_FROST_LOCKOUT_RANK_TWO : knowsRankOne ? EARLY_FROST_LOCKOUT_RANK_ONE : 0;
}

int32 Mage::PiercingChillCount(uint32 talentSpell)
{
    if (talentSpell != PIERCING_CHILL_RANK_ONE && talentSpell != PIERCING_CHILL_RANK_TWO)
    {
        return 0;
    }
    return EffectValue(talentSpell, EFFECT_INDEX_0);
}

void Mage::NearestOthers(std::list<Unit*>& targets, Unit const* excluded, WorldObject const& from, int32 count)
{
    std::vector<std::pair<float, Unit*> > byDistance;
    for (Unit* unit : targets)
    {
        if (!unit || unit == excluded)
        {
            continue;
        }
        bool listed = false;
        for (std::pair<float, Unit*> const& kept : byDistance)
        {
            listed = listed || kept.second == unit;
        }
        if (!listed)
        {
            byDistance.push_back(std::make_pair(from.Where().DistanceTo(unit->Where()), unit));
        }
    }
    std::stable_sort(byDistance.begin(), byDistance.end(),
                     [](std::pair<float, Unit*> const& a, std::pair<float, Unit*> const& b)
                     {
                         return a.first < b.first;
                     });
    targets.clear();
    for (std::size_t i = 0; count > 0 && i < byDistance.size() && i < std::size_t(count); ++i)
    {
        targets.push_back(byDistance[i].second);
    }
}

int32 Mage::PermafrostHeal(uint32 damage, int32 percent)
{
    if (percent <= 0)
    {
        return 0;
    }
    int64 const heal = int64(damage) * percent / 100;
    return heal > std::numeric_limits<int32>::max() ? std::numeric_limits<int32>::max() : int32(heal);
}

uint32 Mage::ImprovedConeOfColdFreeze(uint32 talentSpell)
{
    switch (talentSpell)
    {
        case IMPROVED_CONE_OF_COLD_RANK_ONE:
            return IMPROVED_CONE_OF_COLD_FREEZE_ONE;
        case IMPROVED_CONE_OF_COLD_RANK_TWO:
            return IMPROVED_CONE_OF_COLD_FREEZE_TWO;
        default:
            return 0;
    }
}
